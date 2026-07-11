import os
import sys
import json
import urllib.request
import subprocess
import shutil

CONFIG_FILE = "vpm_config.json"
MODULES_DIR = "viss_modules"

def load_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception:
            pass
    return {"repos": [], "installed": {}}

def save_config(config):
    with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=4)

def ensure_modules_dir():
    if not os.path.exists(MODULES_DIR):
        os.makedirs(MODULES_DIR)

def get_raw_github_url(url, filepath):
    url = url.replace("https://", "").replace("http://", "")
    if url.startswith("github.com/"):
        parts = url.split('/')
        if len(parts) >= 3:
            user = parts[1]
            repo = parts[2].replace(".git", "")
            return [
                f"https://raw.githubusercontent.com/{user}/{repo}/main/{filepath}",
                f"https://raw.githubusercontent.com/{user}/{repo}/master/{filepath}"
            ]
    return []

def download_file(urls, dest_path):
    for url in urls:
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(req) as response:
                with open(dest_path, 'wb') as f:
                    f.write(response.read())
            return True
        except Exception:
            continue
    return False

def fetch_repo_packages(repo_url):
    urls = get_raw_github_url(repo_url, "packages.json")
    temp_json = "temp_packages.json"
    if urls and download_file(urls, temp_json):
        try:
            with open(temp_json, 'r', encoding='utf-8') as f:
                data = json.load(f)
            os.remove(temp_json)
            return data.get("packages", {})
        except Exception:
            if os.path.exists(temp_json):
                os.remove(temp_json)
    
    temp_dir = "temp_vpm_clone"
    if os.path.exists(temp_dir):
        shutil.rmtree(temp_dir)
    
    try:
        result = subprocess.run(["git", "clone", "--depth", "1", repo_url, temp_dir], capture_output=True)
        if result.returncode == 0:
            pkg_json_path = os.path.join(temp_dir, "packages.json")
            if os.path.exists(pkg_json_path):
                with open(pkg_json_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                shutil.rmtree(temp_dir)
                return data.get("packages", {})
    except Exception:
        pass
        
    if os.path.exists(temp_dir):
        shutil.rmtree(temp_dir)
    return {}

def cmd_connect(url):
    config = load_config()
    if url not in config["repos"]:
        config["repos"].append(url)
        save_config(config)
        print(f"[VPM] Successfully connected repository: {url}")
    else:
        print(f"[VPM] Repository is already connected: {url}")

def cmd_update():
    config = load_config()
    if not config["repos"]:
        print("[VPM] No repositories connected. Use 'vpm connect <url>'.")
        return
        
    print("[VPM] Fetching packages from connected repositories...")
    all_packages = {}
    for repo in config["repos"]:
        print(f"  Fetching from {repo}...")
        pkgs = fetch_repo_packages(repo)
        for name, info in pkgs.items():
            info["repo"] = repo
            all_packages[name] = info
            
    with open("vpm_packages_db.json", 'w', encoding='utf-8') as f:
        json.dump(all_packages, f, indent=4)
    print(f"[VPM] Update complete. Found {len(all_packages)} available packages.")

def cmd_install(pkg_name=None, install_all=False, force_yes=False):
    config = load_config()
    ensure_modules_dir()
    
    if not os.path.exists("vpm_packages_db.json"):
        print("[VPM] Package database not found. Running 'vpm update' first...")
        cmd_update()
        if not os.path.exists("vpm_packages_db.json"):
            return
            
    with open("vpm_packages_db.json", 'r', encoding='utf-8') as f:
        db = json.load(f)
        
    if install_all:
        print("[VPM] Installing all packages from repositories...")
        for name in db.keys():
            install_package(name, db[name], config, force_yes)
        save_config(config)
        return

    if not pkg_name:
        print("[VPM] Error: Please specify a package name to install.")
        return
        
    if pkg_name not in db:
        print(f"[VPM] Error: Package '{pkg_name}' not found in connected repositories.")
        return
        
    install_package(pkg_name, db[pkg_name], config, force_yes)
    save_config(config)

def install_package(name, info, config, force_yes):
    if name in config["installed"] and not force_yes:
        ans = input(f"[VPM] Package '{name}' is already installed. Reinstall/Update? (y/n): ")
        if ans.lower() != 'y':
            return
            
    repo = info["repo"]
    filename = info["file"]
    print(f"[VPM] Downloading package '{name}' ({filename})...")
    
    dest_path = os.path.join(MODULES_DIR, filename)
    
    urls = get_raw_github_url(repo, filename)
    success = False
    if urls:
        success = download_file(urls, dest_path)
        
    if not success:
        temp_dir = "temp_vpm_clone"
        if os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)
        try:
            result = subprocess.run(["git", "clone", "--depth", "1", repo, temp_dir], capture_output=True)
            if result.returncode == 0:
                src_file = os.path.join(temp_dir, filename)
                if os.path.exists(src_file):
                    shutil.copy(src_file, dest_path)
                    success = True
        except Exception:
            pass
        if os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)
            
    if success:
        config["installed"][name] = {"version": info.get("version", "1.0.0"), "file": filename}
        print(f"[VPM] Successfully installed '{name}'!")
    else:
        print(f"[VPM] Error: Failed to download package '{name}' from {repo}.")

def cmd_uninstall(name, force=False):
    config = load_config()
    if name not in config["installed"]:
        print(f"[VPM] Package '{name}' is not installed.")
        return
        
    if not force:
        ans = input(f"[VPM] Are you sure you want to uninstall '{name}'? (y/n): ")
        if ans.lower() != 'y':
            return
            
    filename = config["installed"][name]["file"]
    filepath = os.path.join(MODULES_DIR, filename)
    if os.path.exists(filepath):
        os.remove(filepath)
        
    base, _ = os.path.splitext(filename)
    hpp_path = os.path.join(MODULES_DIR, base + ".hpp")
    if os.path.exists(hpp_path):
        os.remove(hpp_path)
        
    del config["installed"][name]
    save_config(config)
    print(f"[VPM] Uninstalled '{name}' successfully.")

def main():
    if len(sys.argv) < 2:
        print("Viss Package Manager (VPM)")
        print("Usage:")
        print("  python vpm.py connect <git_repo_url>      - Connect a repository")
        print("  python vpm.py update                      - Fetch lists of packages")
        print("  python vpm.py install <package>           - Install a package")
        print("  python vpm.py install -a -y               - Install all packages, auto-overwrite")
        print("  python vpm.py uninstall <package> [-f]    - Uninstall a package")
        sys.exit(1)
        
    cmd = sys.argv[1].lower()
    
    if cmd == "connect":
        if len(sys.argv) < 3:
            print("Error: Missing repository URL.")
            sys.exit(1)
        cmd_connect(sys.argv[2])
    elif cmd == "update":
        cmd_update()
    elif cmd == "install":
        install_all = "-a" in sys.argv
        force_yes = "-y" in sys.argv
        pkg = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("-") else None
        cmd_install(pkg, install_all, force_yes)
    elif cmd == "uninstall":
        if len(sys.argv) < 3:
            print("Error: Missing package name.")
            sys.exit(1)
        force = "-f" in sys.argv
        cmd_uninstall(sys.argv[2], force)
    else:
        print(f"Unknown command: {cmd}")

if __name__ == "__main__":
    main()
