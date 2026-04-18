#include "LogAnalyzerDialog.h"
#include <shlobj.h>
#include <sstream>
#include <iomanip>

namespace LogAnalyzer {

LogAnalyzerDialog::LogAnalyzerDialog() 
    : DockingDlgInterface(IDD_LOG_ANALYZER_DIALOG) {
}

LogAnalyzerDialog::~LogAnalyzerDialog() {
}

void LogAnalyzerDialog::init(HINSTANCE hInst, HWND parent) {
    _hInst = hInst;
    _hParent = parent;
    DockingDlgInterface::init(hInst, parent);
}

void LogAnalyzerDialog::display(bool toShow) {
    DockingDlgInterface::display(toShow);
}

std::string LogAnalyzerDialog::getControlText(HWND hControl) {
    int len = GetWindowTextLengthA(hControl);
    if (len == 0) return "";
    
    std::string text(len + 1, '\0');
    GetWindowTextA(hControl, &text[0], len + 1);
    text.resize(len);
    return text;
}

void LogAnalyzerDialog::setControlText(HWND hControl, const std::string& text) {
    SetWindowTextA(hControl, text.c_str());
}

void LogAnalyzerDialog::createControls() {
    // Get control handles from dialog resource
    m_hFolderPath = GetDlgItem(_hSelf, IDC_FOLDER_PATH);
    m_hBrowseButton = GetDlgItem(_hSelf, IDC_BROWSE_FOLDER);
    m_hRegexPattern = GetDlgItem(_hSelf, IDC_REGEX_PATTERN);
    m_hTimestampFormat = GetDlgItem(_hSelf, IDC_TIMESTAMP_FORMAT);
    m_hRecursiveCheck = GetDlgItem(_hSelf, IDC_RECURSIVE_CHECK);
    m_hProcessButton = GetDlgItem(_hSelf, IDC_PROCESS_BUTTON);
    m_hLevelCombo = GetDlgItem(_hSelf, IDC_LEVEL_COMBO);
    m_hKeywordEdit = GetDlgItem(_hSelf, IDC_KEYWORD_EDIT);
    m_hFilterButton = GetDlgItem(_hSelf, IDC_FILTER_BUTTON);
    m_hResultsEdit = GetDlgItem(_hSelf, IDC_RESULTS_EDIT);
    m_hStatusText = GetDlgItem(_hSelf, IDC_STATUS_TEXT);
    
    // Set default timestamp format
    setControlText(m_hTimestampFormat, "%Y-%m-%d %H:%M:%S");
    
    // Populate level combo
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"All");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"TRACE");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"DEBUG");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"INFO");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"WARN");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"ERROR");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"FATAL");
    SendMessageA(m_hLevelCombo, CB_SETCURSEL, 0, 0);
}

void LogAnalyzerDialog::updateStatus(const std::string& message) {
    setControlText(m_hStatusText, message);
}

void LogAnalyzerDialog::onBrowseFolder() {
    BROWSEINFOA bi = {0};
    bi.hwndOwner = _hSelf;
    bi.lpszTitle = "Select Log Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            setControlText(m_hFolderPath, path);
        }
        CoTaskMemFree(pidl);
    }
}

void LogAnalyzerDialog::onProcessLogs() {
    updateStatus("Processing logs...");
    
    // Get configuration from UI
    LogConfig config;
    config.folderPath = getControlText(m_hFolderPath);
    config.regexPattern = getControlText(m_hRegexPattern);
    config.timestampFormat = getControlText(m_hTimestampFormat);
    config.recursive = (SendMessage(m_hRecursiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    // Configure core
    if (!m_core.configure(config)) {
        updateStatus("Error: " + m_core.getLastError());
        MessageBoxA(_hSelf, m_core.getLastError().c_str(), "Configuration Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Process logs
    if (!m_core.processLogs()) {
        updateStatus("Error: " + m_core.getLastError());
        MessageBoxA(_hSelf, m_core.getLastError().c_str(), "Processing Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Display initial results
    std::ostringstream oss;
    oss << "Total logs processed: " << m_core.getTotalLogCount();
    updateStatus(oss.str());
    
    // Auto-filter to show all
    onFilterLogs();
}

void LogAnalyzerDialog::onFilterLogs() {
    if (m_core.getTotalLogCount() == 0) {
        updateStatus("No logs loaded. Please process logs first.");
        return;
    }
    
    // Get filter criteria
    FilterCriteria criteria;
    
    // Level filter
    int levelIdx = (int)SendMessage(m_hLevelCombo, CB_GETCURSEL, 0, 0);
    criteria.level = (levelIdx == 0) ? -1 : (levelIdx - 1);  // 0 = All, 1 = TRACE, etc.
    
    // Keyword filter
    std::string keywords = getControlText(m_hKeywordEdit);
    if (!keywords.empty()) {
        // Split by space
        std::istringstream iss(keywords);
        std::string word;
        while (iss >> word) {
            criteria.keywords.push_back(word);
        }
    }
    
    // Apply filter
    std::vector<int> filtered = m_core.filterLogs(criteria);
    
    // Aggregate results
    AggregationResult result = m_core.aggregateResults(filtered);
    
    // Display results
    displayResults(result);
    
    std::ostringstream oss;
    oss << "Filtered: " << result.totalLogs << " / " << m_core.getTotalLogCount() << " logs";
    updateStatus(oss.str());
}

void LogAnalyzerDialog::displayResults(const AggregationResult& result) {
    std::ostringstream oss;
    
    oss << "=== Aggregation Results ===\n\n";
    oss << "Total Logs: " << result.totalLogs << "\n\n";
    
    oss << "--- By Level ---\n";
    const char* levelNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    for (const auto& pair : result.levelCounts) {
        if (pair.first >= 0 && pair.first < 6) {
            oss << levelNames[pair.first] << ": " << pair.second << "\n";
        }
    }
    
    oss << "\n--- Timeline (first 10 buckets) ---\n";
    int count = 0;
    for (const auto& pair : result.timelineCounts) {
        if (count++ >= 10) break;
        
        time_t t = pair.first;
        struct tm* tm_info = localtime(&t);
        char buffer[26];
        strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
        
        oss << buffer << ": " << pair.second << " logs\n";
    }
    
    setControlText(m_hResultsEdit, oss.str());
}

void LogAnalyzerDialog::generateVisualization(const AggregationResult& /*result*/) {
    // TODO: Implement WebView2 or HTML generation with Chart.js
    // For now, we display text results
}

INT_PTR CALLBACK LogAnalyzerDialog::run_dlgProc(UINT message, WPARAM wParam, LPARAM /*lParam*/) {
    switch (message) {
        case WM_INITDIALOG:
            createControls();
            return TRUE;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BROWSE_FOLDER:
                    onBrowseFolder();
                    return TRUE;
                    
                case IDC_PROCESS_BUTTON:
                    onProcessLogs();
                    return TRUE;
                    
                case IDC_FILTER_BUTTON:
                    onFilterLogs();
                    return TRUE;
            }
            break;
            
        case WM_SIZE:
        case WM_MOVE:
            break;
            
        case WM_DESTROY:
            break;
    }
    
    return FALSE;
}

} // namespace LogAnalyzer

// Made with Bob
