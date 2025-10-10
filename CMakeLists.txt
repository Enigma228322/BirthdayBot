cmake_minimum_required(VERSION 3.16)
project(BirthdayBot)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки компилятора
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Поиск необходимых пакетов
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Поиск OpenSSL (требуется для tgbot-cpp)
find_package(OpenSSL REQUIRED)

# Поиск Boost (требуется для tgbot-cpp)
find_package(Boost REQUIRED COMPONENTS system)

# Добавляем поддиректории с библиотеками (через субмодули)
add_subdirectory(lib/tgbot-cpp)
add_subdirectory(lib/spdlog)
add_subdirectory(lib/json)
add_subdirectory(lib/fmt)

# Создаем директорию для логов
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Создаем исполняемый файл
add_executable(birthday_bot
    src/main.cpp
    src/birthday_manager.cpp
    src/gayrate_manager.cpp
)

# Устанавливаем путь к исполняемому файлу в корневой директории
set_target_properties(birthday_bot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Подключаем заголовочные файлы
target_include_directories(birthday_bot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/tgbot-cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt/include
)

# Линкуем библиотеки
target_link_libraries(birthday_bot
    TgBot
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    fmt::fmt
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)

# Устанавливаем путь к исполняемому файлу
install(TARGETS birthday_bot DESTINATION bin)

# README.md уже существует в проекте
