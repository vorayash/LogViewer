#ifndef LOG_ANALYZER_CORE_H
#define LOG_ANALYZER_CORE_H

#include "DataModels.h"
#include "FileReader.h"
#include "LogParser.h"
#include "IndexEngine.h"
#include "FilterEngine.h"
#include "PatternAnalyzer.h"
#include <vector>
#include <string>
#include <functional>
#include <windows.h>

namespace LogAnalyzer {

// Callback fired on worker thread with progress [0..100] and status text.
// Return false to request cancellation.
using ProgressCallback = std::function<bool(int pct, const std::string& status)>;

class LogAnalyzerCore {
public:
    LogAnalyzerCore();
    ~LogAnalyzerCore();

    // ── Configuration & processing ──────────────────────────────────────────
    bool configure(const LogConfig& config);

    // Synchronous (blocks caller)
    bool processLogs(ProgressCallback cb = nullptr);

    // Asynchronous — posts WM_APP+10 to hNotify when done (lParam = success BOOL)
    void processLogsAsync(HWND hNotify, ProgressCallback cb = nullptr);
    void cancelAsync();

    // ── Querying ─────────────────────────────────────────────────────────────
    std::vector<int>     filterLogs  (const FilterCriteria& criteria);
    AggregationResult    aggregate   (const std::vector<int>& indices);

    void analyzePatterns  (const std::vector<int>& indices,
                           PatternAnalyzer::ProgressFn progress = nullptr);

    // ── Accessors ────────────────────────────────────────────────────────────
    const std::vector<LogEntry>&    getLogs      () const { return m_logs; }
    const std::vector<LogPattern>&  getPatterns  () const { return m_patterns; }
    int  getTotalLogCount() const { return (int)m_logs.size(); }
    std::string getLastError() const { return m_lastError; }

    void setTimeBucketSize(int seconds);
    void clear();

private:
    LogConfig        m_config;
    std::vector<LogEntry>     m_logs;
    std::vector<LogPattern>   m_patterns;

    FileReader      m_fileReader;
    LogParser       m_parser;
    IndexEngine     m_indexEngine;
    FilterEngine    m_filterEngine;
    PatternAnalyzer m_patternAnalyzer;

    int         m_bucketSize = 3600;
    std::string m_lastError;

    // Async worker
    HANDLE           m_hThread   = nullptr;
    volatile bool    m_cancel    = false;
    HWND             m_hNotify   = nullptr;
    ProgressCallback m_progressCb;

    bool processLine(const std::string& line, int lineNum, const std::string& file);

    static DWORD WINAPI workerThreadProc(LPVOID param);
    void workerBody();
};

} // namespace LogAnalyzer

#define WM_LOGANALYZER_DONE (WM_APP + 10)

#endif // LOG_ANALYZER_CORE_H

// Made with Bob
