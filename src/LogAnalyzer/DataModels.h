#ifndef DATA_MODELS_H
#define DATA_MODELS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <ctime>

namespace LogAnalyzer {

enum LogLevel {
    LOG_TRACE   = 0,
    LOG_DEBUG   = 1,
    LOG_INFO    = 2,
    LOG_WARN    = 3,
    LOG_ERROR   = 4,
    LOG_FATAL   = 5,
    LOG_LEVEL_COUNT = 6,
    LOG_UNKNOWN = 99
};

struct LogEntry {
    time_t      timestamp = 0;
    int         level     = LOG_UNKNOWN;
    std::string source;     // optional captured group (thread, component, etc.)
    std::string message;
    std::string rawLine;

    LogEntry() = default;
    LogEntry(time_t ts, int lvl, const std::string& msg, const std::string& raw = "")
        : timestamp(ts), level(lvl), message(msg), rawLine(raw) {}
};

struct FilterCriteria {
    int  level      = -1;       // -1 = all
    std::vector<std::string> keywords;
    time_t startTime = 0;       // 0 = no limit
    time_t endTime   = 0;
    std::string searchText;     // full-text / regex search
    bool searchIsRegex = false;
};

// Per-time-bucket breakdown by level (for histogram multi-series)
struct BucketData {
    int counts[LOG_LEVEL_COUNT] = {};
    int total = 0;

    void add(int lvl) {
        if (lvl >= 0 && lvl < LOG_LEVEL_COUNT) counts[lvl]++;
        total++;
    }
};

struct AggregationResult {
    std::map<time_t, BucketData> timelineBuckets; // time_t -> per-level counts
    std::map<int, int>           levelCounts;      // level  -> total count
    int totalLogs = 0;

    // Helper so old callers (ChartGenerator) still work
    std::map<time_t, int> getTimelineCounts() const {
        std::map<time_t, int> m;
        for (const auto& p : timelineBuckets) m[p.first] = p.second.total;
        return m;
    }
};

// A recurring message pattern (output of PatternAnalyzer)
struct LogPattern {
    std::string pattern;   // normalised template, e.g. "Connection to * failed after * ms"
    std::string sample;    // a real message that produced this pattern
    int         count     = 0;
    time_t      firstSeen = 0;
    time_t      lastSeen  = 0;
    std::vector<int> logIndices;
};

// A detected exception / stack-trace group (output of ExceptionDetector)
struct LogException {
    std::string exceptionType; // e.g. "NullPointerException"
    std::string message;       // exception message
    std::string stackTrace;    // first full trace seen
    int         count     = 0;
    time_t      firstSeen = 0;
    time_t      lastSeen  = 0;
    std::vector<int> logIndices;
};

// Multi-dimensional indexes built by IndexEngine
struct LogIndexes {
    std::unordered_map<int, std::vector<int>>         levelIndex;
    std::unordered_map<std::string, std::vector<int>> keywordIndex;
    std::map<time_t, std::vector<int>>                timeIndex;

    void clear() {
        levelIndex.clear();
        keywordIndex.clear();
        timeIndex.clear();
    }
};

struct LogConfig {
    std::string folderPath;
    std::string regexPattern;
    std::string timestampFormat;
    bool recursive           = false;
    int  timestampGroupIndex = 1;
    int  levelGroupIndex     = 2;
    int  messageGroupIndex   = 3;
    int  sourceGroupIndex    = 0;  // 0 = not captured
};

} // namespace LogAnalyzer

#endif // DATA_MODELS_H

// Made with Bob
