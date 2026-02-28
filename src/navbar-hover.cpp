#include <iostream> 
#include <fstream> 
#include <string> 
#include <vector> 
#include <memory> 
#include <thread> 
#include <chrono> 
#include <cstdlib> 
#include <algorithm> 
#include <unistd.h> 
#include <sys/wait.h> 
#include <atomic> 
#include <sstream> 
#include <dirent.h>
#include <nlohmann/json.hpp>

void log_error(const std::string& msg) {
    std::ofstream log_file("/tmp/nekoroshell-navbar.log", std::ios_base::app);
    if (log_file.is_open()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string time_str = std::ctime(&now);
        time_str.pop_back();
        log_file << "[" << time_str << "] ERROR: " << msg << "\n";
    }
}

using json = nlohmann::json;

struct Monitor { int x, y, w, h; }; 
struct Config { 
    int activate_size = 10; 
    int deactivate_size = 40; 
    std::string bar_position = "top"; 
}; 

struct PipeDeleter { 
    void operator()(FILE* stream) const { if (stream) pclose(stream); } 
}; 

std::string exec(const char* cmd) { 
    char buffer[4096]; 
    std::string result = ""; 
    std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd, "r")); 
    if (!pipe) {
        log_error("popen() failed to execute command: " + std::string(cmd));
        return "";
    }
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) { 
        result += buffer; 
    } 
    return result; 
} 

pid_t get_waybar_pid() { 
    DIR* dir = opendir("/proc");
    if (!dir) return -1;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isdigit(ent->d_name[0])) continue;
        std::string pid_str = ent->d_name;
        std::ifstream comm_file("/proc/" + pid_str + "/comm");
        if (comm_file.is_open()) {
            std::string comm_name;
            std::getline(comm_file, comm_name);
            if (comm_name == "waybar") {
                closedir(dir);
                return std::stoi(pid_str);
            }
        }
    }
    closedir(dir);
    return -1;
} 

class CompositorBackend { 
public: 
    virtual ~CompositorBackend() = default; 
    virtual std::vector<Monitor> get_monitors() = 0; 
    virtual bool get_cursor_pos(int& x, int& y) = 0; 
    virtual bool is_layer_active(const std::string& layer_name) = 0; 
}; 

class HyprlandBackend : public CompositorBackend { 
public: 
    std::vector<Monitor> get_monitors() override { 
        std::vector<Monitor> monitors; 
        std::string mon_out = exec("hyprctl -j monitors"); 
        if (mon_out.empty() || mon_out.front() != '[') return monitors;
        try {
            auto j = json::parse(mon_out);
            for (const auto& m : j) {
                monitors.push_back({m["x"], m["y"], m["width"], m["height"]});
            }
        } catch (...) {}
        return monitors; 
    } 

    bool get_cursor_pos(int& x, int& y) override { 
        std::string pos_out = exec("hyprctl cursorpos"); 
        return (sscanf(pos_out.c_str(), "%d, %d", &x, &y) == 2); 
    } 

    bool is_layer_active(const std::string& layer_name) override { 
        std::string layers = exec("hyprctl layers"); 
        return (layers.find(layer_name) != std::string::npos); 
    } 
}; 

class SwayBackend : public CompositorBackend { 
public: 
    std::vector<Monitor> get_monitors() override { 
        std::vector<Monitor> monitors; 
        std::string cmd = "swaymsg -t get_outputs -r | jq -r '.[] | \"\\(.rect.x) \\(.rect.y) \\(.rect.width) \\(.rect.height)\"'"; 
        std::string out = exec(cmd.c_str()); 
         
        std::stringstream ss(out); 
        int x, y, w, h; 
        while (ss >> x >> y >> w >> h) { 
            monitors.push_back({x, y, w, h}); 
        } 
        return monitors; 
    } 

    bool get_cursor_pos(int& x, int& y) override { 
        x = -1; y = -1; 
        return false;  
    } 

    bool is_layer_active(const std::string& layer_name) override { 
        if (layer_name == "swaync-control-center") { 
            std::string state = exec("timeout 0.5 swaync-client -s 2>/dev/null"); 
            return state.find("\"visible\": true") != std::string::npos ||  
                   state.find("\"visible\":true") != std::string::npos; 
        } 
        std::string cmd = "lswt -j 2>/dev/null | jq -e '.[] | select(.namespace == \"" + layer_name + "\")' >/dev/null 2>&1"; 
        return system(cmd.c_str()) == 0; 
    } 
}; 

std::unique_ptr<CompositorBackend> backend; 
std::atomic<bool> is_swaync_open_flag{false}; 
bool is_bar_visible = true; 
pid_t current_waybar_pid = -1;

void swaync_watcher_thread() { 
    while (true) { 
        if (backend) { 
            bool is_open = backend->is_layer_active("swaync-control-center"); 
            is_swaync_open_flag.store(is_open); 
        } 
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  
    } 
} 

void toggle_waybar(bool want_visible) { 
    if (want_visible != is_bar_visible) { 
        if (current_waybar_pid > 0 && kill(current_waybar_pid, 0) == 0) {
            kill(current_waybar_pid, SIGUSR1);
        } else {
            pid_t pid = get_waybar_pid();
            if (pid > 0) {
                current_waybar_pid = pid;
                kill(current_waybar_pid, SIGUSR1);
            }
        }
        is_bar_visible = want_visible; 
    } 
} 

std::optional<Config> read_config() { 
    Config cfg;

    std::string cache_home;
    const char* xdg_env = std::getenv("XDG_CACHE_HOME");
    
    if (xdg_env && *xdg_env != '\0') {
        cache_home = xdg_env;
    } else {
        const char* home_env = std::getenv("HOME");
        if (!home_env) {
            std::cerr << "Error: Neither XDG_CONFIG_HOME nor HOME environment variables are set.\n";
            return std::nullopt;
        }
        cache_home = std::string(home_env) + "/.cache";
    }
    
    std::ifstream file(cache_home + "/nekoroshell/navbar-hover.conf"); 
     
    if (file.is_open()) { 
        std::string line; 
        while (getline(file, line)) { 
            if (line.find("ACTIVATE_SIZE=") == 0) try { cfg.activate_size = std::stoi(line.substr(14)); } catch (...) {} 
            if (line.find("DEACTIVATE_SIZE=") == 0) try { cfg.deactivate_size = std::stoi(line.substr(16)); } catch (...) {} 
            if (line.find("BAR_POSITION=") == 0) { 
                cfg.bar_position = line.substr(13); 
                cfg.bar_position.erase(std::remove(cfg.bar_position.begin(), cfg.bar_position.end(), '\"'), cfg.bar_position.end()); 
            } 
        } 
    } 
    return cfg; 
} 

int main() { 
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) backend = std::make_unique<HyprlandBackend>(); 
    else if (getenv("SWAYSOCK")) backend = std::make_unique<SwayBackend>(); 
    else return 1; 

    while (backend->is_layer_active("swaync-control-center")) { 
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
    } 

    std::thread(swaync_watcher_thread).detach(); 

    Config cfg = read_config(); 
    std::vector<Monitor> monitors = backend->get_monitors(); 
    int cycle_count = 0; 

    int ret = system("killall -q waybar"); 
    (void)ret;
     
    for (int i = 0; i < 40; ++i) {  
        if (get_waybar_pid() <= 0) break; 
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
    } 

    pid_t pid = fork(); 
    if (pid == 0) { 
        char* args[] = {(char*)"waybar", nullptr}; 
        execvp(args[0], args); 
        std::ofstream log_file("/tmp/nekoroshell-navbar.log", std::ios_base::app);
        log_file << "[FATAL] execvp failed to spawn Waybar. Error code: " << errno << "\n";
        exit(1);
    } 

    for (int i = 0; i < 40; ++i) {  
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
        if (backend->is_layer_active("waybar")) break; 
    } 
     
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 

    is_bar_visible = true; 
    current_waybar_pid = pid; 

    while (true) { 
        if (current_waybar_pid > 0 && kill(current_waybar_pid, 0) != 0) current_waybar_pid = -1; 
        if (current_waybar_pid <= 0) { 
            pid_t new_pid = get_waybar_pid(); 
            if (new_pid > 0) { 
                current_waybar_pid = new_pid; 
                is_bar_visible = true;  
            } 
        } 

        if (is_swaync_open_flag.load()) { 
            if (is_bar_visible) toggle_waybar(false); 
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
            continue;  
        } 

        if (++cycle_count >= 100) { 
            cfg = read_config(); 
            monitors = backend->get_monitors(); 
            cycle_count = 0; 
        } 

        int cx = 0, cy = 0; 
        if (backend->get_cursor_pos(cx, cy)) { 
            int thresh = is_bar_visible ? cfg.deactivate_size : cfg.activate_size; 
            bool is_hovering = false; 

            for (const auto& m : monitors) { 
                bool match = false; 
                if (cfg.bar_position == "top") match = (cx >= m.x && cx < m.x + m.w && cy >= m.y && cy <= m.y + thresh); 
                else if (cfg.bar_position == "bottom") match = (cx >= m.x && cx < m.x + m.w && cy >= m.y + m.h - thresh && cy <= m.y + m.h); 
                else if (cfg.bar_position == "left") match = (cx >= m.x && cx <= m.x + thresh && cy >= m.y && cy < m.y + m.h); 
                else if (cfg.bar_position == "right") match = (cx >= m.x + m.w - thresh && cx <= m.x + m.w && cy >= m.y && cy < m.y + m.h); 
                 
                if (match) { is_hovering = true; break; } 
            } 

            if (is_hovering && !is_bar_visible) toggle_waybar(true); 
            else if (!is_hovering && is_bar_visible) toggle_waybar(false); 
        } 

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
    } 

    return 0; 
}
