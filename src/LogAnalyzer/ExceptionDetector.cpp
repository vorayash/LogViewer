#include "ExceptionDetector.h"
#include <unordered_map>
#include <algorithm>

namespace LogAnalyzer {

ExceptionDetector::ExceptionDetector()
    : m_rexJavaException  (R"(([A-Za-z][\w$.]*Exception[^:]*):?\s*(.*))")
    , m_rexDotNetException(R"(([A-Za-z][\w.]*Exception[^:]*):?\s*(.*))")
    , m_rexPythonException(R"(([\w.]+Error|Traceback \(most recent call last\)):?\s*(.*))")
    , m_rexJavaFrame      (R"(\s*at\s+[\w$.]+\.[\w$<>]+\(.*\))")
    , m_rexDotNetFrame    (R"(\s+at\s+[\w.<> ]+\(.*\)\s*(in\s+\S+)?)")
{}

bool ExceptionDetector::isExceptionHeader(const std::string& msg) const {
    return std::regex_search(msg, m_rexJavaException)
        || std::regex_search(msg, m_rexPythonException);
}

bool ExceptionDetector::isStackFrame(const std::string& msg) const {
    return std::regex_search(msg, m_rexJavaFrame)
        || std::regex_search(msg, m_rexDotNetFrame);
}

std::string ExceptionDetector::extractType(const std::string& msg) const {
    std::smatch m;
    if (std::regex_search(msg, m, m_rexJavaException)   && m.size() > 1) return m[1].str();
    if (std::regex_search(msg, m, m_rexPythonException) && m.size() > 1) return m[1].str();
    return "Exception";
}

std::string ExceptionDetector::extractMessage(const std::string& msg) const {
    std::smatch m;
    if (std::regex_search(msg, m, m_rexJavaException)   && m.size() > 2) return m[2].str();
    if (std::regex_search(msg, m, m_rexPythonException) && m.size() > 2) return m[2].str();
    return msg;
}

std::vector<LogException> ExceptionDetector::detect(
    const std::vector<LogEntry>& logs,
    const std::vector<int>&      indices
) {
    // key = exceptionType + "::" + short message (first 80 chars)
    struct Group {
        LogException ex;
    };
    std::unordered_map<std::string, Group> table;

    // We walk indices in order; for each exception header we open a group,
    // and consecutive stack-frame lines are appended to the last open group.
    std::string lastKey;
    int         lastOrigIdx = -1;

    for (int i : indices) {
        if (i < 0 || i >= (int)logs.size()) continue;
        const LogEntry& e = logs[i];
        const std::string& msg = e.message.empty() ? e.rawLine : e.message;

        if (isExceptionHeader(msg)) {
            std::string type = extractType(msg);
            std::string exMsg = extractMessage(msg);
            std::string shortMsg = exMsg.size() > 80 ? exMsg.substr(0, 80) : exMsg;
            std::string key = type + "::" + shortMsg;

            auto& grp = table[key];
            if (grp.ex.count == 0) {
                grp.ex.exceptionType = type;
                grp.ex.message       = exMsg;
                grp.ex.stackTrace    = msg;
                grp.ex.firstSeen     = e.timestamp;
                grp.ex.lastSeen      = e.timestamp;
            } else {
                if (e.timestamp < grp.ex.firstSeen) grp.ex.firstSeen = e.timestamp;
                if (e.timestamp > grp.ex.lastSeen)  grp.ex.lastSeen  = e.timestamp;
            }
            grp.ex.count++;
            if ((int)grp.ex.logIndices.size() < 500) grp.ex.logIndices.push_back(i);

            lastKey     = key;
            lastOrigIdx = i;

        } else if (isStackFrame(msg) && !lastKey.empty() && i == lastOrigIdx + 1) {
            // Attach frame to the currently open exception group
            auto it = table.find(lastKey);
            if (it != table.end() && it->second.ex.stackTrace.size() < 2000) {
                it->second.ex.stackTrace += "\n" + msg;
            }
            lastOrigIdx = i;
        } else {
            lastKey.clear();
            lastOrigIdx = -1;
        }
    }

    std::vector<LogException> result;
    result.reserve(table.size());
    for (auto& kv : table) result.push_back(std::move(kv.second.ex));

    std::sort(result.begin(), result.end(),
        [](const LogException& a, const LogException& b){ return a.count > b.count; });

    return result;
}

} // namespace LogAnalyzer

// Made with Bob
