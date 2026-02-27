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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    virtual bool is_layer_active(const std::string& layer_name) = 0;
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

            std::string mon_out = exec("hyprctl -j monitors");
            if (!mon_out.empty() && mon_out.front() == '[') {
                auto monitors = json::parse(mon_out);
                for (const auto& mon : monitors) {
                    active_workspaces.insert(mon["activeWorkspace"]["name"].get<std::string>());
                }
            }

            std::string cli_out = exec("hyprctl -j clients");
            if (!cli_out.empty() && cli_out.front() == '[') {
                auto clients = json::parse(cli_out);
                for (const auto& client : clients) {
                    std::string addr = client["address"].get<std::string>();
                    std::string ws = client["workspace"]["name"].get<std::string>();
                    window_to_workspace[addr] = ws;
                    workspace_window_count[ws]++;
                }
            }
        } catch (...) {}
    }

public:
    HyprlandBackend() {
        sync_state_from_json();
    }

    bool is_layer_active(const std::string& layer_name) override {
        std::string layers = exec("hyprctl layers");
        return (layers.find(layer_name) != std::string::npos);
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
        if (!runtime_dir || !signature) return;

        std::string socket_path = std::string(runtime_dir) + "/hypr/" + std::string(signature) + "/.socket2.sock";
        
        struct sockaddr_un addr;
        int sfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
                            if (window_to_workspace.count(addr)) workspace_window_count[window_to_workspace[addr]]--;
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
                
                if (state_changed) on_event(); 
            } else if (num_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else break;
            } else break;
        }
        close(sfd);
    }
};

class SwayBackend : public CompositorBackend {
private:
    int get_socket() {
        const char* sock_path = getenv("SWAYSOCK");
        if (!sock_path) return -1;
        int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sfd == -1) return -1;
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
        if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
            close(sfd);
            return -1;
        }
        return sfd;
    }

    std::string sway_ipc_request(int fd, uint32_t type, const std::string& payload = "") {
        struct { char magic[6]; uint32_t len; uint32_t type; } __attribute__((packed)) header;
        memcpy(header.magic, "i3-ipc", 6);
        header.len = payload.length();
        header.type = type;
        
        if (write(fd, &header, sizeof(header)) == -1) return "";
        if (!payload.empty() && write(fd, payload.c_str(), payload.length()) == -1) return "";
        
        if (read(fd, &header, sizeof(header)) != sizeof(header)) return "";
        std::string response(header.len, '\0');
        size_t total_read = 0;
        while (total_read < header.len) {
            ssize_t bytes = read(fd, &response[total_read], header.len - total_read);
            if (bytes <= 0) break;
            total_read += bytes;
        }
        return response;
    }

    bool has_windows_recursive(const json& j) {
        if (j.contains("type") && j["type"] == "workspace" && j.value("focused", false)) {
            size_t count = 0;
            if (j.contains("nodes")) count += j["nodes"].size();
            if (j.contains("floating_nodes")) count += j["floating_nodes"].size();
            return count > 0;
        }
        if (j.contains("nodes")) {
            for (const auto& node : j["nodes"]) {
                if (has_windows_recursive(node)) return true;
            }
        }
        return false;
    }

public:
    bool is_layer_active(const std::string& layer_name) override {
        std::string cmd = "lswt -j 2>/dev/null | jq -e '.[] | select(.namespace == \"" + layer_name + "\")' >/dev/null 2>&1";
        return system(cmd.c_str()) == 0;
    }

    bool has_active_windows() override {
        int fd = get_socket();
        if (fd == -1) return false;
        
        std::string tree_json = sway_ipc_request(fd, 4); 
        close(fd);
        
        if (tree_json.empty()) return false;
        try {
            auto j = json::parse(tree_json);
            return has_windows_recursive(j);
        } catch (...) { return false; }
    }

    void listen_for_events(std::function<void()> on_event) override {
        int fd = get_socket();
        if (fd == -1) return;
        
        sway_ipc_request(fd, 2, "[\"window\", \"workspace\"]");
        
        struct { char magic[6]; uint32_t len; uint32_t type; } __attribute__((packed)) header;
        std::vector<char> buffer(65536);
        
        while (true) {
            if (read(fd, &header, sizeof(header)) != sizeof(header)) break;
            
            size_t total_read = 0;
            while (total_read < header.len) {
                ssize_t bytes = read(fd, buffer.data(), std::min(buffer.size(), (size_t)(header.len - total_read)));
                if (bytes <= 0) break;
                total_read += bytes;
            }
            
            if ((header.type & 0x80000000) != 0) {
                on_event();
            }
        }
        close(fd);
    }
};

class MangoBackend : public CompositorBackend {
public:
    bool is_layer_active(const std::string& layer_name) override {
        FILE* pipe = popen("mmsg -g -e 2>/dev/null", "r");
        if (!pipe) return false;
        char buffer[128];
        bool active = false;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            if (line.find("last_layer " + layer_name) != std::string::npos) {
                active = true;
                break;
            }
        }
        pclose(pipe);
        return active;
    }

    bool has_active_windows() override {
        FILE* pipe = popen("mmsg -g -t 2>/dev/null", "r");
        if (!pipe) return false;
        char buffer[128];
        bool has_windows = false;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            if (line.find("clients ") != std::string::npos) {
                int clients = std::stoi(line.substr(line.find("clients ") + 8));
                if (clients > 0) has_windows = true;
            }
        }
        pclose(pipe);
        return has_windows;
    }

    void listen_for_events(std::function<void()> on_event) override {
        FILE* pipe = popen("mmsg -w -t -c 2>/dev/null", "r");
        if (!pipe) return;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            on_event();
        }
        pclose(pipe);
    }
};

bool is_waybar_visible = false;
pid_t current_waybar_pid = -1;

void set_waybar(bool visible) {
    if (current_waybar_pid <= 0 || kill(current_waybar_pid, 0) != 0) {
        current_waybar_pid = get_waybar_pid();
    }

    bool process_running = (current_waybar_pid > 0);
    if (!process_running) is_waybar_visible = false;

    if (visible) {
        if (!process_running) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = {(char*)"waybar", nullptr};
                execvp(args[0], args);
                exit(1); 
            }
            current_waybar_pid = pid;
            is_waybar_visible = true;
        } else if (!is_waybar_visible) {
            kill(current_waybar_pid, SIGUSR1);
            is_waybar_visible = true;
        }
    } else {
        if (process_running && is_waybar_visible) {
            kill(current_waybar_pid, SIGUSR1);
            is_waybar_visible = false;
        }
    }
}

int main() {
    std::string wm = getenv("XDG_CURRENT_DESKTOP") ? getenv("XDG_CURRENT_DESKTOP") : "";
    
    CompositorBackend* backend = nullptr;
    
    if (wm.find("Hyprland") != std::string::npos) {
        backend = new HyprlandBackend();
    } else if (wm.find("Sway") != std::string::npos) {
        backend = new SwayBackend();
    } else if (wm.find("Mango") != std::string::npos || wm.find("dwl") != std::string::npos) {
        backend = new MangoBackend();
    } else {
        std::cerr << "Unsupported Window Manager." << std::endl;
        return 1;
    }

    current_waybar_pid = get_waybar_pid();
    if (current_waybar_pid > 0) {
        is_waybar_visible = backend->is_layer_active("waybar");
    } else {
        is_waybar_visible = true; 
    }

    set_waybar(backend->has_active_windows());

    backend->listen_for_events([&backend]() {
        bool has_windows = backend->has_active_windows();
        set_waybar(has_windows);
    });

    return 0;
}
