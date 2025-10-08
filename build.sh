#!/bin/bash

# Скрипт для сборки Birthday Bot

echo "🔨 Building Birthday Bot..."
echo "=========================="

# Создаем директорию для сборки
if [ ! -d "build" ]; then
    echo "📁 Создаем директорию build..."
    mkdir build
fi

cd build

echo "⚙️  Настраиваем CMake..."
cmake .. || {
    echo "❌ Ошибка при настройке CMake"
    exit 1
}

echo "🔨 Собираем проект..."
# Определяем количество ядер процессора (работает на Linux и macOS)
if command -v nproc >/dev/null 2>&1; then
    CORES=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4  # Значение по умолчанию
fi

make -j$CORES || {
    echo "❌ Ошибка при сборке"
    exit 1
}

cd ..

echo "✅ Сборка завершена успешно!"
echo ""
echo "Для запуска бота используйте:"
echo "export BOT_TOKEN=your_bot_token"
echo "./run_bot.sh"
echo ""
echo "Или запустите напрямую:"
echo "./build/birthday_bot"
