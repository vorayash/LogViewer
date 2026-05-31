#include "LogAnalyzerCore.h"
#include <functional>

namespace LogAnalyzer {

LogAnalyzerCore::LogAnalyzerCore() : m_bucketSize(3600) {}

LogAnalyzerCore::~LogAnalyzerCore() {
    cancelAsync();
    if (m_hThread) {
        WaitForSingleObject(m_hThread, 5000);
        CloseHandle(m_hThread);
    }
}

void LogAnalyzerCore::clear() {
    m_logs.clear();
    m_patterns.clear();
    m_indexEngine.clear();
    m_lastError.clear();
}

void LogAnalyzerCore::setTimeBucketSize(int seconds) {
    m_bucketSize = seconds > 0 ? seconds : 3600;
    m_indexEngine.setTimeBucketSize(m_bucketSize);
}

bool LogAnalyzerCore::configure(const LogConfig& config) {
    m_config = config;
    m_lastError.clear();

    if (m_config.folderPath.empty()) { m_lastError = "Folder path is empty"; return false; }
    if (m_config.regexPattern.empty()) { m_lastError = "Regex pattern is empty"; return false; }

    if (!m_parser.setRegexPattern(m_config.regexPattern)) {
        m_lastError = "Invalid regex: " + m_parser.getLastError();
        return false;
    }
    m_parser.setTimestampFormat(m_config.timestampFormat);
    m_parser.setGroupIndices(m_config.timestampGroupIndex,
                             m_config.levelGroupIndex,
                             m_config.messageGroupIndex);
    return true;
}

bool LogAnalyzerCore::processLine(const std::string& line, int /*lineNum*/, const std::string& /*file*/) {
    if (m_cancel) return false;
    LogEntry entry;
    if (m_parser.parseLine(line, entry)) m_logs.push_back(entry);
    return true;
}

bool LogAnalyzerCore::processLogs(ProgressCallback cb) {
    clear();

    if (cb) cb(0, "Reading log files...");

    m_fileReader.setFileFilter(m_config.fileFilter);

    auto lineCb = [this](const std::string& line, int n, const std::string& f) {
        return processLine(line, n, f);
    };

    // Map byte-read progress onto 0-85% of the overall pipeline.
    FileReader::ProgressCallback progCb = nullptr;
    if (cb) {
        progCb = [cb](unsigned long long read, unsigned long long total) {
            int pct = total > 0 ? (int)((read * 85ULL) / total) : 0;
            cb(pct, "Reading & parsing logs...");
        };
    }

    if (!m_fileReader.readFolder(m_config.folderPath, m_config.recursive, lineCb, progCb)) {
        m_lastError = "Failed to read logs: " + m_fileReader.getLastError();
        return false;
    }

    if (m_logs.empty()) { m_lastError = "No log lines matched the pattern"; return false; }

    if (cb) cb(90, "Building indexes...");
    m_indexEngine.buildIndexes(m_logs);

    if (cb) cb(100, "Indexed");
    return true;
}

// ── Async processing ──────────────────────────────────────────────────────────

DWORD WINAPI LogAnalyzerCore::workerThreadProc(LPVOID param) {
    reinterpret_cast<LogAnalyzerCore*>(param)->workerBody();
    return 0;
}

void LogAnalyzerCore::workerBody() {
    bool ok = processLogs(m_progressCb);
    if (m_hNotify) PostMessage(m_hNotify, WM_LOGANALYZER_DONE, 0, ok ? TRUE : FALSE);
}

void LogAnalyzerCore::processLogsAsync(HWND hNotify, ProgressCallback cb) {
    cancelAsync();
    if (m_hThread) { CloseHandle(m_hThread); m_hThread = nullptr; }

    m_cancel     = false;
    m_hNotify    = hNotify;
    m_progressCb = cb;
    m_hThread    = CreateThread(NULL, 0, workerThreadProc, this, 0, NULL);
}

void LogAnalyzerCore::cancelAsync() {
    m_cancel = true;
}

// ── Filtering & aggregation ───────────────────────────────────────────────────

std::vector<int> LogAnalyzerCore::filterLogs(const FilterCriteria& criteria) {
    return m_filterEngine.filter(m_logs, m_indexEngine.getIndexes(), criteria);
}

AggregationResult LogAnalyzerCore::aggregate(const std::vector<int>& indices) {
    return m_filterEngine.aggregate(m_logs, indices, m_bucketSize);
}

void LogAnalyzerCore::analyzePatterns(const std::vector<int>& indices,
                                      PatternAnalyzer::ProgressFn progress) {
    m_patterns = m_patternAnalyzer.analyze(m_logs, indices, progress);
}

} // namespace LogAnalyzer

// Made with Bob
