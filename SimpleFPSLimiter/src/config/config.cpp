#include "pch.h"
#include "config.h"

// Parser minimalista de .ini sin dependencias externas
// Lee líneas con formato: clave=valor
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

void Config::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return; // si no existe, usa defaults

    std::string line;
    while (std::getline(file, line)) {
        // Ignorar comentarios y líneas vacías
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "TargetFPS")       targetFPS = std::stof(value);
        if (key == "LimiterEnabled")  limiterEnabled = (value == "1" || value == "true");
    }
}

void Config::Save(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "; SimpleFPSLimiter config\n";
    file << "; Edita estos valores o usa el overlay en el juego\n\n";
    file << "[Settings]\n";
    file << "TargetFPS=" << targetFPS << "\n";
    file << "LimiterEnabled=" << (limiterEnabled ? "1" : "0") << "\n";
}