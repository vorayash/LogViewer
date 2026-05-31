#include "PatternAnalyzer.h"
#include <unordered_map>
#include <algorithm>

namespace LogAnalyzer {

PatternAnalyzer::PatternAnalyzer()
    : m_rexUuid  (R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})")
    , m_rexHex   (R"(0x[0-9a-fA-F]+)")
    , m_rexIp    (R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)")
    , m_rexNumber(R"(\b\d+(\.\d+)?\b)")
    , m_rexPath  (R"([A-Za-z]:\\[^\s,;]+|/[^\s,;]+)")
    , m_rexQuoted(R"("[^"]{3,}")")
{}

std::string PatternAnalyzer::normalizeMessage(const std::string& msg) const {
    std::string s = msg;
    s = std::regex_replace(s, m_rexUuid,   "*");
    s = std::regex_replace(s, m_rexHex,    "*");
    s = std::regex_replace(s, m_rexIp,     "*");
    s = std::regex_replace(s, m_rexPath,   "*");
    s = std::regex_replace(s, m_rexQuoted, "*");
    s = std::regex_replace(s, m_rexNumber, "*");
    // Collapse consecutive wildcards
    std::regex multi(R"(\*(\s*\*)+)");
    s = std::regex_replace(s, multi, "*");
    return s;
}

std::vector<LogPattern> PatternAnalyzer::analyze(
    const std::vector<LogEntry>& logs,
    const std::vector<int>&      indices
) {
    // pattern -> {count, first, last, sample, indices}
    struct Entry {
        int count = 0;
        time_t first = 0, last = 0;
        std::string sample;
        std::vector<int> idx;
    };
    std::unordered_map<std::string, Entry> table;
    table.reserve(std::min((int)indices.size(), 50000));

    for (int i : indices) {
        if (i < 0 || i >= (int)logs.size()) continue;
        const LogEntry& e = logs[i];
        if (e.message.empty()) continue;

        std::string key = normalizeMessage(e.message);
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
    }

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
