#ifndef FILTER_ENGINE_H
#define FILTER_ENGINE_H

#include "DataModels.h"
#include <vector>
#include <unordered_set>

namespace LogAnalyzer {

class FilterEngine {
public:
    FilterEngine();
    ~FilterEngine();
    
    // Filter logs based on criteria using indexes
    std::vector<int> filter(
        const std::vector<LogEntry>& logs,
        const LogIndexes& indexes,
        const FilterCriteria& criteria
    );
    
    // Aggregate filtered results
    AggregationResult aggregate(
        const std::vector<LogEntry>& logs,
        const std::vector<int>& filteredIndices,
        int timeBucketSize = 3600
    );
    
private:
    // Get log indices matching level filter
    std::unordered_set<int> filterByLevel(
        const LogIndexes& indexes,
        int level
    );
    
    // Get log indices matching keyword filter
    std::unordered_set<int> filterByKeywords(
        const LogIndexes& indexes,
        const std::vector<std::string>& keywords
    );
    
    // Get log indices matching time range filter
    std::unordered_set<int> filterByTimeRange(
        const std::vector<LogEntry>& logs,
        const LogIndexes& indexes,
        time_t startTime,
        time_t endTime
    );
    
    // Intersect multiple result sets
    std::vector<int> intersectResults(
        const std::vector<std::unordered_set<int>>& resultSets
    );
    
    // Get time bucket for timestamp
    time_t getTimeBucket(time_t timestamp, int bucketSize);
};

} // namespace LogAnalyzer

#endif // FILTER_ENGINE_H

// Made with Bob
