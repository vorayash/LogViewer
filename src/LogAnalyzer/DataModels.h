#ifndef DATA_MODELS_H
#define DATA_MODELS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <ctime>

namespace LogAnalyzer {

// Log levels
enum LogLevel {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARN = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5,
    LOG_UNKNOWN = 99
};

// Main log entry structure
struct LogEntry {
    time_t timestamp;
    int level;
    std::string message;
    std::string rawLine;  // Original log line
    
    LogEntry() : timestamp(0), level(LOG_UNKNOWN) {}
    LogEntry(time_t ts, int lvl, const std::string& msg, const std::string& raw = "")
        : timestamp(ts), level(lvl), message(msg), rawLine(raw) {}
};

// Filter criteria
struct FilterCriteria {
    int level;              // -1 means all levels
    std::vector<std::string> keywords;
    time_t startTime;       // 0 means no start limit
    time_t endTime;         // 0 means no end limit
    
    FilterCriteria() : level(-1), startTime(0), endTime(0) {}
};

// Aggregation result
struct AggregationResult {
    std::map<time_t, int> timelineCounts;  // time bucket -> count
    std::map<int, int> levelCounts;        // level -> count
    int totalLogs;
    
    AggregationResult() : totalLogs(0) {}
};

// Index structures
struct LogIndexes {
    // Level index: level -> vector of log indices
    std::unordered_map<int, std::vector<int>> levelIndex;
    
    // Keyword index: keyword -> vector of log indices
    std::unordered_map<std::string, std::vector<int>> keywordIndex;
    
    // Time bucket index: time bucket -> vector of log indices
    std::map<time_t, std::vector<int>> timeIndex;
    
    void clear() {
        levelIndex.clear();
        keywordIndex.clear();
        timeIndex.clear();
    }
};

// Configuration for log parsing
struct LogConfig {
    std::string folderPath;
    std::string regexPattern;
    std::string timestampFormat;
    bool recursive;
    int timestampGroupIndex;
    int levelGroupIndex;
    int messageGroupIndex;
    
    LogConfig() 
        : recursive(false)
        , timestampGroupIndex(1)
        , levelGroupIndex(2)
        , messageGroupIndex(3) {}
};

} // namespace LogAnalyzer

#endif // DATA_MODELS_H

// Made with Bob
