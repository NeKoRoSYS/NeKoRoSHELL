#include <iostream>
#include <fstream>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <chrono>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <set>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <dirent.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    virtual bool is_waybar_active() = 0;
    virtual bool has_active_windows() = 0;
    virtual void listen_for_events(std::function<void()> on_event) = 0;
};

class HyprlandBackend : public CompositorBackend {
private:
    std::unordered_map<std::string, std::string> window_to_workspace;
    std::unordered_map<std::string, int> workspace_window_count;
    std::unordered_set<std::string> active_workspaces;

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) tokens.push_back(token);
        return tokens;
    }

    void sync_state_from_json() {
        try {
            window_to_workspace.clear();
            workspace_window_count.clear();
            active_workspaces.clear();

            auto monitors = json::parse(exec("hyprctl -j monitors"));
            for (const auto& mon : monitors) {
                active_workspaces.insert(mon["activeWorkspace"]["name"].get<std::string>());
            }

            auto clients = json::parse(exec("hyprctl -j clients"));
            for (const auto& client : clients) {
                std::string addr = client["address"].get<std::string>();
                std::string ws = client["workspace"]["name"].get<std::string>();
                window_to_workspace[addr] = ws;
                workspace_window_count[ws]++;
            }
        } catch (const json::exception& e) {
            std::cerr << "[ERROR] JSON parse failed: " << e.what() << "\n";
        }
    }

public:
    HyprlandBackend() {
        sync_state_from_json();
    }

    bool is_waybar_active() override {
        return get_waybar_pid() != -1;
    }

    bool has_active_windows() override {
        for (const auto& ws : active_workspaces) {
            if (workspace_window_count[ws] > 0) return true;
        }
        return false;
    }

    void listen_for_events(std::function<void()> on_event) override {
        const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
        const char* signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!runtime_dir || !signature) {
            std::cerr << "[ERROR] Hyprland environment variables missing." << std::endl;
            return;
        }

        std::string socket_path = std::string(runtime_dir) + "/hypr/" + std::string(signature) + "/.socket2.sock";
        
        struct sockaddr_un addr;
        int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sfd == -1) return;

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
            close(sfd);
            return;
        }

        char buffer[4096];
        std::string pending_data = "";
        
        while (true) {
            ssize_t num_read = read(sfd, buffer, sizeof(buffer) - 1);
            if (num_read > 0) {
                buffer[num_read] = '\0';
                pending_data += buffer;
                
                size_t pos = 0;
                bool state_changed = false;
                
                while ((pos = pending_data.find('\n')) != std::string::npos) {
                    std::string line = pending_data.substr(0, pos);
                    pending_data.erase(0, pos + 1);
                    
                    if (line.rfind("openwindow>>", 0) == 0) {
                        auto parts = split(line.substr(12), ',');
                        if (parts.size() >= 2) {
                            window_to_workspace[parts[0]] = parts[1];
                            workspace_window_count[parts[1]]++;
                            state_changed = true;
                        }
                    } 
                    else if (line.rfind("closewindow>>", 0) == 0) {
                        std::string addr = line.substr(13);
                        if (window_to_workspace.count(addr)) {
                            workspace_window_count[window_to_workspace[addr]]--;
                            window_to_workspace.erase(addr);
                            state_changed = true;
                        }
                    } 
                    else if (line.rfind("movewindow>>", 0) == 0) {
                        auto parts = split(line.substr(12), ',');
                        if (parts.size() >= 2) {
                            std::string addr = parts[0];
                            std::string new_ws = parts[1];
                            if (window_to_workspace.count(addr)) {
                                workspace_window_count[window_to_workspace[addr]]--;
                            }
                            window_to_workspace[addr] = new_ws;
                            workspace_window_count[new_ws]++;
                            state_changed = true;
                        }
                    }
                    else if (line.rfind("workspace>>", 0) == 0 || line.rfind("focusedmon>>", 0) == 0) {
                        sync_state_from_json();
                        state_changed = true;
                    }
                }
                
                if (state_changed) {
                    on_event(); 
                }
            } else if (num_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    std::cerr << "Socket disconnected." << std::endl;
                    break;
                }
            } else {
                break;
            }
        }
        close(sfd);
    }
};

class SwayBackend : public CompositorBackend {
public:
    bool is_waybar_active() override {
        return get_waybar_pid() != -1;
    }

    bool has_active_windows() override {
        std::string cmd = "swaymsg -t get_tree | jq -r '.. | objects | select(.type==\"workspace\" and .focused==true) | (.nodes | length) + (.floating_nodes | length)'";
        std::string out = exec(cmd.c_str());
        
        try {
            int window_count = std::stoi(out);
            return window_count > 0;
        } catch (...) {
            return false;
        }
    }

    void listen_for_events(std::function<void()> on_event) override {
        FILE* stream = popen("swaymsg -m -t subscribe '[\"window\", \"workspace\"]'", "r");
        if (!stream) {
            std::cerr << "[ERROR] Failed to subscribe to Sway IPC." << std::endl;
            return;
        }

        char buffer[2048];
        while (fgets(buffer, sizeof(buffer), stream) != nullptr) {
            on_event(); 
        }
        
        pclose(stream);
    }
};

bool is_waybar_visible = false;
pid_t waybar_pid = -1;

void set_waybar(bool visible) {
    if (waybar_pid <= 0 || kill(waybar_pid, 0) != 0) {
        waybar_pid = get_waybar_pid();
    }

    bool process_running = (waybar_pid > 0);
    if (!process_running) {
        is_waybar_visible = false;
    }

    if (visible) {
        if (!process_running) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = {(char*)"waybar", nullptr};
                execvp(args[0], args);
                exit(1); 
            }
            waybar_pid = pid; 
            is_waybar_visible = true;
        } else if (!is_waybar_visible) {
            kill(waybar_pid, SIGUSR1);
            is_waybar_visible = true;
        }
    } else {
        if (process_running && is_waybar_visible) {
            kill(waybar_pid, SIGUSR1);
            is_waybar_visible = false;
        }
    }
}

int main() {
    std::unique_ptr<CompositorBackend> backend;

    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        backend = std::make_unique<HyprlandBackend>();
    } else if (getenv("SWAYSOCK")) {
        backend = std::make_unique<SwayBackend>();
    } else {
        std::cerr << "[ERROR] Unsupported or undetectable compositor.\n";
        return 1;
    }

    waybar_pid = get_waybar_pid();
    is_waybar_visible = (waybar_pid != -1);

    set_waybar(backend->has_active_windows());

    backend->listen_for_events([&backend]() {
        bool has_windows = backend->has_active_windows();
        set_waybar(has_windows);
    });

    return 0;
}
