#ifndef PATTERN_ANALYZER_H
#define PATTERN_ANALYZER_H

#include "DataModels.h"
#include <vector>
#include <string>
#include <regex>

namespace LogAnalyzer {

// Groups similar log messages into recurring patterns by replacing variable
// tokens (numbers, IDs, IPs, paths) with wildcards.
class PatternAnalyzer {
public:
    PatternAnalyzer();

    // Analyse the given subset of logs and return patterns sorted by count desc.
    std::vector<LogPattern> analyze(
        const std::vector<LogEntry>& logs,
        const std::vector<int>&      indices
    );

private:
    std::string normalizeMessage(const std::string& msg) const;

    // Pre-compiled regexes for variable token detection
    std::regex m_rexUuid;
    std::regex m_rexHex;
    std::regex m_rexIp;
    std::regex m_rexNumber;
    std::regex m_rexPath;
    std::regex m_rexQuoted;
};

} // namespace LogAnalyzer

#endif // PATTERN_ANALYZER_H

// Made with Bob
