#include "FileReader.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace LogAnalyzer {

FileReader::FileReader() {
}

FileReader::~FileReader() {
}

bool FileReader::isLogFile(const std::string& filename) {
    // Check common log file extensions
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    return lower.find(".log") != std::string::npos ||
           lower.find(".txt") != std::string::npos ||
           lower.find(".out") != std::string::npos;
}

std::vector<std::string> FileReader::getLogFiles(const std::string& folderPath, bool recursive) {
    std::vector<std::string> files;
    
    if (recursive) {
        findLogFilesRecursive(folderPath, files);
    } else {
        WIN32_FIND_DATAA findData;
        std::string searchPath = folderPath + "\\*.*";
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string filename = findData.cFileName;
                    if (isLogFile(filename)) {
                        files.push_back(folderPath + "\\" + filename);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    
    return files;
}

void FileReader::findLogFilesRecursive(const std::string& path, std::vector<std::string>& files) {
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*.*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        std::string filename = findData.cFileName;
        
        if (filename == "." || filename == "..") {
            continue;
        }
        
        std::string fullPath = path + "\\" + filename;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into subdirectory
            findLogFilesRecursive(fullPath, files);
        } else {
            // Check if it's a log file
            if (isLogFile(filename)) {
                files.push_back(fullPath);
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

bool FileReader::readFile(const std::string& filePath, LineCallback callback) {
    std::ifstream file(filePath, std::ios::binary);
    
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + filePath;
        return false;
    }
    
    std::string line;
    int lineNumber = 0;
    char ch;
    
    while (file.get(ch)) {
        // Handle all line ending types: CR, LF, CRLF
        if (ch == '\n') {
            // LF or CRLF (LF part)
            lineNumber++;
            if (!line.empty() || lineNumber > 1) {  // Don't skip empty lines
                if (!callback(line, lineNumber, filePath)) {
                    file.close();
                    return true;
                }
            }
            line.clear();
        }
        else if (ch == '\r') {
            // CR - could be CR-only or start of CRLF
            // Peek ahead to see if next char is LF
            char next = static_cast<char>(file.peek());
            if (next == '\n') {
                // CRLF - skip the CR, let the LF handler deal with it
                continue;
            }
            else {
                // CR-only (old Mac format)
                lineNumber++;
                if (!line.empty() || lineNumber > 1) {
                    if (!callback(line, lineNumber, filePath)) {
                        file.close();
                        return true;
                    }
                }
                line.clear();
            }
        }
        else {
            line += ch;
        }
    }
    
    // Don't forget the last line if file doesn't end with newline
    if (!line.empty()) {
        lineNumber++;
        callback(line, lineNumber, filePath);
    }
    
    file.close();
    return true;
}

bool FileReader::readFolder(const std::string& folderPath, bool recursive, LineCallback callback) {
    m_lastError.clear();
    
    // Get all log files
    std::vector<std::string> files = getLogFiles(folderPath, recursive);
    
    if (files.empty()) {
        m_lastError = "No log files found in: " + folderPath;
        return false;
    }
    
    // Read each file
    for (const auto& file : files) {
        if (!readFile(file, callback)) {
            return false;
        }
    }
    
    return true;
}

} // namespace LogAnalyzer

// Made with Bob
