#include "ChartGenerator.h"
#include <algorithm>

namespace LogAnalyzer {

std::string ChartGenerator::generateLevelDataJson(const AggregationResult& result) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"labels\": [";
    
    // Collect all levels that have counts
    std::vector<std::pair<LogLevel, int>> levelPairs;
    for (const auto& pair : result.levelCounts) {
        levelPairs.push_back(std::make_pair(static_cast<LogLevel>(pair.first), pair.second));
    }
    
    // Sort by level enum value for consistent ordering
    std::sort(levelPairs.begin(), levelPairs.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Generate labels
    for (size_t i = 0; i < levelPairs.size(); ++i) {
        json << "\"" << getLevelName(levelPairs[i].first) << "\"";
        if (i < levelPairs.size() - 1) json << ", ";
    }
    json << "],\n";
    
    // Generate values
    json << "  \"values\": [";
    for (size_t i = 0; i < levelPairs.size(); ++i) {
        json << levelPairs[i].second;
        if (i < levelPairs.size() - 1) json << ", ";
    }
    json << "],\n";
    
    // Add individual level counts for stats
    json << "  \"total\": " << result.totalLogs << ",\n";
    json << "  \"TRACE\": " << (result.levelCounts.count(LogLevel::LOG_TRACE) ? result.levelCounts.at(LogLevel::LOG_TRACE) : 0) << ",\n";
    json << "  \"DEBUG\": " << (result.levelCounts.count(LogLevel::LOG_DEBUG) ? result.levelCounts.at(LogLevel::LOG_DEBUG) : 0) << ",\n";
    json << "  \"INFO\": " << (result.levelCounts.count(LogLevel::LOG_INFO) ? result.levelCounts.at(LogLevel::LOG_INFO) : 0) << ",\n";
    json << "  \"WARN\": " << (result.levelCounts.count(LogLevel::LOG_WARN) ? result.levelCounts.at(LogLevel::LOG_WARN) : 0) << ",\n";
    json << "  \"ERROR\": " << (result.levelCounts.count(LogLevel::LOG_ERROR) ? result.levelCounts.at(LogLevel::LOG_ERROR) : 0) << ",\n";
    json << "  \"FATAL\": " << (result.levelCounts.count(LogLevel::LOG_FATAL) ? result.levelCounts.at(LogLevel::LOG_FATAL) : 0) << "\n";
    json << "}";
    
    return json.str();
}

std::string ChartGenerator::generateTimelineDataJson(const AggregationResult& result) {
    auto timelineCounts = result.getTimelineCounts();

    std::ostringstream json;
    json << "{\n";
    json << "  \"labels\": [";

    size_t count = 0;
    for (const auto& pair : timelineCounts) {
        json << "\"" << formatTime(pair.first) << "\"";
        if (++count < timelineCounts.size()) json << ", ";
    }
    json << "],\n";

    json << "  \"values\": [";
    count = 0;
    for (const auto& pair : timelineCounts) {
        json << pair.second;
        if (++count < timelineCounts.size()) json << ", ";
    }
    json << "],\n";

    json << "  \"bucketCount\": " << timelineCounts.size() << "\n";
    json << "}";

    return json.str();
}

std::string ChartGenerator::formatTime(time_t timestamp) {
    struct tm timeinfo;
    localtime_s(&timeinfo, &timestamp);
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string ChartGenerator::getLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::LOG_TRACE: return "TRACE";
        case LogLevel::LOG_DEBUG: return "DEBUG";
        case LogLevel::LOG_INFO: return "INFO";
        case LogLevel::LOG_WARN: return "WARN";
        case LogLevel::LOG_ERROR: return "ERROR";
        case LogLevel::LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

} // namespace LogAnalyzer

// Made with Bob
