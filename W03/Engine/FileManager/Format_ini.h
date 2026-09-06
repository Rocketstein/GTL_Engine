#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

class IniFile {
public:
    template <typename T>
    void Set(const std::string& section, const std::string& key, T value) {
        data[section][key] = std::to_string(value);
    }

    float GetFloat(const std::string& section, const std::string& key, float fallback = 0.f) {
        auto s = data.find(section);
        if (s == data.end()) return fallback;
        auto k = s->second.find(key);
        if (k == s->second.end()) return fallback;
        return std::stof(k->second);
    }

    void Save(const std::string& path) {
        std::ofstream f(path);
        if (!f.is_open()) { return; }
        for (auto& [section, keys] : data) {
            f << "[" << section << "]\n";
            for (auto& [key, value] : keys)
                f << key << "=" << value << "\n";
            f << "\n";
        }
        f.close();
    }

    void Load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) { return; }
        std::string line, currentSection;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == ';') continue;
            if (line[0] == '[') {
                currentSection = line.substr(1, line.find(']') - 1);
            }
            else {
                auto eq = line.find('=');
                if (eq != std::string::npos)
                    data[currentSection][line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
        f.close();
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
};