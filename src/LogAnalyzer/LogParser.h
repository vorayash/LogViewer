#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include "DataModels.h"
#include <regex>
#include <string>

namespace LogAnalyzer {

class LogParser {
public:
    LogParser();
    ~LogParser();
    
    // Set regex pattern for parsing
    bool setRegexPattern(const std::string& pattern);
    
    // Set timestamp format (strptime format)
    void setTimestampFormat(const std::string& format);
    
    // Set capture group indices
    void setGroupIndices(int timestampIdx, int levelIdx, int messageIdx);
    
    // Parse a log line
    bool parseLine(const std::string& line, LogEntry& entry);
    
    // Get last error
    std::string getLastError() const { return m_lastError; }
    
private:
    std::regex m_regex;
    std::string m_timestampFormat;
    int m_timestampGroupIdx;
    int m_levelGroupIdx;
    int m_messageGroupIdx;
    std::string m_lastError;
    bool m_regexValid;
    
    // Parse timestamp string to time_t
    time_t parseTimestamp(const std::string& timestampStr);
    
    // Parse log level string to enum
    int parseLogLevel(const std::string& levelStr);
};

} // namespace LogAnalyzer

#endif // LOG_PARSER_H

// Made with Bob
