#ifndef FILE_READER_H
#define FILE_READER_H

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace LogAnalyzer {

class FileReader {
public:
    FileReader();
    ~FileReader();
    
    // Callback for each line read: (lineContent, lineNumber, fileName) -> bool (continue reading)
    using LineCallback = std::function<bool(const std::string&, int, const std::string&)>;

    // Progress callback: (bytesReadSoFar, totalBytes). Throttled by the reader.
    using ProgressCallback = std::function<void(unsigned long long, unsigned long long)>;

    // Filename mask (e.g. "*.txt" or "*.log;*.txt"). Empty = match all files.
    void setFileFilter(const std::string& filter) { m_filter = filter; }

    // Read all log files from folder
    bool readFolder(const std::string& folderPath, bool recursive,
                    LineCallback callback, ProgressCallback progress = nullptr);

    // Get list of log files in folder
    std::vector<std::string> getLogFiles(const std::string& folderPath, bool recursive);

    // Read single file line by line
    bool readFile(const std::string& filePath, LineCallback callback);
    
    // Get last error message
    std::string getLastError() const { return m_lastError; }
    
private:
    std::string m_lastError;
    std::string m_filter = "*.log;*.txt;*.out";  // filename mask(s)

    // Progress tracking (set up by readFolder, used inside readFile)
    ProgressCallback   m_progress     = nullptr;
    unsigned long long m_totalBytes   = 0;
    unsigned long long m_bytesRead    = 0;
    unsigned long long m_lastReported = 0;
    void reportProgress();

    // Helper to check if file is a log file
    bool isLogFile(const std::string& filename);
    
    // Recursive directory traversal
    void findLogFilesRecursive(const std::string& path, std::vector<std::string>& files);

    // Total size in bytes of a list of files
    unsigned long long totalSize(const std::vector<std::string>& files) const;
};

} // namespace LogAnalyzer

#endif // FILE_READER_H

// Made with Bob
