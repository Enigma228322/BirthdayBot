#!/bin/bash

# Скрипт для запуска Birthday Bot

echo "🎂 Birthday Bot Launcher"
echo "========================"

# Проверяем наличие токена
if [ -z "$BOT_TOKEN" ]; then
    echo "❌ Ошибка: Не установлена переменная окружения BOT_TOKEN"
    echo ""
    echo "Установите токен бота:"
    echo "export BOT_TOKEN=your_bot_token_here"
    echo ""
    echo "Или запустите с токеном:"
    echo "BOT_TOKEN=your_token ./run_bot.sh"
    exit 1
fi

# Проверяем наличие исполняемого файла
if [ ! -f "./birthday_bot" ]; then
    echo "❌ Исполняемый файл birthday_bot не найден"
    echo ""
    echo "Сначала соберите проект:"
    echo "mkdir build && cd build"
    echo "cmake .. && make"
    echo "cd .."
    exit 1
fi

# Создаем директорию для логов если её нет
mkdir -p logs

echo "✅ Токен бота установлен"
echo "✅ Исполняемый файл найден"
echo "✅ Директория логов создана"
echo ""
echo "🚀 Запускаем Birthday Bot..."
echo "Для остановки нажмите Ctrl+C"
echo ""

# Запускаем бота
./birthday_bot
