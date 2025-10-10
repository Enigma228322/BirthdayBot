#include "gayrate_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

GayRateManager::GayRateManager(const std::string& file_path)
    : data_file_path_(file_path) {
    loadData();
}

void GayRateManager::loadData() {
    std::ifstream file(data_file_path_);
    if (file.is_open()) {
        try {
            file >> data_;
        } catch (const std::exception& e) {
            std::cerr << "Error loading data: " << e.what() << std::endl;
            data_ = nlohmann::json::object();
        }
        file.close();
    } else {
        // Создаем пустой JSON объект, если файл не существует
        data_ = nlohmann::json::object();
    }
}

void GayRateManager::saveData() {
    std::ofstream file(data_file_path_);
    if (file.is_open()) {
        file << data_.dump(4); // Красивое форматирование с отступами
        file.close();
    } else {
        std::cerr << "Error: Cannot save data to file " << data_file_path_ << std::endl;
    }
}

void GayRateManager::addGayRate(const std::string& nickname, int grazd, int gayness) {
    nlohmann::json gay_data;
    gay_data["grazd"] = grazd;
    gay_data["gayness"] = gayness;

    data_[nickname] = gay_data;
    saveData();
}

std::vector<GayRateInfo> GayRateManager::getTopGayRates(bool sort_by_grazd) {
    std::vector<GayRateInfo> rating;

    for (auto& [nickname, gay_data] : data_.items()) {
        int grazd = gay_data["grazd"];
        int gayness = gay_data["gayness"];
        rating.emplace_back(nickname, grazd, gayness);
    }

    std::sort(rating.begin(), rating.end(),
        [&sort_by_grazd](const auto& a, const auto& b) {
            if (sort_by_grazd) {
                return a.grazd > b.grazd;
            }
            return a.gayness > b.gayness;
        });

    return rating;
}

bool GayRateManager::gayExists(const std::string& nickname) {
    return data_.contains(nickname);
}

GayRateInfo GayRateManager::getGayInfo(const std::string& nickname) {
    if (data_.contains(nickname)) {
        auto gay_data = data_[nickname];
        return GayRateInfo(nickname, gay_data["grazd"], gay_data["gayness"]);
    }
    return GayRateInfo("", 0, 0);
}
