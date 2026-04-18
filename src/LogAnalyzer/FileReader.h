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
    
    // Read all log files from folder
    bool readFolder(const std::string& folderPath, bool recursive, LineCallback callback);
    
    // Get list of log files in folder
    std::vector<std::string> getLogFiles(const std::string& folderPath, bool recursive);
    
    // Read single file line by line
    bool readFile(const std::string& filePath, LineCallback callback);
    
    // Get last error message
    std::string getLastError() const { return m_lastError; }
    
private:
    std::string m_lastError;
    
    // Helper to check if file is a log file
    bool isLogFile(const std::string& filename);
    
    // Recursive directory traversal
    void findLogFilesRecursive(const std::string& path, std::vector<std::string>& files);
};

} // namespace LogAnalyzer

#endif // FILE_READER_H

// Made with Bob
