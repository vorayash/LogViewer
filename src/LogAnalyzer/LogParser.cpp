#include "LogParser.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace LogAnalyzer {

LogParser::LogParser()
    : m_timestampFormat("%Y-%m-%d %H:%M:%S")
    , m_timestampGroupIdx(1)
    , m_levelGroupIdx(2)
    , m_messageGroupIdx(3)
    , m_regexValid(false) {
}

LogParser::~LogParser() {
}

bool LogParser::setRegexPattern(const std::string& pattern) {
    try {
        m_regex = std::regex(pattern);
        m_regexValid = true;
        m_lastError.clear();
        return true;
    } catch (const std::regex_error& e) {
        m_lastError = "Invalid regex pattern: " + std::string(e.what());
        m_regexValid = false;
        return false;
    }
}

void LogParser::setTimestampFormat(const std::string& format) {
    m_timestampFormat = format;
}

void LogParser::setGroupIndices(int timestampIdx, int levelIdx, int messageIdx) {
    m_timestampGroupIdx = timestampIdx;
    m_levelGroupIdx = levelIdx;
    m_messageGroupIdx = messageIdx;
}

int LogParser::parseLogLevel(const std::string& levelStr) {
    std::string upper = levelStr;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    
    if (upper.find("TRACE") != std::string::npos) return LogLevel::LOG_TRACE;
    if (upper.find("DEBUG") != std::string::npos) return LogLevel::LOG_DEBUG;
    if (upper.find("INFO") != std::string::npos) return LogLevel::LOG_INFO;
    if (upper.find("WARN") != std::string::npos) return LogLevel::LOG_WARN;
    if (upper.find("ERROR") != std::string::npos) return LogLevel::LOG_ERROR;
    if (upper.find("FATAL") != std::string::npos) return LogLevel::LOG_FATAL;
    
    return LogLevel::LOG_UNKNOWN;
}

time_t LogParser::parseTimestamp(const std::string& timestampStr) {
    std::tm tm = {};
    std::istringstream ss(timestampStr);
    
    // Try to parse using the format
    // Note: strptime is not available on Windows, so we use a simplified parser
    // For production, consider using a library like Howard Hinnant's date library
    
    // Simple parser for common format: YYYY-MM-DD HH:MM:SS
    if (m_timestampFormat == "%Y-%m-%d %H:%M:%S") {
        if (sscanf_s(timestampStr.c_str(), "%d-%d-%d %d:%d:%d",
                     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
            tm.tm_year -= 1900;  // Years since 1900
            tm.tm_mon -= 1;      // Months since January
            return mktime(&tm);
        }
    }
    // Format: DD/MMM/YYYY:HH:MM:SS (Apache style)
    else if (m_timestampFormat == "%d/%b/%Y:%H:%M:%S") {
        char month[4] = {0};
        if (sscanf_s(timestampStr.c_str(), "%d/%3s/%d:%d:%d:%d",
                     &tm.tm_mday, month, (unsigned)sizeof(month), &tm.tm_year,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
            
            // Convert month string to number
            const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            for (int i = 0; i < 12; i++) {
                if (_stricmp(month, months[i]) == 0) {
                    tm.tm_mon = i;
                    break;
                }
            }
            
            tm.tm_year -= 1900;
            return mktime(&tm);
        }
    }
    // Format: YYYY-MM-DDTHH:MM:SS (ISO 8601)
    else if (m_timestampFormat == "%Y-%m-%dT%H:%M:%S") {
        if (sscanf_s(timestampStr.c_str(), "%d-%d-%dT%d:%d:%d",
                     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
            tm.tm_year -= 1900;
            tm.tm_mon -= 1;
            return mktime(&tm);
        }
    }
    
    // If parsing fails, return 0
    return 0;
}

bool LogParser::parseLine(const std::string& line, LogEntry& entry) {
    if (!m_regexValid) {
        m_lastError = "Regex pattern not set or invalid";
        return false;
    }
    
    std::smatch matches;
    if (!std::regex_search(line, matches, m_regex)) {
        return false;  // Line doesn't match pattern
    }
    
    // Extract timestamp
    if (m_timestampGroupIdx > 0 && m_timestampGroupIdx < (int)matches.size()) {
        std::string timestampStr = matches[m_timestampGroupIdx].str();
        entry.timestamp = parseTimestamp(timestampStr);
    }
    
    // Extract log level
    if (m_levelGroupIdx > 0 && m_levelGroupIdx < (int)matches.size()) {
        std::string levelStr = matches[m_levelGroupIdx].str();
        entry.level = parseLogLevel(levelStr);
    }
    
    // Extract message
    if (m_messageGroupIdx > 0 && m_messageGroupIdx < (int)matches.size()) {
        entry.message = matches[m_messageGroupIdx].str();
    }
    
    // Store raw line
    entry.rawLine = line;
    
    return true;
}

} // namespace LogAnalyzer

// Made with Bob
