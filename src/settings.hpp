#pragma once
// Tiny key=value settings store persisted to the user's config directory.

#include <map>
#include <string>

namespace fr {

class Settings {
public:
    void load(const std::string& path);
    void save(const std::string& path) const;

    bool has(const std::string& key) const { return map_.count(key) > 0; }
    int getInt(const std::string& key, int def) const;
    double getDouble(const std::string& key, double def) const;
    bool getBool(const std::string& key, bool def) const;
    std::string getString(const std::string& key, const std::string& def) const;

    void set(const std::string& key, int v) { map_[key] = std::to_string(v); }
    void set(const std::string& key, double v);
    void set(const std::string& key, bool v) { map_[key] = v ? "1" : "0"; }
    void set(const std::string& key, const std::string& v) { map_[key] = v; }
    void set(const std::string& key, const char* v) { map_[key] = v; }

    // Remove all keys with the given prefix (used when docks are closed).
    void erasePrefix(const std::string& prefix);

private:
    std::map<std::string, std::string> map_;
};

// ~/Library/Application Support/friture-imgui on macOS, $XDG_CONFIG_HOME or
// ~/.config/friture-imgui elsewhere. Created on demand.
std::string configDir();

}  // namespace fr
