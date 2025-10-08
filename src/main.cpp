#include <tgbot/tgbot.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <nlohmann/json.hpp>
#include "birthday_manager.h"
#include <sstream>
#include <regex>
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <limits>

using namespace TgBot;
using namespace std;

class BirthdayBot {
private:
    Bot bot_;
    BirthdayManager birthday_manager_;
    shared_ptr<spdlog::logger> logger_;

    // Очередь отправки сообщений (не блокирует обработчики). Глобальный воркер соблюдает задержки per-chat
    struct PendingMessage { int64_t chatId; string text; };
    mutex queueMutex_;
    condition_variable queueCv_;
    deque<PendingMessage> messageQueue_;
    unordered_map<int64_t, chrono::steady_clock::time_point> chatNextAllowed_;
    chrono::seconds baseDelay_{4};
    thread worker_;
    atomic<bool> stopWorker_{false};

    void startSenderWorker() {
        stopWorker_ = false;
        worker_ = thread([this]() {
            while (!stopWorker_) {
                PendingMessage msg;
                {
                    unique_lock<mutex> lock(queueMutex_);
                    queueCv_.wait(lock, [this]{ return stopWorker_ || !messageQueue_.empty(); });
                    if (stopWorker_) break;
                    msg = messageQueue_.front();
                    messageQueue_.pop_front();
                }

                // Проверяем per-chat окно
                auto now = chrono::steady_clock::now();
                auto it = chatNextAllowed_.find(msg.chatId);
                if (it != chatNextAllowed_.end() && it->second > now) {
                    // Еще рано отправлять: вернем сообщение в конец и подождем немного, не блокируя другие чаты
                    {
                        lock_guard<mutex> lock(queueMutex_);
                        messageQueue_.push_back(msg);
                    }
                    this_thread::sleep_for(chrono::milliseconds(100));
                    continue;
                }

                // Пытаемся отправить
                try {
                    bot_.getApi().sendMessage(msg.chatId, msg.text);
                    this_thread::sleep_for(baseDelay_);
                    logger_->debug("Message sent to chat {}: {}", msg.chatId, msg.text.substr(0, 50) + "...");
                    // Устанавливаем следующее доступное время для чата
                    chatNextAllowed_[msg.chatId] = chrono::steady_clock::now() + baseDelay_;
                } catch (const TgException& e) {
                    const string errorMsg = e.what();
                    logger_->error("Failed to send message to chat {}: {}", msg.chatId, errorMsg);
                    // Обработка 429: извлекаем retry after и планируем повтор
                    if (errorMsg.find("Too Many Requests") != string::npos) {
                        size_t pos = errorMsg.find("retry after ");
                        int waitSec = 60; // по умолчанию
                        if (pos != string::npos) {
                            try {
                                waitSec = stoi(errorMsg.substr(pos + 12));
                            } catch (...) {
                                waitSec = 60;
                            }
                        }
                        logger_->warn("Rate limited for chat {}. Waiting {}s before retry.", msg.chatId, waitSec);
                        chatNextAllowed_[msg.chatId] = chrono::steady_clock::now() + chrono::seconds(waitSec + 1);
                        // Переочередим сообщение в конец очереди, чтобы не блокировать другие чаты
                        {
                            lock_guard<mutex> lock(queueMutex_);
                            messageQueue_.push_back(msg);
                        }
                        // Короткий сон, чтобы не крутиться в холостую
                        this_thread::sleep_for(chrono::milliseconds(100));
                    } else {
                        // Прочие ошибки: легкий backoff и повторная постановка
                        chatNextAllowed_[msg.chatId] = chrono::steady_clock::now() + chrono::seconds(5);
                        {
                            lock_guard<mutex> lock(queueMutex_);
                            messageQueue_.push_back(msg);
                        }
                        this_thread::sleep_for(chrono::milliseconds(50));
                    }
                }
            }
        });
    }

    void stopSenderWorker() {
        stopWorker_ = true;
        queueCv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    void enqueueMessage(int64_t chatId, const string& text) {
        {
            lock_guard<mutex> lock(queueMutex_);
            messageQueue_.push_back(PendingMessage{chatId, text});
        }
        queueCv_.notify_one();
    }

    void setupLogger() {
        // Создаем консольный логгер с цветами
        auto console_sink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Создаем файловый логгер
        auto file_sink = make_shared<spdlog::sinks::daily_file_sink_mt>("logs/birthday_bot.log", 2, 30);
        file_sink->set_level(spdlog::level::debug);

        // Создаем мульти-синк логгер
        vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        logger_ = make_shared<spdlog::logger>("birthday_bot", sinks.begin(), sinks.end());
        logger_->set_level(spdlog::level::debug);

        // Устанавливаем паттерн логирования
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);
    }

    void setupCommands() {
        // Команда /dr N - показать ближайшие дни рождения
        bot_.getEvents().onCommand("dr", [this](Message::Ptr message) {
            logger_->info("Received /dr command from user: {}", message->from->username);

            // Небольшая задержка перед обработкой команды
            this_thread::sleep_for(chrono::milliseconds(500));

            string text = message->text;
            int days = 365; // По умолчанию 365 дней

            // Парсим параметр N
            regex dr_regex(R"(/dr\s+(\d+))");
            smatch match;
            if (regex_search(text, match, dr_regex)) {
                days = stoi(match[1].str());
                if (days > 365) {
                    enqueueMessage(message->chat->id, "Ошибка: N не может быть больше 365 дней");
                    return;
                }
            }

            auto upcoming = birthday_manager_.getUpcomingBirthdays(days);

            if (upcoming.empty()) {
                enqueueMessage(message->chat->id,
                    "В ближайшие " + to_string(days) + " дней дней рождения не найдено.");
                return;
            }

            stringstream response;
            response << "🎂 Дни рождения в ближайшие " << days << " дней:\n\n";

            for (const auto& [info, age] : upcoming) {
                // Вычисляем количество дней до дня рождения
                auto now = chrono::system_clock::now();
                auto time_t = chrono::system_clock::to_time_t(now);
                auto current_tm = *localtime(&time_t);

                int current_day = current_tm.tm_mday;
                int current_month = current_tm.tm_mon + 1;
                int current_year = current_tm.tm_year + 1900;

                int next_birthday_year = current_year;
                if (current_month > info.month || (current_month == info.month && current_day > info.day)) {
                    next_birthday_year++;
                }

                tm birthday_tm = {};
                birthday_tm.tm_year = next_birthday_year - 1900;
                birthday_tm.tm_mon = info.month - 1;
                birthday_tm.tm_mday = info.day;
                birthday_tm.tm_hour = 0;
                birthday_tm.tm_min = 0;
                birthday_tm.tm_sec = 0;

                auto birthday_time = mktime(&birthday_tm);
                auto current_time = mktime(&current_tm);

                int days_until = (birthday_time - current_time) / (24 * 60 * 60);
                if (days_until < 0) days_until = 0;

                response << "👤 " << info.nickname << " - " << info.day << "." << info.month;
                if (days_until == 0) {
                    response << " (СЕГОДНЯ!)";
                } else if (days_until == 1) {
                    response << " (завтра)";
                } else {
                    response << " (через " << days_until << " дней)";
                }
                response << " - исполнится " << age << " лет\n";
            }

            enqueueMessage(message->chat->id, response.str());
        });

        // Команда /add day.month.year - добавить свой день рождения
        bot_.getEvents().onCommand("add", [this](Message::Ptr message) {
            logger_->info("Received /add command from user: {}", message->from->username);

            // Небольшая задержка перед обработкой команды
            this_thread::sleep_for(chrono::milliseconds(500));

            string text = message->text;
            string username = message->from->username;

            if (username.empty()) {
                enqueueMessage(message->chat->id,
                    "Ошибка: У вас не установлен username в Telegram. Пожалуйста, установите username в настройках профиля.");
                return;
            }

            // Парсим команду /add day.month.year
            regex add_regex(R"(/add\s+(\d{1,2})\.(\d{1,2})\.(\d{4}))");
            smatch match;

            if (regex_search(text, match, add_regex)) {
                int day = stoi(match[1].str());
                int month = stoi(match[2].str());
                int year = stoi(match[3].str());

                // Проверяем корректность даты
                if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 || year > 2024) {
                    enqueueMessage(message->chat->id,
                        "Ошибка: Некорректная дата. Используйте формат: /add день.месяц.год (например: /add 15.03.1990)");
                    return;
                }

                birthday_manager_.addBirthday(username, day, month, year);
                enqueueMessage(message->chat->id,
                    "✅ Ваш день рождения " + to_string(day) + "." + to_string(month) + "." + to_string(year) + " успешно сохранен!");

                logger_->info("Added birthday for user {}: {}.{}.{}", username, day, month, year);
            } else {
                // Парсим команду /add nickname day.month.year
                regex add_nickname_regex(R"(/add\s+(\w+)\s+(\d{1,2})\.(\d{1,2})\.(\d{4}))");
                if (regex_search(text, match, add_nickname_regex)) {
                    string nickname = match[1].str();
                    int day = stoi(match[2].str());
                    int month = stoi(match[3].str());
                    int year = stoi(match[4].str());

                    // Проверяем корректность даты
                    if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 || year > 2024) {
                        enqueueMessage(message->chat->id,
                            "Ошибка: Некорректная дата. Используйте формат: /add никнейм день.месяц.год (например: /add john 15.03.1990)");
                        return;
                    }

                    birthday_manager_.addBirthday(nickname, day, month, year);
                    enqueueMessage(message->chat->id,
                        "✅ День рождения пользователя " + nickname + " (" + to_string(day) + "." + to_string(month) + "." + to_string(year) + ") успешно сохранен!");

                    logger_->info("Added birthday for user {} by {}: {}.{}.{}", nickname, username, day, month, year);
                } else {
                    enqueueMessage(message->chat->id,
                        "Ошибка: Неверный формат команды.\n\n"
                        "Используйте:\n"
                        "• /add день.месяц.год - добавить свой день рождения\n"
                        "• /add никнейм день.месяц.год - добавить день рождения другого пользователя\n\n"
                        "Примеры:\n"
                        "• /add 15.03.1990\n"
                        "• /add john 25.12.1985");
                }
            }
        });

        // // Обработка неизвестных команд
        // bot_.getEvents().onAnyMessage([this](Message::Ptr message) {
        //     if (message->text.empty()) return;

        //     if (message->text[0] == '/') {
        //         logger_->warn("Unknown command from user {}: {}", message->from->username, message->text);
        //         enqueueMessage(message->chat->id,
        //             "❓ Неизвестная команда. Доступные команды:\n\n"
        //             "• /dr [N] - показать дни рождения в ближайшие N дней (по умолчанию 365)\n"
        //             "• /add день.месяц.год - добавить свой день рождения\n"
        //             "• /add никнейм день.месяц.год - добавить день рождения другого пользователя");
        //     }
        // });
    }

public:
    BirthdayBot(const string& token) : bot_(token), birthday_manager_("birthdays.json") {
        setupLogger();
        setupCommands();
    }

    void run() {
        logger_->info("Starting Birthday Bot...");

        try {
            logger_->info("Bot username: {}", bot_.getApi().getMe()->username);
            logger_->info("Bot started successfully");
            // Очистим накопленные до старта обновления: пропустим все старые сообщения
            try {
                bot_.getApi().getUpdates(std::numeric_limits<int32_t>::max(), 0, 0, {});
                logger_->info("Skipped pending updates that arrived before startup");
            } catch (const TgException& e) {
                logger_->warn("Failed to skip pending updates: {}", e.what());
            }
            startSenderWorker();

            TgLongPoll longPoll(bot_);
            while (true) {
                try {
                    longPoll.start();
                } catch (const TgException& e) {
                    logger_->error("LongPoll error: {}", e.what());
                    logger_->info("Waiting 5 seconds before retry...");
                    this_thread::sleep_for(chrono::seconds(5));
                }
            }
        } catch (const TgException& e) {
            logger_->error("Telegram error: {}", e.what());
        } catch (const exception& e) {
            logger_->error("General error: {}", e.what());
        }
        stopSenderWorker();
    }
};

int main() {
    // Получаем токен бота из переменной окружения
    const char* token = getenv("BOT_TOKEN");
    if (!token) {
        cerr << "Ошибка: Не установлена переменная окружения BOT_TOKEN" << endl;
        cerr << "Установите токен бота: export BOT_TOKEN=your_bot_token_here" << endl;
        return 1;
    }

    BirthdayBot bot(token);
    bot.run();

    return 0;
}
