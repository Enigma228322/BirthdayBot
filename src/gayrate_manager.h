#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

struct GayRateInfo {
    std::string nickname;
    int grazd;
    int gayness;

    GayRateInfo() = default;
    GayRateInfo(const std::string& nick, int d, int m)
        : nickname(nick), grazd(d), gayness(m) {}
};

class GayRateManager {
private:
    std::string data_file_path_;
    nlohmann::json data_;

    void loadData();
    void saveData();

public:
    GayRateManager(const std::string& file_path = "GayRates.json");

    void addGayRate(const std::string& nickname, int grazd, int gayness);

    std::vector<GayRateInfo> getTopGayRates(bool sort_by_grazd);

    bool gayExists(const std::string& nickname);

    // Получить информацию о пользователе
    GayRateInfo getGayInfo(const std::string& nickname);
};
