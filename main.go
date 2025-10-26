package main

import (
	"fmt"
	"html/template"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

const (
	uploadDir       = "./uploads"
	compressedDir   = "./compressed"
	decompressedDir = "./decompressed"
	templatesDir    = "./templates"
	port            = ":8080"
)

// Структура для передачи данных в шаблон
type PageData struct {
	Message          string
	ImageURL         string
	DownloadURL      string
	ProcessTime      string
	CompressionRatio string
}

func main() {
	// Создаём необходимые директории
	for _, dir := range []string{uploadDir, compressedDir, decompressedDir, templatesDir} {
		if err := os.MkdirAll(dir, 0755); err != nil {
			log.Fatal("Ошибка создания директории:", err)
		}
	}

	// Обработчики
	http.HandleFunc("/", handleIndex)
	http.HandleFunc("/upload", handleUpload)
	http.HandleFunc("/compress", handleCompress)
	http.HandleFunc("/decompress", handleDecompress)

	// Статические файлы
	http.Handle("/uploads/", http.StripPrefix("/uploads/", http.FileServer(http.Dir(uploadDir))))
	http.Handle("/compressed/", http.StripPrefix("/compressed/", http.FileServer(http.Dir(compressedDir))))
	http.Handle("/decompressed/", http.StripPrefix("/decompressed/", http.FileServer(http.Dir(decompressedDir))))

	fmt.Printf("🌟 Сервер фрактального сжатия запущен на http://localhost%s\n", port)
	fmt.Println("📂 Откройте браузер и загрузите изображение для сжатия")

	log.Fatal(http.ListenAndServe(port, nil))
}

// Главная страница
func handleIndex(w http.ResponseWriter, r *http.Request) {
	tmpl, err := template.ParseFiles(filepath.Join(templatesDir, "index.html"))
	if err != nil {
		http.Error(w, "Ошибка загрузки шаблона: "+err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := tmpl.Execute(w, nil); err != nil {
		http.Error(w, "Ошибка рендеринга шаблона: "+err.Error(), http.StatusInternalServerError)
	}
}

// Загрузка и сжатие файла
func handleCompress(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Метод не поддерживается", http.StatusMethodNotAllowed)
		return
	}

	startTime := time.Now()

	// Парсинг загруженного файла
	file, header, err := r.FormFile("file")
	if err != nil {
		respondJSON(w, false, "Ошибка загрузки файла: "+err.Error(), "", "", "", "")
		return
	}
	defer file.Close()

	// Проверка расширения
	ext := strings.ToLower(filepath.Ext(header.Filename))
	if ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" {
		respondJSON(w, false, "Неподдерживаемый формат. Используйте PNG, JPEG или BMP", "", "", "", "")
		return
	}

	// Сохранение загруженного файла
	inputPath := filepath.Join(uploadDir, "input"+ext)
	outputPath := filepath.Join(compressedDir, "compressed.frac")

	dst, err := os.Create(inputPath)
	if err != nil {
		respondJSON(w, false, "Ошибка сохранения файла: "+err.Error(), "", "", "", "")
		return
	}
	defer dst.Close()

	originalSize, err := io.Copy(dst, file)
	if err != nil {
		respondJSON(w, false, "Ошибка записи файла: "+err.Error(), "", "", "", "")
		return
	}
	dst.Close()

	// Запуск компрессора C++
	cmd := exec.Command("./build/fractal_compress", "encode", inputPath, outputPath)
	output, err := cmd.CombinedOutput()
	if err != nil {
		respondJSON(w, false, "Ошибка сжатия: "+err.Error()+"\n"+string(output), "", "", "", "")
		return
	}

	// Статистика
	compressedInfo, err := os.Stat(outputPath)
	if err != nil {
		respondJSON(w, false, "Ошибка получения размера сжатого файла: "+err.Error(), "", "", "", "")
		return
	}

	compressionRatio := fmt.Sprintf("%.2f%%", (1.0-float64(compressedInfo.Size())/float64(originalSize))*100)
	processTime := fmt.Sprintf("%.2fs", time.Since(startTime).Seconds())

	respondJSON(w, true, "✅ Изображение успешно сжато! Теперь нажмите 'Распаковать' для восстановления.", "", "/compressed/compressed.frac", processTime, compressionRatio)
}

// Распаковка изображения
func handleDecompress(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Метод не поддерживается", http.StatusMethodNotAllowed)
		return
	}

	startTime := time.Now()

	var inputPath string
	outputPath := filepath.Join(decompressedDir, "decompressed.png")

	// Проверяем, загружен ли .frac файл
	file, _, err := r.FormFile("file")
	if err == nil {
		// Загружен новый .frac файл
		defer file.Close()

		inputPath = filepath.Join(compressedDir, "uploaded.frac")
		dst, err := os.Create(inputPath)
		if err != nil {
			respondJSON(w, false, "Ошибка сохранения файла: "+err.Error(), "", "", "", "")
			return
		}
		defer dst.Close()

		if _, err := io.Copy(dst, file); err != nil {
			respondJSON(w, false, "Ошибка записи файла: "+err.Error(), "", "", "", "")
			return
		}
	} else {
		// Используем ранее сжатый файл
		inputPath = filepath.Join(compressedDir, "compressed.frac")

		// Проверка наличия сжатого файла
		if _, err := os.Stat(inputPath); os.IsNotExist(err) {
			respondJSON(w, false, "Сначала загрузите и сжмите изображение или загрузите .frac файл", "", "", "", "")
			return
		}
	}

	// Запуск декомпрессора C++
	cmd := exec.Command("./build/fractal_compress", "decode", inputPath, outputPath)
	output, err := cmd.CombinedOutput()
	if err != nil {
		respondJSON(w, false, "Ошибка распаковки: "+err.Error()+"\n"+string(output), "", "", "", "")
		return
	}

	processTime := fmt.Sprintf("%.2fs", time.Since(startTime).Seconds())

	respondJSON(w, true, "✅ Изображение успешно восстановлено!", "/decompressed/decompressed.png", "/decompressed/decompressed.png", processTime, "")
}

// Вспомогательная функция для отправки JSON ответа
func respondJSON(w http.ResponseWriter, success bool, message, imageURL, downloadURL, processTime, compressionRatio string) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")

	response := fmt.Sprintf(`{
		"success": %t,
		"message": "%s",
		"imageUrl": "%s",
		"downloadUrl": "%s",
		"processTime": "%s",
		"compressionRatio": "%s"
	}`, success, message, imageURL, downloadURL, processTime, compressionRatio)

	if !success {
		response = fmt.Sprintf(`{"success": false, "error": "%s"}`, message)
	}

	w.Write([]byte(response))
}

// Обработчик загрузки файла (для первоначальной загрузки)
func handleUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Метод не поддерживается", http.StatusMethodNotAllowed)
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		http.Error(w, "Ошибка загрузки файла", http.StatusBadRequest)
		return
	}
	defer file.Close()

	// Сохранение файла
	filename := filepath.Join(uploadDir, header.Filename)
	dst, err := os.Create(filename)
	if err != nil {
		http.Error(w, "Ошибка сохранения файла", http.StatusInternalServerError)
		return
	}
	defer dst.Close()

	if _, err := io.Copy(dst, file); err != nil {
		http.Error(w, "Ошибка записи файла", http.StatusInternalServerError)
		return
	}

	fmt.Fprintf(w, "Файл %s загружен успешно", header.Filename)
}
