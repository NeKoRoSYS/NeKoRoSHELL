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
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

pid_t get_waybar_pid() {
    std::string pid_str = exec("pgrep -x waybar");
    if (!pid_str.empty()) {
        try { return std::stoi(pid_str); } catch(...) {}
    }
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
    int parse_int(const std::string& json, size_t pos) {
        while (pos < json.length() && !isdigit(json[pos]) && json[pos] != '-') pos++;
        if (pos >= json.length()) return -999;
        return std::stoi(json.substr(pos));
    }

public:
    std::vector<Monitor> get_monitors() override {
        std::vector<Monitor> monitors;
        std::string mon_out = exec("hyprctl -j monitors");
        size_t pos = 0;
        while ((pos = mon_out.find("\"width\":", pos)) != std::string::npos) {
            int w = parse_int(mon_out, pos + 8);
            pos = mon_out.find("\"height\":", pos);
            int h = parse_int(mon_out, pos + 9);
            pos = mon_out.find("\"x\":", pos);
            int x = parse_int(mon_out, pos + 4);
            pos = mon_out.find("\"y\":", pos);
            int y = parse_int(mon_out, pos + 4);
            if (x != -999 && y != -999 && w != -999 && h != -999) monitors.push_back({x, y, w, h});
            pos++;
        }
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
    int parse_int(const std::string& json, size_t pos) {
        while (pos < json.length() && !isdigit(json[pos]) && json[pos] != '-') pos++;
        if (pos >= json.length()) return -999;
        return std::stoi(json.substr(pos));
    }

public:
    std::vector<Monitor> get_monitors() override {
        std::vector<Monitor> monitors;
        std::string mon_out = exec("swaymsg -t get_outputs");
        size_t pos = 0;
        
        while ((pos = mon_out.find("\"rect\":", pos)) != std::string::npos) {
            int x = parse_int(mon_out, mon_out.find("\"x\":", pos) + 4);
            int y = parse_int(mon_out, mon_out.find("\"y\":", pos) + 4);
            int w = parse_int(mon_out, mon_out.find("\"width\":", pos) + 8);
            int h = parse_int(mon_out, mon_out.find("\"height\":", pos) + 9);
            
            if (x != -999 && y != -999 && w != -999 && h != -999) {
                monitors.push_back({x, y, w, h});
            }
            pos += 7;
        }
        return monitors;
    }

    bool get_cursor_pos(int& x, int& y) override {
        x = -1;
        y = -1;
        return false; 
    }

    bool is_layer_active(const std::string& layer_name) override {
        std::string layers = exec("lswt 2>/dev/null");
        return (layers.find(layer_name) != std::string::npos);
    }
};

std::unique_ptr<CompositorBackend> backend;
std::atomic<bool> is_swaync_open_flag{false};
bool is_bar_visible = true;

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
        system("killall -q -SIGUSR1 waybar");
        is_bar_visible = want_visible;
    }
}

Config read_config() {
    Config cfg;
    const char* home_env = getenv("HOME");
    std::string home = home_env ? home_env : "";
    std::ifstream file(home + "/.cache/navbar-hover.conf");
    
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (line.find("ACTIVATE_SIZE=") == 0) {
                try { cfg.activate_size = std::stoi(line.substr(14)); } catch (...) {}
            }
            if (line.find("DEACTIVATE_SIZE=") == 0) {
                try { cfg.deactivate_size = std::stoi(line.substr(16)); } catch (...) {}
            }
            if (line.find("BAR_POSITION=") == 0) {
                cfg.bar_position = line.substr(13);
                cfg.bar_position.erase(
                    std::remove(cfg.bar_position.begin(), cfg.bar_position.end(), '\"'),
                    cfg.bar_position.end()
                );
            }
        }
    }
    return cfg;
}

int main() {
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        backend = std::make_unique<HyprlandBackend>();
    } else if (getenv("SWAYSOCK")) {
        backend = std::make_unique<SwayBackend>();
    } else {
        std::cerr << "[ERROR] Unsupported or undetectable compositor.\n";
        return 1;
    }

    while (backend->is_layer_active("swaync-control-center")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    std::thread(swaync_watcher_thread).detach();

    Config cfg = read_config();
    std::vector<Monitor> monitors = backend->get_monitors();
    int cycle_count = 0;

    system("killall -q waybar");
    
    for (int i = 0; i < 40; ++i) { 
        if (get_waybar_pid() <= 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    pid_t pid = fork();
    if (pid == 0) {
        char* args[] = {(char*)"waybar", nullptr};
        execvp(args[0], args);
        exit(1);
    }

    for (int i = 0; i < 40; ++i) { 
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (backend->is_layer_active("waybar")) break;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    is_bar_visible = true;
    pid_t current_waybar_pid = pid;

    while (true) {
        if (current_waybar_pid > 0 && kill(current_waybar_pid, 0) != 0) {
            current_waybar_pid = -1;
        }
        if (current_waybar_pid <= 0) {
            pid_t new_pid = get_waybar_pid();
            if (new_pid > 0) {
                current_waybar_pid = new_pid;
                is_bar_visible = true; 
            }
        }

        if (is_swaync_open_flag.load()) {
            if (is_bar_visible) {
                toggle_waybar(false);
            }
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
                if (cfg.bar_position == "top") {
                    match = (cx >= m.x && cx < m.x + m.w && cy >= m.y && cy <= m.y + thresh);
                } else if (cfg.bar_position == "bottom") {
                    match = (cx >= m.x && cx < m.x + m.w && cy >= m.y + m.h - thresh && cy <= m.y + m.h);
                } else if (cfg.bar_position == "left") {
                    match = (cx >= m.x && cx <= m.x + thresh && cy >= m.y && cy < m.y + m.h);
                } else if (cfg.bar_position == "right") {
                    match = (cx >= m.x + m.w - thresh && cx <= m.x + m.w && cy >= m.y && cy < m.y + m.h);
                }
                
                if (match) {
                    is_hovering = true;
                    break;
                }
            }

            if (is_hovering && !is_bar_visible) {
                toggle_waybar(true);
            } else if (!is_hovering && is_bar_visible) {
                toggle_waybar(false);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
