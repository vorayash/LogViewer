#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <vector>
#include <unordered_set>

namespace LogAnalyzer {

class Tokenizer {
public:
    Tokenizer();
    ~Tokenizer();
    
    // Tokenize a message into keywords
    std::vector<std::string> tokenize(const std::string& message);
    
    // Set custom stop words
    void setStopWords(const std::unordered_set<std::string>& stopWords);
    
    // Enable/disable stop word filtering
    void setFilterStopWords(bool filter);
    
private:
    std::unordered_set<std::string> m_stopWords;
    bool m_filterStopWords;
    
    // Normalize a word (lowercase, remove punctuation)
    std::string normalizeWord(const std::string& word);
    
    // Check if character is word separator
    bool isSeparator(char c);
    
    // Initialize default stop words
    void initDefaultStopWords();
};

} // namespace LogAnalyzer

#endif // TOKENIZER_H

// Made with Bob
