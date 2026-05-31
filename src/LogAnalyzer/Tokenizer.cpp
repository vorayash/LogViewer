#include "Tokenizer.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace LogAnalyzer {

Tokenizer::Tokenizer() : m_filterStopWords(true) {
    initDefaultStopWords();
}

Tokenizer::~Tokenizer() {
}

void Tokenizer::initDefaultStopWords() {
    // Common English stop words
    m_stopWords = {
        "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
        "has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
        "to", "was", "will", "with", "this", "but", "they", "have", "had",
        "what", "when", "where", "who", "which", "why", "how"
    };
}

void Tokenizer::setStopWords(const std::unordered_set<std::string>& stopWords) {
    m_stopWords = stopWords;
}

void Tokenizer::setFilterStopWords(bool filter) {
    m_filterStopWords = filter;
}

bool Tokenizer::isSeparator(char c) {
    return std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == '.' || c == ';' ||
           c == ':' || c == '!' || c == '?' || c == '(' || c == ')' ||
           c == '[' || c == ']' || c == '{' || c == '}' || c == '"' ||
           c == '\'' || c == '/' || c == '\\' || c == '-' || c == '_';
}

std::string Tokenizer::normalizeWord(const std::string& word) {
    std::string normalized;
    normalized.reserve(word.length());
    
    // Convert to lowercase and remove punctuation
    for (unsigned char c : word) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    
    return normalized;
}

std::vector<std::string> Tokenizer::tokenize(const std::string& message) {
    std::vector<std::string> tokens;
    std::string currentWord;
    
    for (char c : message) {
        if (isSeparator(c)) {
            if (!currentWord.empty()) {
                std::string normalized = normalizeWord(currentWord);
                
                // Filter out empty strings and stop words
                if (!normalized.empty()) {
                    if (!m_filterStopWords || m_stopWords.find(normalized) == m_stopWords.end()) {
                        tokens.push_back(normalized);
                    }
                }
                
                currentWord.clear();
            }
        } else {
            currentWord += c;
        }
    }
    
    // Don't forget the last word
    if (!currentWord.empty()) {
        std::string normalized = normalizeWord(currentWord);
        if (!normalized.empty()) {
            if (!m_filterStopWords || m_stopWords.find(normalized) == m_stopWords.end()) {
                tokens.push_back(normalized);
            }
        }
    }
    
    return tokens;
}

} // namespace LogAnalyzer

// Made with Bob
