# Fractal Image Compression - Algorithm Only

Реализация алгоритма **PIFS (Partitioned Iterated Function Systems)** для фрактального сжатия изображений на C++.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

> **Примечание**: Эта ветка содержит только C++ алгоритм сжатия. Для полного веб-приложения с интерфейсом смотрите ветку [`main`](https://github.com/xloned/fractal_compression/tree/main).

## Особенности

- Полная реализация PIFS алгоритма
- 8 изометрий (все комбинации поворотов/отражений)
- Оптимизация коэффициентов методом наименьших квадратов
- Многопоточная обработка (до 4 потоков CPU)
- Сжатие ~46% от оригинального размера
- Поддержка PNG, JPEG, BMP

## Быстрый старт

### Требования

- C++ компилятор с поддержкой C++17
- CMake 3.10+

### Сборка

```bash
# Клонируем репозиторий (ветка algorithm-only)
git clone -b algorithm-only https://github.com/xloned/fractal_compression.git
cd fractal_compression

# Собираем проект
mkdir -p build && cd build
cmake ..
make -j$(nproc)  # Linux
# make -j$(sysctl -n hw.ncpu)  # macOS
```

### Использование

```bash
# Сжатие изображения
./build/fractal_compress encode input.jpg output.frac

# Распаковка изображения
./build/fractal_compress decode output.frac restored.png
```

## Алгоритм PIFS

### Принцип работы

Фрактальное сжатие использует самоподобие частей изображения:

1. **Разбиение на Range блоки** (8×8 пикселей)
2. **Поиск похожих Domain блоков** (16×16 пикселей)
3. **Применение трансформаций**:
   - Уменьшение Domain блока в 2 раза
   - 8 изометрий (повороты 0°, 90°, 180°, 270° + отражения)
   - Оптимизация контраста α и яркости β
4. **Сохранение только параметров** трансформаций

### Математическая модель

Для каждого Range блока R ищется оптимальная аффинная трансформация:

```
R ≈ α·T(D) + β
```

где:
- `T(D)` — Domain блок после уменьшения и изометрии
- `α` — коэффициент контраста (0.0-1.0)
- `β` — смещение яркости (-128 до 127)

Параметры вычисляются методом наименьших квадратов для минимизации среднеквадратичной ошибки (MSE).

### Формат .frac

Компактный бинарный формат:

```
Заголовок (19 байт):
  - "FRAC" (4 байта) - магическая строка
  - Версия (2 байта)
  - Ширина, высота (4+4 байта)
  - Размер Range блока (1 байт)
  - Количество трансформаций (4 байта)

Данные трансформаций (7 байт × количество блоков):
  - domain_x, domain_y (2+2 байта)
  - rotation (1 байт)
  - brightness (1 байт)
  - contrast (1 байт)
```

## Параметры

Настройка производится в [compression.cpp](compression.cpp):

```cpp
FractalCompressor compressor(
    8,      // range_size: размер Range блока (4, 8, 16)
    2,      // search_step: шаг поиска Domain блоков (1-8)
    12      // max_iterations: итерации декодирования (5-15)
);
```

### Влияние параметров

| Параметр | Меньше | Больше |
|----------|--------|--------|
| **range_size** | Лучше качество, медленнее, больше файл | Хуже качество, быстрее, меньше файл |
| **search_step** | Лучше качество, медленнее | Хуже качество, быстрее |
| **max_iterations** | Хуже восстановление | Лучше восстановление, медленнее |

## Структура проекта

```
fractal_compression/
├── compression.cpp        # Основной алгоритм
├── fractal_compress.h     # Заголовочный файл
├── CMakeLists.txt         # Конфигурация сборки
├── stb_image.h            # Загрузка изображений (STB)
├── stb_image_write.h      # Сохранение изображений (STB)
├── .gitignore
└── README.md
```

## Примеры

### Базовое использование

```bash
# Сжать фото котиков
./build/fractal_compress encode cats.jpg cats.frac

# Проверить размеры
ls -lh cats.jpg cats.frac
# cats.jpg:  12K
# cats.frac: 6.5K  (~46% сжатие)

# Восстановить изображение
./build/fractal_compress decode cats.frac cats_restored.png
```

### Интеграция в код

```cpp
#include "fractal_compress.h"

int main() {
    // Создаём компрессор
    FractalCompressor compressor(
        8,      // range_size
        2,      // search_step
        12      // max_iterations
    );

    // Сжимаем
    compressor.compress("input.jpg", "output.frac");

    // Распаковываем
    compressor.decompress("output.frac", "restored.png");

    return 0;
}
```

## Производительность

Тестирование на изображении 285×177 пикселей (Intel i7):

- **Время сжатия**: ~3-5 секунд (4 потока)
- **Время распаковки**: ~0.5 секунды
- **Сжатие**: ~46% от JPEG (54% экономия)

Сложность алгоритма:
- **Сжатие**: O(n² × m²) где n - количество Range блоков, m - количество Domain блоков
- **Распаковка**: O(n × k) где k - количество итераций

## Технические детали

### Реализованные оптимизации

- **Многопоточность**: Параллельная обработка Range блоков (до 4 потоков)
- **Шаг поиска**: Не проверяем каждый пиксель, используем шаг
- **Биквадратичное усреднение**: Качественное уменьшение Domain блоков
- **Компиляция**: Флаг -O3 для максимальной оптимизации
- **Кэширование**: Минимизация повторных вычислений

### Ограничения

- **Только градации серого**: Цветные изображения конвертируются
- **Квадратичная сложность**: Медленно для больших изображений
- **Lossy сжатие**: Восстановленное изображение не идентично оригиналу
- **Фиксированный размер блока**: Требует пересборки для изменения

## Решение проблем

**Долгое сжатие?**
```cpp
// Увеличьте search_step
FractalCompressor compressor(8, 8, 12);  // было 2
```

**Низкое качество?**
```cpp
// Уменьшите search_step, увеличьте iterations
FractalCompressor compressor(8, 1, 15);
```

**Ошибки компиляции?**
```bash
# Проверьте версию C++
g++ --version  # должна поддерживать C++17

# macOS: установите cmake
brew install cmake

# Linux: установите build-essential
sudo apt-get install cmake build-essential
```

## Ссылки

- [Основная ветка (веб-приложение)](https://github.com/xloned/fractal_compression/tree/main)
- [Fractal Compression - Wikipedia](https://en.wikipedia.org/wiki/Fractal_compression)
- [IFS Theory](https://en.wikipedia.org/wiki/Iterated_function_system)
- [STB Libraries](https://github.com/nothings/stb)

## Научные источники

- **Barnsley, M. F.** (1988). "Fractals Everywhere"
- **Jacquin, A. E.** (1992). "Image coding based on a fractal theory of iterated contractive image transformations"
- **Fisher, Y.** (1995). "Fractal Image Compression: Theory and Application"

## Лицензия

MIT License - Образовательный проект

## Автор

Реализация алгоритма PIFS с оптимизациями для производительности и качества.

---

⭐ Поставьте звезду, если проект был полезен!

🔀 Для полного веб-приложения переключитесь на ветку [`main`](https://github.com/xloned/fractal_compression/tree/main)
