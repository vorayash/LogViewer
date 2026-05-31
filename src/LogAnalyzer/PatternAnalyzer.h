#ifndef PATTERN_ANALYZER_H
#define PATTERN_ANALYZER_H

#include "DataModels.h"
#include <vector>
#include <string>
#include <functional>

namespace LogAnalyzer {

// Groups similar log messages into recurring patterns by replacing variable
// tokens (numbers, IDs, IPs, hashes) with a single "*" wildcard.
//
// Uses a fast single-pass scanner (no std::regex) so it stays responsive even
// on very large logs with long JSON lines.
class PatternAnalyzer {
public:
    // Optional progress callback: receives 0-100 as analysis proceeds.
    using ProgressFn = std::function<void(int)>;

    PatternAnalyzer() = default;

    std::vector<LogPattern> analyze(
        const std::vector<LogEntry>& logs,
        const std::vector<int>&      indices,
        ProgressFn                   progress = nullptr
    );

private:
    // Replaces any whitespace-delimited token containing a digit with "*",
    // collapses consecutive wildcards, and only looks at the first
    // MAX_NORMALIZE_LEN characters for speed.
    std::string normalizeMessage(const std::string& msg) const;

    static const size_t MAX_NORMALIZE_LEN = 512;
};

} // namespace LogAnalyzer

#endif // PATTERN_ANALYZER_H

// Made with Bob
