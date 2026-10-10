#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include "../libs/std/json.hpp"
#include "../libs/std/net.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#endif

namespace sfs = std::filesystem;

std::string getExecutablePath();
std::string getExecutableDir();
extern const std::string VERSION;
extern const std::string CODENAME;

namespace viss {
namespace updater {

    struct PlatformInfo {
        std::string os;   // "windows", "linux", "macos"
        std::string arch; // "x86_64", "aarch64", "arm", "unknown"
    };

    inline PlatformInfo detectPlatform() {
        PlatformInfo info;
#ifdef _WIN32
        info.os = "windows";
        info.arch = "x86_64";
#elif defined(__APPLE__)
        info.os = "macos";
        #if defined(__aarch64__) || defined(_M_ARM64)
        info.arch = "arm64";
        #else
        info.arch = "x86_64";
        #endif
#else
        info.os = "linux";
        struct utsname ubuf;
        if (uname(&ubuf) == 0) {
            std::string m = ubuf.machine;
            if (m == "aarch64" || m == "arm64") info.arch = "aarch64";
            else if (m == "armv7l" || m == "armv8l" || m == "arm") info.arch = "arm";
            else if (m == "x86_64" || m == "amd64") info.arch = "x86_64";
            else info.arch = m;
        } else {
            #if defined(__aarch64__) || defined(_M_ARM64)
            info.arch = "aarch64";
            #elif defined(__x86_64__) || defined(_M_X64)
            info.arch = "x86_64";
            #else
            info.arch = "unknown";
            #endif
        }
#endif
        return info;
    }

    inline std::vector<int> parseSemver(const std::string& v) {
        std::string s = v;
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s = s.substr(1);
        size_t dash = s.find('-');
        if (dash != std::string::npos) s = s.substr(0, dash);
        std::vector<int> parts;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, '.')) {
            try { parts.push_back(std::stoi(item)); } catch (...) { parts.push_back(0); }
        }
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    }

    inline int compareVersions(const std::string& current, const std::string& target) {
        auto c = parseSemver(current);
        auto t = parseSemver(target);
        for (size_t i = 0; i < 3; ++i) {
            if (t[i] > c[i]) return 1;  // target is newer
            if (t[i] < c[i]) return -1; // current is newer
        }
        return 0; // identical
    }

    struct ReleaseAsset {
        std::string name;
        std::string download_url;
        uintmax_t size = 0;
    };

    struct GithubRelease {
        std::string tag_name;
        std::string title;
        std::string body;
        std::string published_at;
        std::string tarball_url;
        std::string zipball_url;
        std::vector<ReleaseAsset> assets;
    };

    inline std::string fetchUrlContent(const std::string& url) {
        sfs::path tmp_path = sfs::temp_directory_path() / ("viss_rel_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        std::string cmd = "curl -s -L -H \"User-Agent: Viss-Updater/" + VERSION + "\" -H \"Accept: application/vnd.github.v3+json\" -o \"" + tmp_path.string() + "\" \"" + url + "\"";
        int ret = std::system(cmd.c_str());
        if (ret == 0 && sfs::exists(tmp_path) && sfs::file_size(tmp_path) > 0) {
            std::ifstream in(tmp_path);
            std::stringstream ss; ss << in.rdbuf(); in.close();
            std::error_code ec; sfs::remove(tmp_path, ec);
            return ss.str();
        }
        // Fallback using net::get
        return net::get(url, "User-Agent: Viss-Updater/" + VERSION + "\r\nAccept: application/vnd.github.v3+json\r\n");
    }

    inline bool downloadFile(const std::string& url, const std::string& dest_path) {
        std::string cmd = "curl -s -L -A \"Viss-Updater/" + VERSION + "\" -o \"" + dest_path + "\" \"" + url + "\"";
        int ret = std::system(cmd.c_str());
        if (ret == 0 && sfs::exists(dest_path) && sfs::file_size(dest_path) > 0) {
            return true;
        }
        return net::download(url, dest_path);
    }

    inline bool fetchLatestRelease(const std::string& repo, GithubRelease& out_rel, std::string& err_msg) {
        std::string api_url = "https://api.github.com/repos/" + repo + "/releases/latest";
        std::string json_str = fetchUrlContent(api_url);
        if (json_str.empty()) {
            err_msg = "Could not connect to GitHub API at " + api_url;
            return false;
        }

        try {
            auto j = json::parse(json_str);
            if (j.contains("message") && !j.contains("tag_name")) {
                err_msg = "GitHub API response: " + j["message"].to_str();
                return false;
            }
            if (!j.contains("tag_name")) {
                err_msg = "No release tag found in GitHub API response.";
                return false;
            }

            out_rel.tag_name = j["tag_name"].to_str();
            if (j.contains("name")) out_rel.title = j["name"].to_str();
            if (j.contains("body")) out_rel.body = j["body"].to_str();
            if (j.contains("published_at")) out_rel.published_at = j["published_at"].to_str();
            if (j.contains("tarball_url")) out_rel.tarball_url = j["tarball_url"].to_str();
            if (j.contains("zipball_url")) out_rel.zipball_url = j["zipball_url"].to_str();

            if (j.contains("assets") && j["assets"].is_array()) {
                for (Int ai = 0; ai < j["assets"].len(); ++ai) {
                    auto a = j["assets"][ai];
                    ReleaseAsset item;
                    if (a.contains("name")) item.name = a["name"].to_str();
                    if (a.contains("browser_download_url")) item.download_url = a["browser_download_url"].to_str();
                    if (a.contains("size")) item.size = (uintmax_t)a["size"].as_int();
                    out_rel.assets.push_back(item);
                }
            }
            return true;
        } catch (const std::exception& e) {
            err_msg = std::string("JSON parsing error: ") + e.what();
            return false;
        }
    }

    inline const ReleaseAsset* matchAsset(const GithubRelease& rel, const PlatformInfo& plat) {
        for (const auto& a : rel.assets) {
            std::string n = a.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);

            if (plat.os == "windows") {
                if (n.find("windows") != std::string::npos || n.find("win") != std::string::npos || n.find(".exe") != std::string::npos) {
                    return &a;
                }
            } else if (plat.os == "linux") {
                if (n.find("linux") != std::string::npos) {
                    if (plat.arch == "aarch64" && (n.find("arm64") != std::string::npos || n.find("aarch64") != std::string::npos)) {
                        return &a;
                    }
                    if (plat.arch == "x86_64" && (n.find("x86_64") != std::string::npos || n.find("x64") != std::string::npos || n.find("amd64") != std::string::npos)) {
                        return &a;
                    }
                }
            } else if (plat.os == "macos") {
                if (n.find("darwin") != std::string::npos || n.find("macos") != std::string::npos || n.find("apple") != std::string::npos) {
                    return &a;
                }
            }
        }
        return nullptr;
    }

    inline bool installFromSource(const std::string& repo_url, const std::string& ref, const PlatformInfo& plat) {
        std::cout << "[Viss Updater] Building and installing from source (" << ref << ")...\n";
        sfs::path temp_build_dir = sfs::temp_directory_path() / ("viss_src_update_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        sfs::create_directories(temp_build_dir);

        std::string clone_cmd = "git clone --depth 1 -b " + ref + " " + repo_url + " \"" + temp_build_dir.string() + "\"";
        std::cout << "[Viss Updater] Fetching repository: " << repo_url << " [" << ref << "]...\n";
        int c_ret = std::system(clone_cmd.c_str());
        if (c_ret != 0 || !sfs::exists(temp_build_dir / "Makefile")) {
            std::cerr << "[Viss Updater Error] Failed to clone repository branch/tag: " << ref << "\n";
            std::error_code ec; sfs::remove_all(temp_build_dir, ec);
            return false;
        }

        std::cout << "[Viss Updater] Compiling Viss native toolchain...\n";
        int build_ret = 0;
#ifdef _WIN32
        std::string b_cmd = "cd /d \"" + temp_build_dir.string() + "\" && make build";
        build_ret = std::system(b_cmd.c_str());
        if (build_ret != 0 || !sfs::exists(temp_build_dir / "viss.exe")) {
            std::string direct_cmd = "cd /d \"" + temp_build_dir.string() + "\" && g++ -std=c++20 -O3 -pipe -static -static-libgcc -static-libstdc++ -s src/vissc.cpp -o viss.exe -lwinmm -lws2_32 -lwininet -lgdi32 -lgdiplus -lcomdlg32 -lole32 -loleaut32 -luuid -lshell32";
            build_ret = std::system(direct_cmd.c_str());
        }

        if (build_ret == 0 && sfs::exists(temp_build_dir / "viss.exe")) {
            sfs::path current_exe(getExecutablePath());
            sfs::path backup_exe = current_exe.parent_path() / (current_exe.stem().string() + ".old.exe");
            std::error_code ec;
            if (sfs::exists(backup_exe, ec)) sfs::remove(backup_exe, ec);
            sfs::rename(current_exe, backup_exe, ec);
            sfs::copy_file(temp_build_dir / "viss.exe", current_exe, sfs::copy_options::overwrite_existing, ec);

            sfs::path current_libs = current_exe.parent_path() / "libs";
            if (sfs::exists(temp_build_dir / "libs")) {
                sfs::create_directories(current_libs, ec);
                sfs::copy(temp_build_dir / "libs", current_libs, sfs::copy_options::recursive | sfs::copy_options::overwrite_existing, ec);
            }
            sfs::remove_all(temp_build_dir, ec);
            std::cout << "\033[32m[Viss Updater] Successfully updated to latest source build!\033[0m\n";
            return true;
        }
#else
        std::string b_cmd = "cd \"" + temp_build_dir.string() + "\" && make clean && make -j4";
        build_ret = std::system(b_cmd.c_str());
        if (build_ret == 0 && sfs::exists(temp_build_dir / "bin/viss")) {
            std::cout << "[Viss Updater] Installing binary and libraries...\n";
            bool is_root = (geteuid() == 0);
            std::string inst_cmd = "cd \"" + temp_build_dir.string() + "\" && " + (is_root ? "make install" : "sudo make install");
            int inst_ret = std::system(inst_cmd.c_str());
            std::error_code ec; sfs::remove_all(temp_build_dir, ec);
            if (inst_ret == 0) {
                std::cout << "\033[32m[Viss Updater] Successfully updated to latest source build!\033[0m\n";
                return true;
            }
        }
#endif
        std::cerr << "[Viss Updater Error] Build from source failed.\n";
        std::error_code ec; sfs::remove_all(temp_build_dir, ec);
        return false;
    }

    inline int runUpdateCommand(int argc, char* argv[]) {
        bool check_only = false;
        bool force_update = false;
        bool build_source = false;
        std::string target_branch = "";
        std::string repo = "Halva-developer/Viss";

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--check" || arg == "-c") check_only = true;
            else if (arg == "--force" || arg == "-f") force_update = true;
            else if (arg == "--source" || arg == "-s") build_source = true;
            else if (arg == "--branch" || arg == "-b") {
                if (i + 1 < argc) target_branch = argv[++i];
            } else if (arg == "dev" || arg == "nightly") {
                target_branch = "dev/v0.3.0-peppermint-butler";
                build_source = true;
            } else if (arg == "main") {
                target_branch = "main";
                build_source = true;
            }
        }

        PlatformInfo plat = detectPlatform();

        std::cout << "=================================================================\n";
        std::cout << "  Viss Toolchain Self-Updater\n";
        std::cout << "  Current version: v" << VERSION << " \"" << CODENAME << "\" (" << plat.os << "-" << plat.arch << ")\n";
        std::cout << "=================================================================\n\n";

        // If specific branch was requested, build directly from branch
        if (!target_branch.empty()) {
            std::string repo_url = "https://github.com/" + repo + ".git";
            return installFromSource(repo_url, target_branch, plat) ? 0 : 1;
        }

        std::cout << "[Viss Updater] Checking GitHub releases for " << repo << "...\n";
        GithubRelease rel;
        std::string err;
        if (!fetchLatestRelease(repo, rel, err)) {
            std::cerr << "[Viss Updater Error] " << err << "\n";
            std::cout << "[Viss Updater] Would you like to update directly from the Git repository? (viss update dev)\n";
            return 1;
        }

        int cmp = compareVersions(VERSION, rel.tag_name);
        std::cout << "[Viss Updater] Latest available release: \033[36m" << rel.tag_name << "\033[0m"
                  << (!rel.title.empty() ? (" (" + rel.title + ")") : "") << "\n";

        if (check_only) {
            if (cmp > 0) {
                std::cout << "\033[33m[Viss Updater] An update is available: v" << VERSION << " -> " << rel.tag_name << "\033[0m\n";
                std::cout << "Run 'viss update' to install it automatically.\n";
            } else {
                std::cout << "\033[32m[Viss Updater] You are running the latest version (v" << VERSION << ").\033[0m\n";
            }
            return 0;
        }

        if (cmp <= 0 && !force_update && !build_source) {
            std::cout << "\033[32m[Viss Updater] You are already on the latest version (v" << VERSION << ").\033[0m\n";
            std::cout << "To force reinstall, run: viss update --force\n";
            std::cout << "To update to development branch, run: viss update dev\n";
            return 0;
        }

        const ReleaseAsset* asset = build_source ? nullptr : matchAsset(rel, plat);

        // Prebuilt binary path
        if (asset != nullptr) {
            std::cout << "[Viss Updater] Found prebuilt binary package: " << asset->name << "\n";
            sfs::path temp_pkg_dir = sfs::temp_directory_path() / ("viss_pkg_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            sfs::create_directories(temp_pkg_dir);
            sfs::path download_path = temp_pkg_dir / asset->name;

            std::cout << "[Viss Updater] Downloading " << asset->name << "...\n";
            if (!downloadFile(asset->download_url, download_path.string())) {
                std::cerr << "[Viss Updater Error] Failed to download release asset.\n";
                std::error_code ec; sfs::remove_all(temp_pkg_dir, ec);
                return 1;
            }

            // Extract archive or install binary
            std::cout << "[Viss Updater] Extracting package...\n";
            sfs::path extract_dir = temp_pkg_dir / "extracted";
            sfs::create_directories(extract_dir);

            bool extracted = false;
            if (download_path.extension() == ".zip") {
#ifdef _WIN32
                std::string unzip_cmd = "powershell -Command \"Expand-Archive -Path '" + download_path.string() + "' -DestinationPath '" + extract_dir.string() + "' -Force\"";
                extracted = (std::system(unzip_cmd.c_str()) == 0);
#else
                std::string unzip_cmd = "unzip -q \"" + download_path.string() + "\" -d \"" + extract_dir.string() + "\"";
                extracted = (std::system(unzip_cmd.c_str()) == 0);
#endif
            } else if (download_path.extension() == ".gz" || download_path.extension() == ".tgz") {
                std::string tar_cmd = "tar -xzf \"" + download_path.string() + "\" -C \"" + extract_dir.string() + "\"";
                extracted = (std::system(tar_cmd.c_str()) == 0);
            } else {
                // Raw binary
                extract_dir = temp_pkg_dir;
                extracted = true;
            }

            if (!extracted) {
                std::cerr << "[Viss Updater Error] Archive extraction failed.\n";
                std::error_code ec; sfs::remove_all(temp_pkg_dir, ec);
                return 1;
            }

            // Locate new binary
            sfs::path new_bin = "";
            for (const auto& entry : sfs::recursive_directory_iterator(extract_dir)) {
                if (entry.is_regular_file()) {
#ifdef _WIN32
                    if (entry.path().filename() == "viss.exe") { new_bin = entry.path(); break; }
#else
                    if (entry.path().filename() == "viss") { new_bin = entry.path(); break; }
#endif
                }
            }

            if (new_bin.empty() && download_path.extension() == ".exe") {
                new_bin = download_path;
            }

            if (new_bin.empty()) {
                std::cerr << "[Viss Updater Error] Could not find 'viss' binary inside release archive.\n";
                std::error_code ec; sfs::remove_all(temp_pkg_dir, ec);
                return 1;
            }

            // Replace current binary
            sfs::path target_exe(getExecutablePath());
            sfs::path backup_exe = target_exe.parent_path() / (target_exe.stem().string() + ".old" + target_exe.extension().string());

            std::cout << "[Viss Updater] Installing binary into: " << target_exe.string() << "...\n";
            std::error_code ec;
            if (sfs::exists(backup_exe, ec)) sfs::remove(backup_exe, ec);
            sfs::rename(target_exe, backup_exe, ec);

            sfs::copy_file(new_bin, target_exe, sfs::copy_options::overwrite_existing, ec);
            if (ec) {
                std::cerr << "[Viss Updater Error] Failed to write new binary (" << ec.message() << "). Rolling back...\n";
                sfs::rename(backup_exe, target_exe, ec);
                sfs::remove_all(temp_pkg_dir, ec);
                return 1;
            }

#ifndef _WIN32
            chmod(target_exe.c_str(), 0755);
#endif

            // Copy libs/ if bundled
            for (const auto& entry : sfs::recursive_directory_iterator(extract_dir)) {
                if (entry.is_directory() && entry.path().filename() == "libs") {
                    std::cout << "[Viss Updater] Updating standard libraries (libs/)...\n";
#ifdef _WIN32
                    sfs::path dest_libs = target_exe.parent_path() / "libs";
                    sfs::create_directories(dest_libs, ec);
                    sfs::copy(entry.path(), dest_libs, sfs::copy_options::recursive | sfs::copy_options::overwrite_existing, ec);
#else
                    bool is_root = (geteuid() == 0);
                    std::string cp_cmd = (is_root ? "" : "sudo ") + std::string("cp -r \"") + entry.path().string() + "/*\" /usr/local/share/viss/libs/ 2>/dev/null || true";
                    std::system(cp_cmd.c_str());
#endif
                    break;
                }
            }

            sfs::remove_all(temp_pkg_dir, ec);
            std::cout << "\n\033[32m[Viss Updater] Successfully updated to " << rel.tag_name << "!\033[0m\n";
            std::cout << "Verify installation with: viss --version\n\n";
            return 0;
        }

        // Fallback: build from source if no matching binary
        std::cout << "[Viss Updater] No precompiled binary found for " << plat.os << "-" << plat.arch << " in release " << rel.tag_name << ".\n";
        std::string repo_url = "https://github.com/" + repo + ".git";
        return installFromSource(repo_url, rel.tag_name, plat) ? 0 : 1;
    }

} // namespace updater
} // namespace viss
