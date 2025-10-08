#include "birthday_manager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

BirthdayManager::BirthdayManager(const std::string& file_path)
    : data_file_path_(file_path) {
    loadData();
}

void BirthdayManager::loadData() {
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

void BirthdayManager::saveData() {
    std::ofstream file(data_file_path_);
    if (file.is_open()) {
        file << data_.dump(4); // Красивое форматирование с отступами
        file.close();
    } else {
        std::cerr << "Error: Cannot save data to file " << data_file_path_ << std::endl;
    }
}

void BirthdayManager::addBirthday(const std::string& nickname, int day, int month, int year) {
    nlohmann::json user_data;
    user_data["day"] = day;
    user_data["month"] = month;
    user_data["year"] = year;

    data_[nickname] = user_data;
    saveData();
}

std::vector<std::pair<BirthdayInfo, int>> BirthdayManager::getUpcomingBirthdays(int days) {
    std::vector<std::pair<BirthdayInfo, int>> upcoming;

    // Получаем текущую дату
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    int current_day = tm.tm_mday;
    int current_month = tm.tm_mon + 1; // tm_mon начинается с 0
    int current_year = tm.tm_year + 1900; // tm_year это годы с 1900

    for (auto& [nickname, user_data] : data_.items()) {
        int day = user_data["day"];
        int month = user_data["month"];
        int year = user_data["year"];

        BirthdayInfo info(nickname, day, month, year);

        // Вычисляем возраст и дату следующего дня рождения
        int age = current_year - year;
        if (current_month > month || (current_month == month && current_day > day)) {
            age++;
        }

        // Находим дату следующего дня рождения
        int next_birthday_year = current_year;
        if (current_month > month || (current_month == month && current_day > day)) {
            next_birthday_year++;
        }

        // Вычисляем количество дней до дня рождения
        std::tm birthday_tm = {};
        birthday_tm.tm_year = next_birthday_year - 1900;
        birthday_tm.tm_mon = month - 1;
        birthday_tm.tm_mday = day;
        birthday_tm.tm_hour = 0;
        birthday_tm.tm_min = 0;
        birthday_tm.tm_sec = 0;

        auto birthday_time = std::mktime(&birthday_tm);
        auto current_time = std::mktime(&tm);

        int days_until_birthday = (birthday_time - current_time) / (24 * 60 * 60);

        // Если день рождения сегодня, то days_until_birthday = 0
        if (days_until_birthday < 0) {
            days_until_birthday = 0;
        }

        if (days_until_birthday <= days) {
            upcoming.push_back({info, age});
        }
    }

    // Сортируем по количеству дней до дня рождения
    std::sort(upcoming.begin(), upcoming.end(),
        [](const auto& a, const auto& b) {
            // Сначала вычисляем дни до дня рождения для каждого
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);

            int current_day = tm.tm_mday;
            int current_month = tm.tm_mon + 1;
            int current_year = tm.tm_year + 1900;

            auto calc_days = [&](const BirthdayInfo& info) {
                int next_birthday_year = current_year;
                if (current_month > info.month || (current_month == info.month && current_day > info.day)) {
                    next_birthday_year++;
                }

                std::tm birthday_tm = {};
                birthday_tm.tm_year = next_birthday_year - 1900;
                birthday_tm.tm_mon = info.month - 1;
                birthday_tm.tm_mday = info.day;
                birthday_tm.tm_hour = 0;
                birthday_tm.tm_min = 0;
                birthday_tm.tm_sec = 0;

                auto birthday_time = std::mktime(&birthday_tm);
                auto current_time = std::mktime(&tm);

                return (birthday_time - current_time) / (24 * 60 * 60);
            };

            return calc_days(a.first) < calc_days(b.first);
        });

    return upcoming;
}

bool BirthdayManager::userExists(const std::string& nickname) {
    return data_.contains(nickname);
}

BirthdayInfo BirthdayManager::getUserInfo(const std::string& nickname) {
    if (data_.contains(nickname)) {
        auto user_data = data_[nickname];
        return BirthdayInfo(nickname, user_data["day"], user_data["month"], user_data["year"]);
    }
    return BirthdayInfo("", 0, 0, 0);
}
