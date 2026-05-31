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
    m_exceptions.clear();
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

    auto callback = [this](const std::string& line, int n, const std::string& f) {
        return processLine(line, n, f);
    };

    if (!m_fileReader.readFolder(m_config.folderPath, m_config.recursive, callback)) {
        m_lastError = "Failed to read logs: " + m_fileReader.getLastError();
        return false;
    }

    if (m_logs.empty()) { m_lastError = "No log lines matched the pattern"; return false; }

    if (cb) cb(80, "Building indexes...");
    m_indexEngine.buildIndexes(m_logs);

    if (cb) cb(100, "Done");
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

void LogAnalyzerCore::analyzePatterns(const std::vector<int>& indices) {
    m_patterns = m_patternAnalyzer.analyze(m_logs, indices);
}

void LogAnalyzerCore::analyzeExceptions(const std::vector<int>& indices) {
    m_exceptions = m_exceptionDetector.detect(m_logs, indices);
}

} // namespace LogAnalyzer

// Made with Bob
