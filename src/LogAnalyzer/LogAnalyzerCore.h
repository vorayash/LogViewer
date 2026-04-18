#ifndef LOG_ANALYZER_CORE_H
#define LOG_ANALYZER_CORE_H

#include "DataModels.h"
#include "FileReader.h"
#include "LogParser.h"
#include "IndexEngine.h"
#include "FilterEngine.h"
#include <vector>
#include <string>

namespace LogAnalyzer {

// Main orchestrator class
class LogAnalyzerCore {
public:
    LogAnalyzerCore();
    ~LogAnalyzerCore();
    
    // Configure and process logs
    bool configure(const LogConfig& config);
    bool processLogs();
    
    // Filtering and aggregation
    std::vector<int> filterLogs(const FilterCriteria& criteria);
    AggregationResult aggregateResults(const std::vector<int>& filteredIndices);
    
    // Get results
    const std::vector<LogEntry>& getLogs() const { return m_logs; }
    int getTotalLogCount() const { return static_cast<int>(m_logs.size()); }
    
    // Get last error
    std::string getLastError() const { return m_lastError; }
    
    // Clear all data
    void clear();
    
private:
    LogConfig m_config;
    std::vector<LogEntry> m_logs;
    
    FileReader m_fileReader;
    LogParser m_parser;
    IndexEngine m_indexEngine;
    FilterEngine m_filterEngine;
    
    std::string m_lastError;
    
    // Process callback for file reader
    bool processLine(const std::string& line, int lineNumber, const std::string& fileName);
};

} // namespace LogAnalyzer

#endif // LOG_ANALYZER_CORE_H

// Made with Bob
