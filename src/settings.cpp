#include "settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fr {

void Settings::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos || line.empty() || line[0] == '#') continue;
        map_[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

void Settings::save(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    for (const auto& kv : map_) f << kv.first << '=' << kv.second << '\n';
}

int Settings::getInt(const std::string& key, int def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

double Settings::getDouble(const std::string& key, double def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    try { return std::stod(it->second); } catch (...) { return def; }
}

bool Settings::getBool(const std::string& key, bool def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    return it->second == "1" || it->second == "true";
}

std::string Settings::getString(const std::string& key, const std::string& def) const {
    auto it = map_.find(key);
    return it == map_.end() ? def : it->second;
}

void Settings::set(const std::string& key, double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.17g", v);
    map_[key] = b;
}

void Settings::erasePrefix(const std::string& prefix) {
    for (auto it = map_.begin(); it != map_.end();) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) it = map_.erase(it);
        else ++it;
    }
}

std::string configDir() {
    namespace fs = std::filesystem;
    if (const char* over = std::getenv("FRITURE_CONFIG_DIR")) {
        std::error_code ec;
        fs::create_directories(over, ec);
        return over;
    }
    fs::path base;
    const char* home = std::getenv("HOME");
#ifdef __APPLE__
    base = fs::path(home ? home : ".") / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    base = xdg ? fs::path(xdg) : fs::path(home ? home : ".") / ".config";
#endif
    fs::path dir = base / "friture-imgui";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

}  // namespace fr
