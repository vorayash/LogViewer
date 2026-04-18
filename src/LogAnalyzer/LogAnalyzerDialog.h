#ifndef LOG_ANALYZER_DIALOG_H
#define LOG_ANALYZER_DIALOG_H

#include "../DockingFeature/StaticDialog.h"
#include "../DockingFeature/DockingDlgInterface.h"
#include "LogAnalyzerCore.h"
#include "resource.h"
#include <string>

namespace LogAnalyzer {

class LogAnalyzerDialog : public DockingDlgInterface {
public:
    LogAnalyzerDialog();
    ~LogAnalyzerDialog();
    
    void init(HINSTANCE hInst, HWND parent);
    void display(bool toShow = true);
    
    virtual void setParent(HWND parent2set) {
        _hParent = parent2set;
    }
    
protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);
    
private:
    LogAnalyzerCore m_core;
    std::string m_lastError;
    
    // UI Control IDs
    enum {
        IDC_FOLDER_PATH = 1001,
        IDC_BROWSE_FOLDER,
        IDC_REGEX_PATTERN,
        IDC_TIMESTAMP_FORMAT,
        IDC_RECURSIVE_CHECK,
        IDC_PROCESS_BUTTON,
        IDC_LEVEL_COMBO,
        IDC_KEYWORD_EDIT,
        IDC_FILTER_BUTTON,
        IDC_RESULTS_EDIT,
        IDC_STATUS_TEXT,
        IDC_WEBVIEW
    };
    
    // Control handles
    HWND m_hFolderPath;
    HWND m_hBrowseButton;
    HWND m_hRegexPattern;
    HWND m_hTimestampFormat;
    HWND m_hRecursiveCheck;
    HWND m_hProcessButton;
    HWND m_hLevelCombo;
    HWND m_hKeywordEdit;
    HWND m_hFilterButton;
    HWND m_hResultsEdit;
    HWND m_hStatusText;
    
    // Event handlers
    void onBrowseFolder();
    void onProcessLogs();
    void onFilterLogs();
    
    // UI helpers
    void createControls();
    void updateStatus(const std::string& message);
    void displayResults(const AggregationResult& result);
    void generateVisualization(const AggregationResult& result);
    
    // Get control text
    std::string getControlText(HWND hControl);
    void setControlText(HWND hControl, const std::string& text);
};

} // namespace LogAnalyzer

#endif // LOG_ANALYZER_DIALOG_H

// Made with Bob
