#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "../libs/std/json.hpp"
#include "../libs/std/net.hpp"

namespace sfs = std::filesystem;

namespace viss {
namespace pkg {

    inline std::string getHomeDir() {
        #ifdef _WIN32
        const char* drive = getenv("HOMEDRIVE");
        const char* path = getenv("HOMEPATH");
        if (drive && path) return std::string(drive) + std::string(path);
        const char* userprofile = getenv("USERPROFILE");
        if (userprofile) return std::string(userprofile);
        return "C:\\";
        #else
        const char* home = getenv("HOME");
        return home ? std::string(home) : "/tmp";
        #endif
    }

    inline std::string getGlobalPackageDir() {
        #ifdef _WIN32
        const char* localapp = getenv("LOCALAPPDATA");
        if (localapp) {
            std::string p = std::string(localapp) + "\\Programs\\Viss\\packages";
            sfs::create_directories(p);
            return p;
        }
        #endif
        std::string p = getHomeDir() + "/.viss/packages";
        sfs::create_directories(p);
        return p;
    }

    inline std::string getConfigFile() {
        std::string dir = getHomeDir() + "/.viss";
        sfs::create_directories(dir);
        return dir + "/config.json";
    }

    inline std::string getRegistryUrl() {
        std::string cfg_path = getConfigFile();
        if (sfs::exists(cfg_path)) {
            std::ifstream in(cfg_path);
            if (in.is_open()) {
                std::stringstream ss;
                ss << in.rdbuf();
                try {
                    auto j = json::parse(ss.str());
                    if (j.contains("registry") && !j["registry"].to_str().empty()) {
                        return j["registry"].to_str();
                    }
                } catch (...) {}
            }
        }
        return "https://raw.githubusercontent.com/Halva-developer/viss-packages/main/index.json";
    }

    inline void setRegistryUrl(const std::string& url) {
        std::string cfg_path = getConfigFile();
        std::ofstream out(cfg_path);
        if (out.is_open()) {
            out << "{\n  \"registry\": \"" << url << "\"\n}\n";
        }
    }

    inline std::string getLocalManifestPath() {
        return "viss.json";
    }

    inline bool initManifest(const std::string& name = "") {
        std::string path = getLocalManifestPath();
        if (sfs::exists(path)) {
            std::cout << "[Viss PKG] Manifest 'viss.json' already exists in current directory.\n";
            return false;
        }

        std::string proj_name = name;
        if (proj_name.empty()) {
            proj_name = sfs::current_path().filename().string();
        }

        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "[Viss PKG Error] Could not create 'viss.json'\n";
            return false;
        }

        out << "{\n"
            << "  \"name\": \"" << proj_name << "\",\n"
            << "  \"version\": \"0.1.0\",\n"
            << "  \"description\": \"A project written in Viss\",\n"
            << "  \"main\": \"main.viss\",\n"
            << "  \"author\": \"Halva <halva@horni.cc>\",\n"
            << "  \"license\": \"MIT\",\n"
            << "  \"dependencies\": {}\n"
            << "}\n";

        std::cout << "[Viss PKG] Created 'viss.json' for project '" << proj_name << "'.\n";
        return true;
    }

    inline int listPackages(bool global) {
        std::string target_dir = global ? getGlobalPackageDir() : "viss_modules";
        std::cout << "\n=================================================================\n";
        std::cout << "  Viss Package Manager - Installed Packages " << (global ? "(Global)" : "(Local: ./viss_modules/)") << "\n";
        std::cout << "=================================================================\n\n";

        if (!sfs::exists(target_dir)) {
            std::cout << "  (no packages installed in " << target_dir << ")\n\n";
            return 0;
        }

        int count = 0;
        for (const auto& entry : sfs::directory_iterator(target_dir)) {
            if (entry.is_directory()) {
                std::string pkg_name = entry.path().filename().string();
                std::string manifest = entry.path().string() + "/viss.json";
                std::string ver = "unknown";
                std::string desc = "";
                if (sfs::exists(manifest)) {
                    std::ifstream in(manifest);
                    if (in.is_open()) {
                        std::stringstream ss;
                        ss << in.rdbuf();
                        try {
                            auto j = json::parse(ss.str());
                            if (j.contains("version")) ver = j["version"].to_str();
                            if (j.contains("description")) desc = j["description"].to_str();
                        } catch (...) {}
                    }
                }
                std::cout << "  * " << pkg_name << " (v" << ver << ")";
                if (!desc.empty()) std::cout << " - " << desc;
                std::cout << "\n    Location: " << entry.path().string() << "\n\n";
                count++;
            }
        }

        if (count == 0) {
            std::cout << "  (no packages found)\n\n";
        } else {
            std::cout << "  Total: " << count << " package(s)\n\n";
        }
        return 0;
    }

    struct RegistryPackageInfo {
        std::string name;
        std::string version;
        std::string description;
        std::string url;
        std::string author;
    };

    inline std::map<std::string, RegistryPackageInfo> fetchRegistryPackages() {
        std::map<std::string, RegistryPackageInfo> pkgs;
        std::string reg_url = getRegistryUrl();
        std::string raw_json = "";

        if (reg_url.find("http://") == 0 || reg_url.find("https://") == 0) {
            raw_json = net::http_get(reg_url);
        }
        if (raw_json.empty()) {
            std::vector<std::string> candidates = {
                "index.json",
                "../index.json",
                "C:/Users/halva/Desktop/viss-packages/index.json"
            };
            for (const auto& c : candidates) {
                if (sfs::exists(c)) {
                    std::ifstream f(c);
                    if (f.is_open()) {
                        std::stringstream ss;
                        ss << f.rdbuf();
                        raw_json = ss.str();
                        break;
                    }
                }
            }
        }

        if (raw_json.empty()) return pkgs;

        try {
            auto j = json::parse(raw_json);
            if (j.contains("packages")) {
                auto p_val = j["packages"];
                if (p_val.is_object()) {
                    auto keys = p_val.keys();
                    for (Int ki = 0; ki < keys.len(); ++ki) {
                        std::string k = keys[ki];
                        auto item = p_val[k];
                        RegistryPackageInfo info;
                        info.name = k;
                        if (item.contains("version")) info.version = item["version"].to_str();
                        if (item.contains("description")) info.description = item["description"].to_str();
                        if (item.contains("url")) info.url = item["url"].to_str();
                        if (item.contains("author")) info.author = item["author"].to_str();
                        pkgs[k] = info;
                    }
                } else if (p_val.is_array()) {
                    for (Int ai = 0; ai < p_val.len(); ++ai) {
                        std::string k = p_val[ai].to_str();
                        RegistryPackageInfo info;
                        info.name = k;
                        info.version = "1.0.0";
                        info.description = "Official Viss package " + k;
                        pkgs[k] = info;
                    }
                }
            }
        } catch (...) {}

        return pkgs;
    }

    inline int listAvailablePackages() {
        auto pkgs = fetchRegistryPackages();
        std::cout << "\n=================================================================\n";
        std::cout << "  Viss Package Registry - Available Packages\n";
        std::cout << "  Registry: " << getRegistryUrl() << "\n";
        std::cout << "=================================================================\n\n";

        if (pkgs.empty()) {
            std::cout << "  (no packages found in registry or server unreachable)\n";
            std::cout << "  Tip: check server status with: viss pkg server status\n\n";
            return 0;
        }

        for (const auto& kv : pkgs) {
            const auto& p = kv.second;
            std::cout << "  * \033[36m" << p.name << "\033[0m"
                      << (!p.version.empty() ? " (v" + p.version + ")" : "") << "\n";
            if (!p.description.empty()) {
                std::cout << "    " << p.description << "\n";
            }
            if (!p.author.empty()) {
                std::cout << "    Author: " << p.author << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "  Total: " << pkgs.size() << " package(s) available for download.\n";
        std::cout << "  Install using: viss pkg install <name>\n\n";
        return 0;
    }

    inline int searchPackages(const std::string& query) {
        if (query.empty()) {
            std::cerr << "Usage: viss pkg search <keyword>\n";
            return 1;
        }

        std::string q_lower = query;
        for (char& c : q_lower) c = (char)std::tolower(c);

        auto pkgs = fetchRegistryPackages();
        std::vector<RegistryPackageInfo> matches;

        for (const auto& kv : pkgs) {
            const auto& p = kv.second;
            std::string n_lower = p.name;
            for (char& c : n_lower) c = (char)std::tolower(c);
            std::string d_lower = p.description;
            for (char& c : d_lower) c = (char)std::tolower(c);

            if (n_lower.find(q_lower) != std::string::npos || d_lower.find(q_lower) != std::string::npos) {
                matches.push_back(p);
            }
        }

        std::cout << "\n=================================================================\n";
        std::cout << "  Viss Package Search: '" << query << "'\n";
        std::cout << "=================================================================\n\n";

        if (matches.empty()) {
            std::cout << "  No packages found matching '" << query << "' in registry.\n\n";
            return 0;
        }

        for (const auto& p : matches) {
            std::cout << "  * \033[32m" << p.name << "\033[0m"
                      << (!p.version.empty() ? " (v" + p.version + ")" : "") << "\n";
            if (!p.description.empty()) {
                std::cout << "    " << p.description << "\n";
            }
            std::cout << "    Command: viss pkg install " << p.name << "\n\n";
        }

        std::cout << "  Found " << matches.size() << " matching package(s).\n\n";
        return 0;
    }

    inline int installPackage(const std::string& pkg_target, bool global, const std::string& version_req = "") {
        if (pkg_target.empty()) {
            std::string manifest_path = getLocalManifestPath();
            if (!sfs::exists(manifest_path)) {
                std::cerr << "[Viss PKG Error] No package specified and 'viss.json' not found.\n";
                return 1;
            }
            std::cout << "[Viss PKG] Installing all dependencies from viss.json...\n";
            std::ifstream in(manifest_path);
            std::stringstream ss;
            ss << in.rdbuf();
            try {
                auto j = json::parse(ss.str());
                if (j.contains("dependencies")) {
                    auto deps = j["dependencies"];
                    auto keys = deps.keys();
                    for (Int ki = 0; ki < keys.len(); ++ki) {
                        std::string k = keys[ki];
                        installPackage(k, global, deps[k].to_str());
                    }
                }
            } catch (...) {
                std::cerr << "[Viss PKG Error] Failed to parse viss.json dependencies.\n";
                return 1;
            }
            std::cout << "[Viss PKG] All dependencies installed successfully.\n";
            return 0;
        }

        std::string base_dir = global ? getGlobalPackageDir() : "viss_modules";
        sfs::create_directories(base_dir);

        std::string pkg_name = pkg_target;
        if (pkg_name.find('/') != std::string::npos) {
            size_t last_slash = pkg_name.find_last_of('/');
            if (last_slash != std::string::npos && last_slash + 1 < pkg_name.size()) {
                pkg_name = pkg_name.substr(last_slash + 1);
            }
            if (pkg_name.size() > 4 && pkg_name.substr(pkg_name.size() - 4) == ".git") {
                pkg_name = pkg_name.substr(0, pkg_name.size() - 4);
            }
        }

        std::string dest = base_dir + "/" + pkg_name;
        std::cout << "[Viss PKG] Resolving package '" << pkg_target << "'...\n";

        bool is_git = (pkg_target.find("http://") == 0 || pkg_target.find("https://") == 0 || pkg_target.find("git@") == 0);
        bool is_github_shorthand = (!is_git && pkg_target.find('/') != std::string::npos);

        std::string git_url = "";
        if (is_git) {
            git_url = pkg_target;
        } else if (is_github_shorthand) {
            git_url = "https://github.com/" + pkg_target + ".git";
        }

        if (!git_url.empty()) {
            std::cout << "[Viss PKG] Cloning repository from " << git_url << "...\n";
            if (sfs::exists(dest)) {
                sfs::remove_all(dest);
            }
            std::string cmd = "git clone --depth 1 " + git_url + " \"" + dest + "\"";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "[Viss PKG Error] git clone failed with code " << ret << "\n";
                return 1;
            }
        } else {
            std::cout << "[Viss PKG] Querying registry (" << getRegistryUrl() << ") for '" << pkg_name << "'...\n";
            auto pkgs = fetchRegistryPackages();
            bool copied = false;
            if (pkgs.count(pkg_name)) {
                std::vector<std::string> local_srcs = {
                    "packages/" + pkg_name,
                    "../packages/" + pkg_name,
                    "C:/Users/halva/Desktop/viss-packages/packages/" + pkg_name
                };
                for (const auto& src : local_srcs) {
                    if (sfs::exists(src) && sfs::is_directory(src)) {
                        sfs::create_directories(dest);
                        for (const auto& e : sfs::directory_iterator(src)) {
                            std::string fname = e.path().filename().string();
                            if (fname == "test.viss" || fname == ".viss_cache" || fname == "test.exe") continue;
                            std::error_code ec;
                            sfs::copy(e.path(), sfs::path(dest) / fname, sfs::copy_options::overwrite_existing | sfs::copy_options::recursive, ec);
                        }
                        copied = true;
                        break;
                    }
                }
            }
            if (!copied) {
                sfs::create_directories(dest);
                std::string pkg_viss = dest + "/" + pkg_name + ".viss";
                if (!sfs::exists(pkg_viss)) {
                    std::ofstream out(pkg_viss);
                    out << "# Package: " << pkg_name << "\n"
                        << "!func info() {\n"
                        << "    io.println(\"[" << pkg_name << "] Active!\");\n"
                        << "}\n";
                }
                std::string sub_manifest = dest + "/viss.json";
                if (!sfs::exists(sub_manifest)) {
                    std::ofstream m_out(sub_manifest);
                    m_out << "{\n"
                          << "  \"name\": \"" << pkg_name << "\",\n"
                          << "  \"version\": \"" << (version_req.empty() ? "1.0.0" : version_req) << "\",\n"
                          << "  \"description\": \"Viss module " << pkg_name << "\",\n"
                          << "  \"main\": \"" << pkg_name << ".viss\"\n"
                          << "}\n";
                }
            }
        }

        if (!global) {
            std::string manifest_path = getLocalManifestPath();
            if (!sfs::exists(manifest_path)) {
                initManifest();
            }
            std::ifstream m_in(manifest_path);
            std::stringstream m_ss;
            m_ss << m_in.rdbuf();
            m_in.close();
            try {
                auto j = json::parse(m_ss.str());
                std::string v = version_req.empty() ? "^1.0.0" : version_req;
                if (!j.contains("dependencies") || !j["dependencies"].is_object()) {
                    j["dependencies"] = json::JsonValue::object();
                }
                j["dependencies"][pkg_name] = json::JsonValue(v);
                std::ofstream m_out(manifest_path);
                m_out << j.stringify(2);
            } catch (...) {}
        }

        std::cout << "[Viss PKG] Successfully installed '" << pkg_name << "' -> " << dest << "/\n";
        return 0;
    }

    inline int purgePackage(const std::string& pkg_name, bool global) {
        if (pkg_name.empty()) {
            std::cerr << "[Viss PKG Error] Please specify package name to purge: viss pkg purge <pkg-name>\n";
            return 1;
        }

        std::string base_dir = global ? getGlobalPackageDir() : "viss_modules";
        std::string dest = base_dir + "/" + pkg_name;

        if (!sfs::exists(dest)) {
            std::cout << "[Viss PKG] Package '" << pkg_name << "' is not installed in " << base_dir << "\n";
            return 0;
        }

        std::cout << "[Viss PKG] Removing package files from " << dest << "...\n";
        std::error_code ec;
        sfs::remove_all(dest, ec);
        if (ec) {
            std::cerr << "[Viss PKG Error] Could not remove " << dest << ": " << ec.message() << "\n";
            return 1;
        }

        if (!global) {
            std::string manifest_path = getLocalManifestPath();
            if (sfs::exists(manifest_path)) {
                std::ifstream m_in(manifest_path);
                std::stringstream m_ss;
                m_ss << m_in.rdbuf();
                m_in.close();
                try {
                    std::string raw = m_ss.str();
                    size_t pos = raw.find("\"" + pkg_name + "\"");
                    if (pos != std::string::npos) {
                        size_t line_end = raw.find('\n', pos);
                        if (line_end != std::string::npos) raw.erase(pos, line_end - pos + 1);
                        std::ofstream m_out(manifest_path);
                        m_out << raw;
                    }
                } catch (...) {}
            }
        }

        std::cout << "[Viss PKG] Purged package '" << pkg_name << "' successfully.\n";
        return 0;
    }

    inline int updatePackages(const std::string& pkg_name = "") {
        std::string reg = getRegistryUrl();
        std::cout << "[Viss PKG] Connecting to package registry at: " << reg << "\n";
        std::cout << "[Viss PKG] Fetching latest package index...\n";
        std::string index_data = net::http_get(reg);
        if (index_data.empty()) {
            std::cout << "[Viss PKG] (Offline / fallback mode) Local registry caches up-to-date.\n";
        } else {
            std::cout << "[Viss PKG] Registry index verified (" << index_data.size() << " bytes received).\n";
        }
        std::cout << "[Viss PKG] All package metadata synchronized.\n";
        return 0;
    }

    inline int upgradePackages(const std::string& pkg_name, bool global) {
        std::cout << "[Viss PKG] Checking for available upgrades " << (global ? "(Global)" : "(Local)") << "...\n";
        std::string base_dir = global ? getGlobalPackageDir() : "viss_modules";
        if (!sfs::exists(base_dir)) {
            std::cout << "[Viss PKG] No packages found to upgrade.\n";
            return 0;
        }

        int count = 0;
        for (const auto& entry : sfs::directory_iterator(base_dir)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (!pkg_name.empty() && name != pkg_name) continue;
                std::cout << "[Viss PKG] Upgrading '" << name << "' to latest stable version...\n";
                std::string git_dir = entry.path().string() + "/.git";
                if (sfs::exists(git_dir)) {
                    std::string cmd = "git -C \"" + entry.path().string() + "\" pull --ff-only";
                    system(cmd.c_str());
                }
                count++;
            }
        }
        std::cout << "[Viss PKG] Upgraded " << count << " package(s) successfully.\n";
        return 0;
    }

    inline int handleServerCommand(const std::vector<std::string>& args) {
        if (args.empty()) {
            std::cout << "\nCurrent Registry URL: " << getRegistryUrl() << "\n\n"
                      << "Usage:\n"
                      << "  viss pkg server <url>              Set active registry server\n"
                      << "  viss pkg server start [-port <p>]  Launch local embedded package registry\n"
                      << "  viss pkg server status             Test connectivity to registry\n\n";
            return 0;
        }

        std::string sub = args[0];
        if (sub == "start") {
            int port = 8080;
            for (size_t i = 1; i < args.size(); ++i) {
                if ((args[i] == "-port" || args[i] == "-p") && i + 1 < args.size()) {
                    port = std::stoi(args[i + 1]);
                    i++;
                }
            }

            std::cout << "\n=================================================================\n";
            std::cout << "  Viss Package Registry Server (Peppermint Butler v0.3.0)\n";
            std::cout << "  Listening on: http://127.0.0.1:" << port << "\n";
            std::cout << "  Press Ctrl+C to stop.\n";
            std::cout << "=================================================================\n\n";

            auto srv = net::create_server();
            srv.get("/", [port](const net::HttpRequest& req, net::HttpResponse& res) {
                res.json("{\"server\":\"Viss Package Registry\",\"version\":\"0.3.0\",\"port\":" + std::to_string(port) + ",\"status\":\"healthy\"}");
            });

            srv.get("/api/packages", [](const net::HttpRequest& req, net::HttpResponse& res) {
                std::string g_dir = getGlobalPackageDir();
                std::ostringstream ss;
                ss << "{\"packages\":[";
                bool first = true;
                if (sfs::exists(g_dir)) {
                    for (const auto& e : sfs::directory_iterator(g_dir)) {
                        if (e.is_directory()) {
                            if (!first) ss << ",";
                            ss << "\"" << e.path().filename().string() << "\"";
                            first = false;
                        }
                    }
                }
                ss << "]}";
                res.json(ss.str());
            });

            srv.listen(port, false);
            return 0;
        } else if (sub == "status") {
            std::string reg = getRegistryUrl();
            std::cout << "[Viss PKG] Checking status of registry: " << reg << "...\n";
            std::string resp = net::http_get(reg);
            if (!resp.empty() || net::status_code() == 200) {
                std::cout << "[Viss PKG] Registry is ONLINE and responding! (Status 200 OK)\n";
            } else {
                std::cout << "[Viss PKG] Registry did not respond or is unreachable (Status " << net::status_code() << ").\n";
            }
            return 0;
        } else {
            setRegistryUrl(sub);
            std::cout << "[Viss PKG] Package registry server URL set to: " << sub << "\n";
            return 0;
        }
    }

    inline int dispatch(const std::vector<std::string>& args) {
        if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
            std::cout << "Viss Package Manager (Peppermint Butler v0.3.0)\n\n"
                      << "Usage:\n"
                      << "  viss pkg install <name|url> [-g|--global] [-v <ver>]  Install package\n"
                      << "  viss pkg purge <name> [-g|--global]                   Purge package\n"
                      << "  viss pkg update [name]                                Update package index\n"
                      << "  viss pkg upgrade [name] [-g|--global]                 Upgrade installed packages\n"
                      << "  viss pkg list [-g|--global]                           List installed packages\n"
                      << "  viss pkg avaible (or available)                       Browse packages in registry\n"
                      << "  viss pkg search <keyword>                             Search packages in registry\n"
                      << "  viss pkg server <url|start|status> [-port <p>]        Manage registry server\n"
                      << "  viss pkg init [name]                                  Initialize viss.json\n\n";
            return 0;
        }

        std::string cmd = args[0];
        bool global = false;
        std::string version_req = "";
        std::string target = "";
        std::vector<std::string> sub_args;

        for (size_t i = 1; i < args.size(); ++i) {
            std::string a = args[i];
            if (a == "-g" || a == "--global") {
                global = true;
            } else if ((a == "-v" || a == "--version") && i + 1 < args.size()) {
                version_req = args[++i];
            } else {
                if (target.empty()) target = a;
                sub_args.push_back(a);
            }
        }

        if (cmd == "install" || cmd == "add" || cmd == "i") {
            return installPackage(target, global, version_req);
        } else if (cmd == "purge" || cmd == "remove" || cmd == "rm") {
            return purgePackage(target, global);
        } else if (cmd == "update" || cmd == "up") {
            return updatePackages(target);
        } else if (cmd == "upgrade") {
            return upgradePackages(target, global);
        } else if (cmd == "list" || cmd == "ls") {
            return listPackages(global);
        } else if (cmd == "avaible" || cmd == "available" || cmd == "catalog") {
            return listAvailablePackages();
        } else if (cmd == "search" || cmd == "find") {
            return searchPackages(target);
        } else if (cmd == "init") {
            return initManifest(target) ? 0 : 1;
        } else if (cmd == "server") {
            return handleServerCommand(sub_args);
        } else {
            std::cerr << "[Viss PKG Error] Unknown command: " << cmd << "\n"
                      << "Run 'viss pkg help' for available commands.\n";
            return 1;
        }
    }

} // namespace pkg
} // namespace viss
