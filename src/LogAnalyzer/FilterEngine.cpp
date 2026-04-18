#include "FilterEngine.h"
#include <algorithm>

namespace LogAnalyzer {

FilterEngine::FilterEngine() {
}

FilterEngine::~FilterEngine() {
}

time_t FilterEngine::getTimeBucket(time_t timestamp, int bucketSize) {
    return (timestamp / bucketSize) * bucketSize;
}

std::unordered_set<int> FilterEngine::filterByLevel(
    const LogIndexes& indexes,
    int level
) {
    std::unordered_set<int> result;
    
    auto it = indexes.levelIndex.find(level);
    if (it != indexes.levelIndex.end()) {
        result.insert(it->second.begin(), it->second.end());
    }
    
    return result;
}

std::unordered_set<int> FilterEngine::filterByKeywords(
    const LogIndexes& indexes,
    const std::vector<std::string>& keywords
) {
    if (keywords.empty()) {
        return std::unordered_set<int>();
    }
    
    // Start with first keyword's results
    std::unordered_set<int> result;
    auto it = indexes.keywordIndex.find(keywords[0]);
    if (it != indexes.keywordIndex.end()) {
        result.insert(it->second.begin(), it->second.end());
    }
    
    // Intersect with remaining keywords (AND logic)
    for (size_t i = 1; i < keywords.size(); i++) {
        std::unordered_set<int> keywordResult;
        auto kit = indexes.keywordIndex.find(keywords[i]);
        if (kit != indexes.keywordIndex.end()) {
            keywordResult.insert(kit->second.begin(), kit->second.end());
        }
        
        // Keep only common elements
        std::unordered_set<int> intersection;
        for (int idx : result) {
            if (keywordResult.find(idx) != keywordResult.end()) {
                intersection.insert(idx);
            }
        }
        result = intersection;
    }
    
    return result;
}

std::unordered_set<int> FilterEngine::filterByTimeRange(
    const std::vector<LogEntry>& logs,
    const LogIndexes& indexes,
    time_t startTime,
    time_t endTime
) {
    std::unordered_set<int> result;
    
    // Iterate through time buckets in range
    for (const auto& bucket : indexes.timeIndex) {
        time_t bucketTime = bucket.first;
        
        // Check if bucket overlaps with time range
        if ((startTime == 0 || bucketTime >= startTime) &&
            (endTime == 0 || bucketTime <= endTime)) {
            
            // Add all logs in this bucket, but verify exact timestamp
            for (int idx : bucket.second) {
                if (idx < (int)logs.size()) {
                    time_t logTime = logs[idx].timestamp;
                    if ((startTime == 0 || logTime >= startTime) &&
                        (endTime == 0 || logTime <= endTime)) {
                        result.insert(idx);
                    }
                }
            }
        }
    }
    
    return result;
}

std::vector<int> FilterEngine::intersectResults(
    const std::vector<std::unordered_set<int>>& resultSets
) {
    if (resultSets.empty()) {
        return std::vector<int>();
    }
    
    // Start with first set
    std::unordered_set<int> intersection = resultSets[0];
    
    // Intersect with remaining sets
    for (size_t i = 1; i < resultSets.size(); i++) {
        std::unordered_set<int> newIntersection;
        for (int idx : intersection) {
            if (resultSets[i].find(idx) != resultSets[i].end()) {
                newIntersection.insert(idx);
            }
        }
        intersection = newIntersection;
    }
    
    // Convert to vector and sort
    std::vector<int> result(intersection.begin(), intersection.end());
    std::sort(result.begin(), result.end());
    
    return result;
}

std::vector<int> FilterEngine::filter(
    const std::vector<LogEntry>& logs,
    const LogIndexes& indexes,
    const FilterCriteria& criteria
) {
    std::vector<std::unordered_set<int>> resultSets;
    
    // Apply level filter
    if (criteria.level >= 0) {
        resultSets.push_back(filterByLevel(indexes, criteria.level));
    }
    
    // Apply keyword filter
    if (!criteria.keywords.empty()) {
        resultSets.push_back(filterByKeywords(indexes, criteria.keywords));
    }
    
    // Apply time range filter
    if (criteria.startTime > 0 || criteria.endTime > 0) {
        resultSets.push_back(filterByTimeRange(logs, indexes, criteria.startTime, criteria.endTime));
    }
    
    // If no filters applied, return all indices
    if (resultSets.empty()) {
        std::vector<int> allIndices;
        for (size_t i = 0; i < logs.size(); i++) {
            allIndices.push_back(static_cast<int>(i));
        }
        return allIndices;
    }
    
    // Intersect all result sets
    return intersectResults(resultSets);
}

AggregationResult FilterEngine::aggregate(
    const std::vector<LogEntry>& logs,
    const std::vector<int>& filteredIndices,
    int timeBucketSize
) {
    AggregationResult result;
    result.totalLogs = static_cast<int>(filteredIndices.size());
    
    // Aggregate by time and level
    for (int idx : filteredIndices) {
        if (idx >= 0 && idx < (int)logs.size()) {
            const LogEntry& entry = logs[idx];
            
            // Time bucket aggregation
            time_t bucket = getTimeBucket(entry.timestamp, timeBucketSize);
            result.timelineCounts[bucket]++;
            
            // Level aggregation
            result.levelCounts[entry.level]++;
        }
    }
    
    return result;
}

} // namespace LogAnalyzer

// Made with Bob
