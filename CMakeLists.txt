cmake_minimum_required(VERSION 3.10)
project(FractalCompression)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Оптимизации для лучшей производительности
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Поиск потоков (для многопоточности)
find_package(Threads REQUIRED)

# Исходные файлы
add_executable(fractal_compress compression.cpp)

# Включаем заголовочные файлы из текущей директории
target_include_directories(fractal_compress PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Линкуем с pthread для многопоточности
target_link_libraries(fractal_compress PRIVATE Threads::Threads)

# Установка исполняемого файла в директорию build
install(TARGETS fractal_compress DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build)
