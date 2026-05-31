#include "PatternAnalyzer.h"
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace LogAnalyzer {

std::string PatternAnalyzer::normalizeMessage(const std::string& msg) const {
    const size_t n = std::min(msg.size(), MAX_NORMALIZE_LEN);
    std::string out;
    out.reserve(n);

    size_t i = 0;
    bool lastStar = false;

    while (i < n) {
        unsigned char c = (unsigned char)msg[i];

        // Collapse runs of whitespace into a single space
        if (std::isspace(c)) {
            if (!out.empty() && out.back() != ' ') out += ' ';
            i++;
            lastStar = false;
            continue;
        }

        // Read one whitespace-delimited token
        size_t start = i;
        bool hasDigit = false;
        while (i < n && !std::isspace((unsigned char)msg[i])) {
            if (std::isdigit((unsigned char)msg[i])) hasDigit = true;
            i++;
        }

        if (hasDigit) {
            // Variable token (number / id / ip / hash / timestamp) -> single "*"
            if (!lastStar) { out += '*'; lastStar = true; }
        } else {
            out.append(msg, start, i - start);
            lastStar = false;
        }
    }

    // Trim a trailing space
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::vector<LogPattern> PatternAnalyzer::analyze(
    const std::vector<LogEntry>& logs,
    const std::vector<int>&      indices,
    ProgressFn                   progress
) {
    struct Entry {
        int count = 0;
        time_t first = 0, last = 0;
        std::string sample;
        std::vector<int> idx;
    };
    std::unordered_map<std::string, Entry> table;
    table.reserve(4096);

    const size_t total = indices.size();
    const size_t step  = total > 0 ? (total / 100 + 1) : 1;  // ~100 progress ticks

    for (size_t k = 0; k < total; k++) {
        int i = indices[k];
        if (i < 0 || i >= (int)logs.size()) continue;
        const LogEntry& e = logs[i];
        if (e.message.empty()) continue;

        std::string key = normalizeMessage(e.message);
        if (key.empty()) continue;

        auto& entry = table[key];
        entry.count++;
        if (entry.count == 1) {
            entry.sample = e.message;
            entry.first  = e.timestamp;
            entry.last   = e.timestamp;
        } else {
            if (e.timestamp < entry.first) entry.first = e.timestamp;
            if (e.timestamp > entry.last)  entry.last  = e.timestamp;
        }
        if ((int)entry.idx.size() < 1000) entry.idx.push_back(i);

        if (progress && (k % step) == 0)
            progress((int)((k * 100) / (total ? total : 1)));
    }

    if (progress) progress(100);

    std::vector<LogPattern> result;
    result.reserve(table.size());
    for (auto& kv : table) {
        LogPattern p;
        p.pattern    = kv.first;
        p.sample     = kv.second.sample;
        p.count      = kv.second.count;
        p.firstSeen  = kv.second.first;
        p.lastSeen   = kv.second.last;
        p.logIndices = std::move(kv.second.idx);
        result.push_back(std::move(p));
    }

    std::sort(result.begin(), result.end(),
        [](const LogPattern& a, const LogPattern& b){ return a.count > b.count; });

    return result;
}

} // namespace LogAnalyzer

// Made with Bob
