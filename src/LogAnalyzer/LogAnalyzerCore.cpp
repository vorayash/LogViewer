#include "LogAnalyzerCore.h"
#include <functional>

namespace LogAnalyzer {

LogAnalyzerCore::LogAnalyzerCore() {
}

LogAnalyzerCore::~LogAnalyzerCore() {
}

void LogAnalyzerCore::clear() {
    m_logs.clear();
    m_indexEngine.clear();
    m_lastError.clear();
}

bool LogAnalyzerCore::configure(const LogConfig& config) {
    m_config = config;
    m_lastError.clear();
    
    // Validate configuration
    if (m_config.folderPath.empty()) {
        m_lastError = "Folder path is empty";
        return false;
    }
    
    if (m_config.regexPattern.empty()) {
        m_lastError = "Regex pattern is empty";
        return false;
    }
    
    // Configure parser
    if (!m_parser.setRegexPattern(m_config.regexPattern)) {
        m_lastError = "Invalid regex pattern: " + m_parser.getLastError();
        return false;
    }
    
    m_parser.setTimestampFormat(m_config.timestampFormat);
    m_parser.setGroupIndices(
        m_config.timestampGroupIndex,
        m_config.levelGroupIndex,
        m_config.messageGroupIndex
    );
    
    return true;
}

bool LogAnalyzerCore::processLine(const std::string& line, int /*lineNumber*/, const std::string& /*fileName*/) {
    LogEntry entry;
    
    // Try to parse the line
    if (m_parser.parseLine(line, entry)) {
        m_logs.push_back(entry);
    }
    // If parsing fails, we continue (some lines might not match the pattern)
    
    return true;  // Continue reading
}

bool LogAnalyzerCore::processLogs() {
    clear();
    
    // Read and parse all log files
    auto callback = std::bind(&LogAnalyzerCore::processLine, this,
                             std::placeholders::_1,
                             std::placeholders::_2,
                             std::placeholders::_3);
    
    if (!m_fileReader.readFolder(m_config.folderPath, m_config.recursive, callback)) {
        m_lastError = "Failed to read logs: " + m_fileReader.getLastError();
        return false;
    }
    
    if (m_logs.empty()) {
        m_lastError = "No logs matched the pattern";
        return false;
    }
    
    // Build indexes
    m_indexEngine.buildIndexes(m_logs);
    
    return true;
}

std::vector<int> LogAnalyzerCore::filterLogs(const FilterCriteria& criteria) {
    return m_filterEngine.filter(m_logs, m_indexEngine.getIndexes(), criteria);
}

AggregationResult LogAnalyzerCore::aggregateResults(const std::vector<int>& filteredIndices) {
    return m_filterEngine.aggregate(m_logs, filteredIndices, 3600);  // 1 hour buckets
}

} // namespace LogAnalyzer

// Made with Bob
