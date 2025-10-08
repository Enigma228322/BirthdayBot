#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

struct BirthdayInfo {
    std::string nickname;
    int day;
    int month;
    int year;

    BirthdayInfo() = default;
    BirthdayInfo(const std::string& nick, int d, int m, int y)
        : nickname(nick), day(d), month(m), year(y) {}
};

class BirthdayManager {
private:
    std::string data_file_path_;
    nlohmann::json data_;

    void loadData();
    void saveData();

public:
    BirthdayManager(const std::string& file_path = "birthdays.json");

    // Добавить день рождения пользователя
    void addBirthday(const std::string& nickname, int day, int month, int year);

    // Получить ближайшие дни рождения в течение N дней
    std::vector<std::pair<BirthdayInfo, int>> getUpcomingBirthdays(int days = 365);

    // Проверить, существует ли пользователь
    bool userExists(const std::string& nickname);

    // Получить информацию о пользователе
    BirthdayInfo getUserInfo(const std::string& nickname);
};
