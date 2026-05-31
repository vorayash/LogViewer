#include "FileReader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace LogAnalyzer {

FileReader::FileReader() {
}

FileReader::~FileReader() {
}

// Returns true if 'filename' matches the configured mask.
// The mask supports Windows wildcards (* and ?) and multiple ';'-separated
// patterns, e.g. "*.txt", "*.log;*.txt", "app*.log". Empty mask = match all.
bool FileReader::isLogFile(const std::string& filename) {
    if (m_filter.empty()) return true;

    // PathMatchSpecA matches a single pattern; split the mask on ';' so we
    // support multiple patterns. It is case-insensitive.
    std::stringstream ss(m_filter);
    std::string pat;
    while (std::getline(ss, pat, ';')) {
        // Trim surrounding whitespace
        size_t a = pat.find_first_not_of(" \t");
        size_t b = pat.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        pat = pat.substr(a, b - a + 1);
        if (pat.empty()) continue;
        if (PathMatchSpecA(filename.c_str(), pat.c_str())) return true;
    }
    return false;
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

unsigned long long FileReader::totalSize(const std::vector<std::string>& files) const {
    unsigned long long total = 0;
    for (const auto& f : files) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(f.c_str(), GetFileExInfoStandard, &fad)) {
            ULARGE_INTEGER sz;
            sz.LowPart  = fad.nFileSizeLow;
            sz.HighPart = fad.nFileSizeHigh;
            total += sz.QuadPart;
        }
    }
    return total;
}

bool FileReader::readFile(const std::string& filePath, LineCallback callback) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + filePath;
        return false;
    }

    // Buffered read — dramatically faster than char-by-char get().
    const size_t BUF_SIZE = 64 * 1024;
    std::vector<char> buf(BUF_SIZE);
    std::string line;
    line.reserve(256);
    int  lineNumber  = 0;
    bool prevWasCR   = false;

    auto emit = [&](void) -> bool {
        lineNumber++;
        if (!line.empty() || lineNumber > 1) {
            if (!callback(line, lineNumber, filePath)) return false;
        }
        line.clear();
        return true;
    };

    while (file) {
        file.read(buf.data(), (std::streamsize)BUF_SIZE);
        std::streamsize got = file.gcount();
        if (got <= 0) break;

        // Report progress per block (throttled by the caller via member counters)
        m_bytesRead += (unsigned long long)got;
        reportProgress();

        for (std::streamsize i = 0; i < got; i++) {
            char ch = buf[(size_t)i];
            if (ch == '\n') {
                if (prevWasCR) { prevWasCR = false; continue; } // CRLF: LF already handled
                if (!emit()) { file.close(); return true; }
            } else if (ch == '\r') {
                // CR (or start of CRLF) — emit line now, swallow following LF
                prevWasCR = true;
                if (!emit()) { file.close(); return true; }
            } else {
                prevWasCR = false;
                line += ch;
            }
        }
    }

    // Trailing line with no terminator
    if (!line.empty()) {
        lineNumber++;
        callback(line, lineNumber, filePath);
    }

    file.close();
    return true;
}

void FileReader::reportProgress() {
    if (!m_progress || m_totalBytes == 0) return;
    // Throttle: report at most ~200 times (every 0.5%)
    if (m_bytesRead - m_lastReported < m_totalBytes / 200 + 1) return;
    m_lastReported = m_bytesRead;
    m_progress(m_bytesRead, m_totalBytes);
}

bool FileReader::readFolder(const std::string& folderPath, bool recursive,
                            LineCallback callback, ProgressCallback progress) {
    m_lastError.clear();

    std::vector<std::string> files = getLogFiles(folderPath, recursive);
    if (files.empty()) {
        m_lastError = "No log files found in: " + folderPath;
        return false;
    }

    // Set up progress tracking
    m_progress     = progress;
    m_totalBytes   = totalSize(files);
    m_bytesRead    = 0;
    m_lastReported = 0;

    for (const auto& file : files) {
        if (!readFile(file, callback)) {
            m_progress = nullptr;
            return false;
        }
    }

    // Final 100% tick
    if (m_progress) m_progress(m_totalBytes, m_totalBytes);
    m_progress = nullptr;
    return true;
}

} // namespace LogAnalyzer

// Made with Bob
