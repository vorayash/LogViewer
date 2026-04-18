#include "IndexEngine.h"

namespace LogAnalyzer {

IndexEngine::IndexEngine() : m_timeBucketSize(3600) {  // Default: 1 hour buckets
}

IndexEngine::~IndexEngine() {
}

void IndexEngine::setTimeBucketSize(int seconds) {
    m_timeBucketSize = seconds;
}

time_t IndexEngine::getTimeBucket(time_t timestamp) {
    // Round down to nearest bucket
    return (timestamp / m_timeBucketSize) * m_timeBucketSize;
}

void IndexEngine::clear() {
    m_indexes.clear();
}

void IndexEngine::buildLevelIndex(const std::vector<LogEntry>& logs) {
    for (size_t i = 0; i < logs.size(); i++) {
        int level = logs[i].level;
        m_indexes.levelIndex[level].push_back(static_cast<int>(i));
    }
}

void IndexEngine::buildKeywordIndex(const std::vector<LogEntry>& logs) {
    for (size_t i = 0; i < logs.size(); i++) {
        // Tokenize the message
        std::vector<std::string> keywords = m_tokenizer.tokenize(logs[i].message);
        
        // Add log index to each keyword
        for (const auto& keyword : keywords) {
            m_indexes.keywordIndex[keyword].push_back(static_cast<int>(i));
        }
    }
}

void IndexEngine::buildTimeIndex(const std::vector<LogEntry>& logs) {
    for (size_t i = 0; i < logs.size(); i++) {
        time_t bucket = getTimeBucket(logs[i].timestamp);
        m_indexes.timeIndex[bucket].push_back(static_cast<int>(i));
    }
}

void IndexEngine::buildIndexes(const std::vector<LogEntry>& logs) {
    // Clear existing indexes
    clear();
    
    if (logs.empty()) {
        return;
    }
    
    // Build all indexes
    buildLevelIndex(logs);
    buildKeywordIndex(logs);
    buildTimeIndex(logs);
}

} // namespace LogAnalyzer

// Made with Bob
