#pragma once

#include "DataModels.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace LogAnalyzer {

class ChartGenerator {
public:
    // Generate JSON data for level distribution chart
    static std::string generateLevelDataJson(const AggregationResult& result);
    
    // Generate JSON data for timeline chart
    static std::string generateTimelineDataJson(const AggregationResult& result);
    
    // Helper to convert time_t to readable string
    static std::string formatTime(time_t timestamp);
    
    // Helper to get log level name
    static std::string getLevelName(LogLevel level);
};

} // namespace LogAnalyzer

// Made with Bob
