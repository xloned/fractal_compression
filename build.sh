#!/bin/bash

# Скрипт для сборки проекта фрактального сжатия

echo "🔨 Сборка C++ компонента..."

# Создаём директорию build если её нет
mkdir -p build

# Переходим в директорию build
cd build

# Запускаем CMake
echo "⚙️  Конфигурирование CMake..."
cmake ../cpp_src

# Собираем проект
echo "🔧 Компиляция..."
make

# Возвращаемся в корневую директорию
cd ..

echo "✅ C++ компонент собран успешно!"
echo ""
echo "🏗️  Сборка Go сервера..."
go build -o fractal-server main.go

echo ""
echo "✅ Проект успешно собран!"
echo ""
echo "🚀 Для запуска используйте:"
echo "   ./fractal-server"
echo "   или"
echo "   go run main.go"
