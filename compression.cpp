#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "fractal_compress.h"

#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <thread>
#include <mutex>
#include <vector>

FractalCompressor::FractalCompressor(int range_size, int search_step, int max_iterations)
    : range_size_(range_size), domain_size_(range_size * 2),
      search_step_(search_step), max_iterations_(max_iterations) {
}

uint8_t* FractalCompressor::load_image(const std::string& path, int& width, int& height, int& channels) {
    uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Ошибка загрузки изображения: " << path << std::endl;
        return nullptr;
    }
    std::cout << "Загружено изображение: " << width << "x" << height
              << " каналов: " << channels << std::endl;
    return data;
}

bool FractalCompressor::save_image(const std::string& path, const uint8_t* data,
                                   int width, int height, int channels) {
    int result = stbi_write_png(path.c_str(), width, height, channels, data, width * channels);
    if (!result) {
        std::cerr << "Ошибка сохранения изображения: " << path << std::endl;
        return false;
    }
    std::cout << "Изображение сохранено: " << path << std::endl;
    return true;
}

std::vector<uint8_t> FractalCompressor::to_grayscale(const uint8_t* data, int width,
                                                      int height, int channels) {
    std::vector<uint8_t> gray(width * height);

    for (int i = 0; i < width * height; i++) {
        if (channels >= 3) {
            // Конвертация RGB в grayscale с учетом восприятия человеческого глаза
            gray[i] = static_cast<uint8_t>(
                0.299 * data[i * channels] +
                0.587 * data[i * channels + 1] +
                0.114 * data[i * channels + 2]
            );
        } else {
            gray[i] = data[i * channels];
        }
    }

    return gray;
}

std::vector<double> FractalCompressor::get_block(const std::vector<uint8_t>& image,
                                                  int width, int height,
                                                  int x, int y, int block_size) {
    std::vector<double> block(block_size * block_size);

    for (int j = 0; j < block_size; j++) {
        for (int i = 0; i < block_size; i++) {
            int px = std::min(x + i, width - 1);
            int py = std::min(y + j, height - 1);
            block[j * block_size + i] = image[py * width + px];
        }
    }

    return block;
}

std::vector<double> FractalCompressor::downsample_block(const std::vector<double>& block, int size) {
    int new_size = size / 2;
    std::vector<double> downsampled(new_size * new_size);

    // Биквадратичное усреднение 2x2 пикселей
    for (int j = 0; j < new_size; j++) {
        for (int i = 0; i < new_size; i++) {
            double sum = block[(j*2) * size + (i*2)] +
                        block[(j*2) * size + (i*2 + 1)] +
                        block[(j*2 + 1) * size + (i*2)] +
                        block[(j*2 + 1) * size + (i*2 + 1)];
            downsampled[j * new_size + i] = sum / 4.0;
        }
    }

    return downsampled;
}

std::vector<double> FractalCompressor::apply_isometry(const std::vector<double>& block,
                                                       int size, int rotation) {
    std::vector<double> result(size * size);

    for (int j = 0; j < size; j++) {
        for (int i = 0; i < size; i++) {
            int src_i = i, src_j = j;

            // 8 изометрий (повороты и отражения)
            switch (rotation) {
                case 0: // Без изменений
                    src_i = i; src_j = j;
                    break;
                case 1: // Поворот 90° по часовой
                    src_i = j; src_j = size - 1 - i;
                    break;
                case 2: // Поворот 180°
                    src_i = size - 1 - i; src_j = size - 1 - j;
                    break;
                case 3: // Поворот 270° по часовой
                    src_i = size - 1 - j; src_j = i;
                    break;
                case 4: // Отражение по горизонтали
                    src_i = size - 1 - i; src_j = j;
                    break;
                case 5: // Отражение по вертикали
                    src_i = i; src_j = size - 1 - j;
                    break;
                case 6: // Отражение по диагонали
                    src_i = j; src_j = i;
                    break;
                case 7: // Отражение по другой диагонали
                    src_i = size - 1 - j; src_j = size - 1 - i;
                    break;
            }

            result[j * size + i] = block[src_j * size + src_i];
        }
    }

    return result;
}

double FractalCompressor::compute_error(const std::vector<double>& range_block,
                                        const std::vector<double>& domain_block,
                                        double& best_contrast, double& best_brightness) {
    int n = range_block.size();

    // Вычисление оптимальных коэффициентов линейной регрессии
    double sum_r = 0, sum_d = 0, sum_rr = 0, sum_dd = 0, sum_rd = 0;

    for (int i = 0; i < n; i++) {
        sum_r += range_block[i];
        sum_d += domain_block[i];
        sum_rr += range_block[i] * range_block[i];
        sum_dd += domain_block[i] * domain_block[i];
        sum_rd += range_block[i] * domain_block[i];
    }

    double mean_r = sum_r / n;
    double mean_d = sum_d / n;

    // Вычисление контраста (наклон регрессии)
    double numerator = sum_rd - n * mean_r * mean_d;
    double denominator = sum_dd - n * mean_d * mean_d;

    double contrast = (denominator > 1e-6) ? (numerator / denominator) : 0.0;
    // Ограничение контраста для стабильности и качества
    contrast = std::max(0.0, std::min(1.0, contrast));

    // Вычисление яркости (смещение)
    double brightness = mean_r - contrast * mean_d;
    brightness = std::max(-128.0, std::min(127.0, brightness));

    // Вычисление среднеквадратичной ошибки
    double error = 0.0;
    for (int i = 0; i < n; i++) {
        double predicted = contrast * domain_block[i] + brightness;
        double diff = range_block[i] - predicted;
        error += diff * diff;
    }

    best_contrast = contrast;
    best_brightness = brightness;

    return error / n;
}

Transform FractalCompressor::find_best_match(const std::vector<uint8_t>& image,
                                             int width, int height,
                                             const std::vector<double>& range_block,
                                             int range_x, int range_y) {
    Transform best_transform;
    double best_error = std::numeric_limits<double>::max();

    // Поиск по всему изображению с шагом search_step_
    for (int y = 0; y <= height - domain_size_; y += search_step_) {
        for (int x = 0; x <= width - domain_size_; x += search_step_) {
            // Получаем блок домена
            auto domain_block = get_block(image, width, height, x, y, domain_size_);

            // Уменьшаем домен до размера range
            auto downsampled = downsample_block(domain_block, domain_size_);

            // Проверяем все 8 изометрий
            for (int rot = 0; rot < 8; rot++) {
                auto rotated = apply_isometry(downsampled, range_size_, rot);

                double contrast, brightness;
                double error = compute_error(range_block, rotated, contrast, brightness);

                if (error < best_error) {
                    best_error = error;
                    best_transform.domain_x = x;
                    best_transform.domain_y = y;
                    best_transform.rotation = rot;
                    // Кодируем контраст в диапазоне 0-255, где 255 = 1.0
                    best_transform.contrast = static_cast<uint8_t>(
                        std::max(0.0, std::min(255.0, contrast * 255.0))
                    );
                    best_transform.brightness = static_cast<int8_t>(
                        std::max(-128.0, std::min(127.0, brightness))
                    );
                }
            }
        }
    }

    return best_transform;
}

bool FractalCompressor::compress(const std::string& input_path, const std::string& output_path) {
    std::cout << "=== Начало сжатия ===" << std::endl;

    // Загрузка изображения
    int width, height, channels;
    uint8_t* img_data = load_image(input_path, width, height, channels);
    if (!img_data) return false;

    // Конвертация в grayscale
    auto gray_image = to_grayscale(img_data, width, height, channels);
    stbi_image_free(img_data);

    std::cout << "Размер блока range: " << range_size_ << "x" << range_size_ << std::endl;
    std::cout << "Шаг поиска: " << search_step_ << std::endl;

    // Разбиение на блоки и поиск трансформаций
    int blocks_x = (width + range_size_ - 1) / range_size_;
    int blocks_y = (height + range_size_ - 1) / range_size_;
    int total_blocks = blocks_x * blocks_y;

    std::cout << "Обработка " << total_blocks << " блоков..." << std::endl;

    // ВАЖНО: Используем вектор фиксированного размера для сохранения порядка блоков
    std::vector<Transform> transforms(total_blocks);

    int processed = 0;
    std::mutex progress_mutex;

    // Параллельная обработка блоков
    auto process_block = [&](int start_block, int end_block) {
        for (int block_idx = start_block; block_idx < end_block; block_idx++) {
            int bx = block_idx % blocks_x;
            int by = block_idx / blocks_x;

            int x = bx * range_size_;
            int y = by * range_size_;

            auto range_block = get_block(gray_image, width, height, x, y, range_size_);
            Transform transform = find_best_match(gray_image, width, height,
                                                  range_block, x, y);

            // Сохраняем в нужную позицию массива (не push_back!)
            transforms[block_idx] = transform;

            {
                std::lock_guard<std::mutex> lock(progress_mutex);
                processed++;

                if (processed % 10 == 0 || processed == total_blocks) {
                    std::cout << "Прогресс: " << processed << "/" << total_blocks
                              << " (" << (processed * 100 / total_blocks) << "%)" << std::endl;
                }
            }
        }
    };

    // Используем несколько потоков для ускорения
    int num_threads = std::thread::hardware_concurrency();
    num_threads = std::max(1, std::min(num_threads, 4)); // Ограничиваем 4 потоками

    std::vector<std::thread> threads;
    int blocks_per_thread = (total_blocks + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++) {
        int start = t * blocks_per_thread;
        int end = std::min(start + blocks_per_thread, total_blocks);
        if (start < end) {
            threads.emplace_back(process_block, start, end);
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Сохранение сжатых данных
    bool success = save_compressed(output_path, transforms, width, height);

    if (success) {
        std::cout << "=== Сжатие завершено успешно ===" << std::endl;
    }

    return success;
}

bool FractalCompressor::save_compressed(const std::string& path,
                                        const std::vector<Transform>& transforms,
                                        int width, int height) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Не удалось открыть файл для записи: " << path << std::endl;
        return false;
    }

    // Заголовок файла
    file.write("FRAC", 4);
    uint16_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&width), sizeof(int));
    file.write(reinterpret_cast<const char*>(&height), sizeof(int));

    uint8_t range_size = range_size_;
    file.write(reinterpret_cast<const char*>(&range_size), sizeof(range_size));

    uint32_t num_transforms = transforms.size();
    file.write(reinterpret_cast<const char*>(&num_transforms), sizeof(num_transforms));

    // Трансформации
    for (const auto& t : transforms) {
        file.write(reinterpret_cast<const char*>(&t), sizeof(Transform));
    }

    file.close();
    return true;
}

bool FractalCompressor::load_compressed(const std::string& path,
                                        std::vector<Transform>& transforms,
                                        int& width, int& height) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Не удалось открыть файл для чтения: " << path << std::endl;
        return false;
    }

    // Проверка заголовка
    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "FRAC") {
        std::cerr << "Неверный формат файла" << std::endl;
        return false;
    }

    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    file.read(reinterpret_cast<char*>(&width), sizeof(int));
    file.read(reinterpret_cast<char*>(&height), sizeof(int));

    uint8_t range_size;
    file.read(reinterpret_cast<char*>(&range_size), sizeof(range_size));
    range_size_ = range_size;
    domain_size_ = range_size * 2;

    uint32_t num_transforms;
    file.read(reinterpret_cast<char*>(&num_transforms), sizeof(num_transforms));

    transforms.resize(num_transforms);
    for (auto& t : transforms) {
        file.read(reinterpret_cast<char*>(&t), sizeof(Transform));
    }

    file.close();
    return true;
}

std::vector<double> FractalCompressor::apply_transform(const std::vector<uint8_t>& image,
                                                       int width, int height,
                                                       const Transform& transform) {
    // Получаем домен
    auto domain_block = get_block(image, width, height,
                                  transform.domain_x, transform.domain_y, domain_size_);

    // Уменьшаем
    auto downsampled = downsample_block(domain_block, domain_size_);

    // Применяем изометрию
    auto rotated = apply_isometry(downsampled, range_size_, transform.rotation);

    // Применяем контраст и яркость
    // Декодируем контраст из диапазона 0-255 в 0.0-1.0
    double contrast = transform.contrast / 255.0;
    double brightness = transform.brightness;

    std::vector<double> result(range_size_ * range_size_);
    for (size_t i = 0; i < rotated.size(); i++) {
        result[i] = std::max(0.0, std::min(255.0,
                                           contrast * rotated[i] + brightness));
    }

    return result;
}

bool FractalCompressor::decompress(const std::string& input_path,
                                   const std::string& output_path) {
    std::cout << "=== Начало распаковки ===" << std::endl;

    // Загрузка сжатых данных
    std::vector<Transform> transforms;
    int width, height;

    if (!load_compressed(input_path, transforms, width, height)) {
        return false;
    }

    std::cout << "Размер изображения: " << width << "x" << height << std::endl;
    std::cout << "Количество трансформаций: " << transforms.size() << std::endl;

    // Инициализация изображения серым цветом
    std::vector<uint8_t> image(width * height, 128);

    int blocks_x = (width + range_size_ - 1) / range_size_;

    // Итеративное восстановление
    std::cout << "Итерации восстановления:" << std::endl;
    for (int iter = 0; iter < max_iterations_; iter++) {
        std::vector<uint8_t> new_image(width * height);

        for (size_t idx = 0; idx < transforms.size(); idx++) {
            int bx = idx % blocks_x;
            int by = idx / blocks_x;

            int x = bx * range_size_;
            int y = by * range_size_;

            // Применяем трансформацию
            auto block = apply_transform(image, width, height, transforms[idx]);

            // Копируем результат в изображение
            for (int j = 0; j < range_size_ && (y + j) < height; j++) {
                for (int i = 0; i < range_size_ && (x + i) < width; i++) {
                    new_image[(y + j) * width + (x + i)] =
                        static_cast<uint8_t>(block[j * range_size_ + i]);
                }
            }
        }

        image = new_image;
        std::cout << "  Итерация " << (iter + 1) << "/" << max_iterations_ << std::endl;
    }

    // Сохранение результата
    bool success = save_image(output_path, image.data(), width, height, 1);

    if (success) {
        std::cout << "=== Распаковка завершена успешно ===" << std::endl;
    }

    return success;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Использование:" << std::endl;
        std::cout << "  " << argv[0] << " encode <input> <output>" << std::endl;
        std::cout << "  " << argv[0] << " decode <input> <output>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];

    // Параметры компрессора (настроены для баланса качества и скорости)
    FractalCompressor compressor(
        8,      // range_size: размер блока (8x8)
        2,      // search_step: шаг поиска (меньше = лучше качество, но медленнее)
        12      // max_iterations: итерации декодирования
    );

    if (mode == "encode") {
        if (!compressor.compress(input, output)) {
            std::cerr << "Ошибка сжатия" << std::endl;
            return 1;
        }
    } else if (mode == "decode") {
        if (!compressor.decompress(input, output)) {
            std::cerr << "Ошибка распаковки" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Неизвестный режим: " << mode << std::endl;
        return 1;
    }

    return 0;
}
