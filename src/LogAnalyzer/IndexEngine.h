#ifndef INDEX_ENGINE_H
#define INDEX_ENGINE_H

#include "DataModels.h"
#include "Tokenizer.h"
#include <vector>

namespace LogAnalyzer {

class IndexEngine {
public:
    IndexEngine();
    ~IndexEngine();
    
    // Build indexes from log entries
    void buildIndexes(const std::vector<LogEntry>& logs);
    
    // Clear all indexes
    void clear();
    
    // Get indexes
    const LogIndexes& getIndexes() const { return m_indexes; }
    
    // Set time bucket size in seconds (default: 3600 = 1 hour)
    void setTimeBucketSize(int seconds);
    
private:
    LogIndexes m_indexes;
    Tokenizer m_tokenizer;
    int m_timeBucketSize;  // in seconds
    
    // Get time bucket for a timestamp
    time_t getTimeBucket(time_t timestamp);
    
    // Build level index
    void buildLevelIndex(const std::vector<LogEntry>& logs);
    
    // Build keyword index
    void buildKeywordIndex(const std::vector<LogEntry>& logs);
    
    // Build time index
    void buildTimeIndex(const std::vector<LogEntry>& logs);
};

} // namespace LogAnalyzer

#endif // INDEX_ENGINE_H

// Made with Bob
