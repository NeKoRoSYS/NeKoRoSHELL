#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <cstdlib>
#include <sstream>
#include <iomanip>

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string sanitize_action(std::string action) {
    action = std::regex_replace(action, std::regex(R"(^\s*exec,\s*)"), "");
    action = std::regex_replace(action, std::regex(R"(\$killPanel;(\s*pkill\s+\$launcher\s*(\|\||;))?\s*)"), "");
    action = std::regex_replace(action, std::regex(R"(if\s+.*?;\s*then\s+)"), "");
    action = std::regex_replace(action, std::regex(R"(\bfi\b)"), "");
    action = std::regex_replace(action, std::regex(R"(;\s*(?=#|$))"), "");
    action = std::regex_replace(action, std::regex(R"(\s*#\s*)"), "  ▏ 󰋎 ");
    return trim(action);
}

std::string parse_binds(const std::string& filepath, const std::string& category) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";

    std::ostringstream output;
    output << " ────────────────── " << category << " ────────────────── \n";

    std::string line;
    std::regex bind_regex(R"(^\s*bind[a-z]*\s*=\s*(.*))");
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_match(line, match, bind_regex)) {
            std::string payload = match[1].str();
            
            std::vector<std::string> parts;
            std::stringstream ss(payload);
            std::string item;
            while (std::getline(ss, item, ',')) {
                parts.push_back(item);
            }

            if (parts.size() >= 3) {
                std::string mod = trim(parts[0]);
                replace_all(mod, "$mainMod", "SUPER");

                std::string key = trim(parts[1]);

                std::string action;
                for (size_t i = 2; i < parts.size(); ++i) {
                    action += parts[i];
                    if (i != parts.size() - 1) action += ",";
                }
                
                action = sanitize_action(action);

                std::string keys_combo = "[" + mod + " + " + key + "]";
                
                std::ostringstream line_out;
                line_out << std::left << std::setw(25) << keys_combo << " " << action << "\n";
                output << line_out.str();
            }
        }
    }
    return output.str();
}

int main() {
    std::string config_home;
    const char* xdg_env = std::getenv("XDG_CONFIG_HOME");
    
    if (xdg_env && *xdg_env != '\0') {
        config_home = xdg_env;
    } else {
        const char* home_env = std::getenv("HOME");
        if (!home_env) {
            std::cerr << "Error: Neither XDG_CONFIG_HOME nor HOME environment variables are set.\n";
            return 1;
        }
        config_home = std::string(home_env) + "/.config";
    }

    std::string core_conf = config_home + "/hypr/conf.d/08-keybinds.conf";
    std::string user_conf = config_home + "/hypr/user/configs/keybinds.conf";

    std::string full_output = parse_binds(core_conf, "SYSTEM CORE BINDS");
    full_output += parse_binds(user_conf, "USER OVERRIDES");

    if (full_output.empty() || full_output.find("SUPER") == std::string::npos) {
        std::cerr << "No keybinds found or config missing.\n";
        return 1;
    }

    std::string rofi_cmd = R"(rofi -dmenu -i -p "  Search Keybinds" -theme-str 'window { width: 900px; border-radius: 12px; } listview { lines: 20; fixed-height: true; } element-text { font: "monospace 11"; }')";

    FILE* pipe = popen(rofi_cmd.c_str(), "w");
    if (!pipe) {
        std::cerr << "Failed to open pipe to Rofi.\n";
        return 1;
    }

    fwrite(full_output.c_str(), 1, full_output.size(), pipe);
    pclose(pipe);

    return 0;
}
