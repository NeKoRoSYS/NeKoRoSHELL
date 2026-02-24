#include <iostream>
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

class CompositorBackend {
public:
    virtual ~CompositorBackend() = default;
    
    virtual bool is_waybar_active() = 0;
    
    virtual bool has_active_windows() = 0;
    
    virtual void listen_for_events(std::function<void()> on_event) = 0;
};

class HyprlandBackend : public CompositorBackend {
private:
    int parse_int_value(const std::string& json, size_t pos) {
        while (pos < json.length() && !isdigit(json[pos]) && json[pos] != '-') pos++;
        if (pos >= json.length()) return -999;
        return std::stoi(json.substr(pos));
    }

public:
    bool is_waybar_active() override {
        std::string layers = exec("hyprctl layers");
        return (layers.find("waybar") != std::string::npos);
    }

    bool has_active_windows() override {
        std::string mon_out = exec("hyprctl -j monitors");
        std::set<int> active_workspace_ids;

        size_t pos = 0;
        while ((pos = mon_out.find("\"activeWorkspace\":", pos)) != std::string::npos) {
            size_t id_key = mon_out.find("\"id\":", pos);
            if (id_key != std::string::npos) {
                int id = parse_int_value(mon_out, id_key + 5);
                if (id != -999) active_workspace_ids.insert(id);
            }
            pos++;
        }

        if (active_workspace_ids.empty()) return false;

        std::string clients_out = exec("hyprctl -j clients");
        pos = 0;
        while ((pos = clients_out.find("\"workspace\":", pos)) != std::string::npos) {
            size_t id_key = clients_out.find("\"id\":", pos);
            if (id_key != std::string::npos) {
                int id = parse_int_value(clients_out, id_key + 5);
                if (active_workspace_ids.count(id)) {
                    return true; 
                }
            }
            pos++;
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
                bool dirty = false;
                
                while ((pos = pending_data.find('\n')) != std::string::npos) {
                    std::string line = pending_data.substr(0, pos);
                    pending_data.erase(0, pos + 1);
                    
                    if (line.find("openwindow") == 0 ||
                        line.find("closewindow") == 0 ||
                        line.find("movewindow") == 0 ||
                        line.find("workspace") == 0 ||
                        line.find("focusedmon") == 0) {
                        dirty = true;
                    }
                }
                
                if (dirty) {
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
        std::string layers = exec("lswt"); 
        return (layers.find("waybar") != std::string::npos);
    }

    bool has_active_windows() override {
        std::string ws_out = exec("swaymsg -t get_workspaces");
        
        return (ws_out.find("\"focused\": true") != std::string::npos && 
                ws_out.find("\"nodes\": []") == std::string::npos); 
    }

    void listen_for_events(std::function<void()> on_event) override {
        FILE* stream = popen("swaymsg -m -t subscribe '[\"window\", \"workspace\"]'", "r");
        if (!stream) return;

        char buffer[2048];
        while (fgets(buffer, sizeof(buffer), stream) != nullptr) {
            on_event(); 
        }
        pclose(stream);
    }
};

bool is_waybar_visible = false;

void set_waybar(bool visible) {
    bool process_running = (system("pgrep -x waybar > /dev/null") == 0);

    if (visible) {
        if (!process_running) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = {(char*)"waybar", nullptr};
                execvp(args[0], args);
                exit(1); 
            }
            is_waybar_visible = true;
        } else if (!is_waybar_visible) {
            system("killall -q -SIGUSR1 waybar");
            is_waybar_visible = true;
        }
    } else {
        if (process_running && is_waybar_visible) {
            system("killall -q -SIGUSR1 waybar");
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

    is_waybar_visible = backend->is_waybar_active();

    set_waybar(backend->has_active_windows());

    backend->listen_for_events([&backend]() {
        bool has_windows = backend->has_active_windows();
        set_waybar(has_windows);
    });

    return 0;
}
