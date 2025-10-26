#ifndef FRACTAL_COMPRESS_H
#define FRACTAL_COMPRESS_H

#include <vector>
#include <string>
#include <cstdint>

// Структура для хранения информации о трансформации
struct Transform {
    uint16_t domain_x;      // Позиция X домена
    uint16_t domain_y;      // Позиция Y домена
    uint8_t  rotation;      // Поворот (0-7 для 8 изометрий)
    int8_t   brightness;    // Смещение яркости (-128 до 127)
    uint8_t  contrast;      // Масштаб контраста (0-255, 128 = 1.0)
};

// Класс для фрактального сжатия изображений
class FractalCompressor {
public:
    // Конструктор с параметрами качества
    FractalCompressor(int range_size = 8, int search_step = 8, int max_iterations = 10);

    // Сжатие изображения
    bool compress(const std::string& input_path, const std::string& output_path);

    // Распаковка изображения
    bool decompress(const std::string& input_path, const std::string& output_path);

private:
    int range_size_;        // Размер блока range (обычно 4 или 8)
    int domain_size_;       // Размер блока domain (range_size * 2)
    int search_step_;       // Шаг поиска домена (для ускорения)
    int max_iterations_;    // Максимальное количество итераций декодирования

    // Загрузка изображения
    uint8_t* load_image(const std::string& path, int& width, int& height, int& channels);

    // Сохранение изображения
    bool save_image(const std::string& path, const uint8_t* data, int width, int height, int channels);

    // Конвертация в градации серого
    std::vector<uint8_t> to_grayscale(const uint8_t* data, int width, int height, int channels);

    // Получение блока изображения
    std::vector<double> get_block(const std::vector<uint8_t>& image, int width, int height,
                                   int x, int y, int block_size);

    // Уменьшение блока в 2 раза (биквадратичная интерполяция)
    std::vector<double> downsample_block(const std::vector<double>& block, int size);

    // Применение изометрии (поворот/отражение)
    std::vector<double> apply_isometry(const std::vector<double>& block, int size, int rotation);

    // Поиск лучшего соответствия для range блока
    Transform find_best_match(const std::vector<uint8_t>& image, int width, int height,
                              const std::vector<double>& range_block, int range_x, int range_y);

    // Вычисление MSE между двумя блоками с учетом контраста и яркости
    double compute_error(const std::vector<double>& range_block,
                        const std::vector<double>& domain_block,
                        double& best_contrast, double& best_brightness);

    // Применение трансформации к блоку
    std::vector<double> apply_transform(const std::vector<uint8_t>& image, int width, int height,
                                        const Transform& transform);

    // Сохранение/загрузка закодированных данных
    bool save_compressed(const std::string& path, const std::vector<Transform>& transforms,
                        int width, int height);
    bool load_compressed(const std::string& path, std::vector<Transform>& transforms,
                        int& width, int& height);
};

#endif // FRACTAL_COMPRESS_H
