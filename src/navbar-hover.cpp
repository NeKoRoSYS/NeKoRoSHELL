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
#include <fcntl.h>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

struct Monitor { int x, y, w, h; };
struct Config {
    int activate_size = 10;
    int deactivate_size = 40;
    std::string bar_position = "top";
};

std::string exec(const char* cmd, int timeout_ms = 1000) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return "";

    pid_t pid = fork();
    if (pid == -1) return "";

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        exit(1);
    }

    close(pipefd[1]);

    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    std::string result = "";
    char buffer[4096];
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        ssize_t count = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';
            result += buffer;
        } else if (count == 0) {
            break; 
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                if (elapsed > timeout_ms) {
                    kill(pid, SIGKILL); 
                    std::cerr << "[WARNING] Command timed out: " << cmd << "\n";
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } else {
                break;
            }
        }
    }

    close(pipefd[0]);
    waitpid(pid, nullptr, 0); 
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
        if (mon_out.empty()) return monitors;

        try {
            auto j = json::parse(mon_out);
            for (const auto& m : j) {
                monitors.push_back({
                    m["x"].get<int>(),
                    m["y"].get<int>(),
                    m["width"].get<int>(),
                    m["height"].get<int>()
                });
            }
        } catch (const json::exception& e) {
            std::cerr << "[ERROR] JSON parse failed: " << e.what() << "\n";
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
        x = -1;
        y = -1;
        return false; 
    }

    bool is_layer_active(const std::string& layer_name) override {
        if (layer_name == "swaync-control-center") {
            std::string state = exec("timeout 0.5 swaync-client -s 2>/dev/null");
            return state.find("\"visible\": true") != std::string::npos || 
                   state.find("\"visible\":true") != std::string::npos;
        }
        
        std::string cmd = "lswt -j 2>/dev/null | jq -e '.[] | select(.namespace == \"" + layer_name + "\")'";
        std::string out = exec(cmd.c_str());
        return !out.empty();
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
        }
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

    pid_t old_waybar_pid;
    while ((old_waybar_pid = get_waybar_pid()) > 0) {
        kill(old_waybar_pid, SIGTERM);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
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
    current_waybar_pid = pid;

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
