#pragma once
#include <fstream>
#include <string>

/**
 * @brief Проверяет, существует ли файл.
 * @param path Путь к файлу.
 * @return true, если файл можно открыть для чтения.
 */
inline bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}