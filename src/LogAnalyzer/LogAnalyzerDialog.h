#ifndef LOG_ANALYZER_DIALOG_H
#define LOG_ANALYZER_DIALOG_H

#include "../DockingFeature/StaticDialog.h"
#include "../DockingFeature/DockingDlgInterface.h"
#include "LogAnalyzerCore.h"
#include "HistogramControl.h"
#include "resource.h"
#include <string>
#include <vector>
#include <commctrl.h>

namespace LogAnalyzer {

class LogAnalyzerDialog : public DockingDlgInterface {
public:
    LogAnalyzerDialog();
    ~LogAnalyzerDialog();

    void init(HINSTANCE hInst, HWND parent);
    void display(bool toShow = true);

    virtual void setParent(HWND parent2set) { _hParent = parent2set; }

protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

private:
    // ── Control IDs (must match RC) ─────────────────────────────────────────
    enum CtrlID {
        IDC_FOLDER_PATH    = 1001,
        IDC_BROWSE_FOLDER  = 1002,
        IDC_REGEX_PATTERN  = 1003,
        IDC_TIMESTAMP_FMT  = 1004,
        IDC_RECURSIVE      = 1005,
        IDC_PROCESS        = 1006,
        IDC_LEVEL_COMBO    = 1007,
        IDC_KEYWORDS       = 1008,
        IDC_FILTER         = 1009,
        IDC_LOGS_LIST      = 1010,
        IDC_STATUS         = 1011,
        IDC_BUCKET_UNIT    = 1012,
        IDC_BUCKET_VALUE   = 1013,
        IDC_SEARCH_EDIT    = 1014,
        IDC_SEARCH_REGEX   = 1015,
        IDC_HISTOGRAM      = 1017,
        IDC_TABS           = 1018,
        IDC_PATTERNS_LIST  = 1019,
        IDC_EXCEPTIONS_LIST= 1020,
    };

    // ── Core & state ────────────────────────────────────────────────────────
    LogAnalyzerCore  m_core;
    std::vector<int> m_filteredIndices;
    AggregationResult m_aggregation;
    bool             m_chartWindowOpened = false;
    bool             m_processing        = false;

    // Active tab: 0=Logs, 1=Patterns, 2=Exceptions
    int  m_activeTab   = 0;

    // Sort state for logs list
    int  m_sortCol     = 0;
    bool m_sortAsc     = true;

    // Histogram time-range selection
    HistogramSelection m_histSel;

    // ── Control handles ──────────────────────────────────────────────────────
    HWND m_hFolder, m_hBrowse;
    HWND m_hRegex,  m_hTsFmt;
    HWND m_hRecursive, m_hProcess;
    HWND m_hBucketUnit, m_hBucketVal;
    HWND m_hSearch, m_hSearchRegex;
    HWND m_hLevelCombo, m_hKeywords, m_hFilter;
    HWND m_hHistogram;
    HWND m_hTabs;
    HWND m_hLogsList;
    HWND m_hPatternsList;
    HWND m_hExceptionsList;
    HWND m_hStatus;

    // ── Setup ────────────────────────────────────────────────────────────────
    void createControls();
    void setupLogsColumns();
    void setupPatternsColumns();
    void setupExceptionsColumns();

    // ── Event handlers ───────────────────────────────────────────────────────
    void onBrowse();
    void onProcess();
    void onFilter();
    void onTabChange();
    void onHistogramRange();
    void onLogsDoubleClick(int itemIdx);
    void onPatternsDoubleClick(int itemIdx);
    void onExceptionsDoubleClick(int itemIdx);
    void onColumnClick(int col);
    void onContextMenu(HWND hList, POINT pt);

    // ── Data display ─────────────────────────────────────────────────────────
    void applyFilter();
    void refreshLogsVirtualList();
    void populatePatternsTab();
    void populateExceptionsTab();
    void updateHistogram();
    void updateStatus();

    // ── Virtual list (LVS_OWNERDATA) ─────────────────────────────────────────
    // Notepad++ is compiled Unicode, so the ListView sends the W variant.
    void onGetDispInfoW(NMLVDISPINFOW* pdi);

    // ── Detail popup ─────────────────────────────────────────────────────────
    void showDetailDialog(int logIdx);
    void showPatternDetail(int patIdx);
    void showExceptionDetail(int exIdx);

    // ── Chart (browser) ──────────────────────────────────────────────────────
    void generateBrowserChart();

    // ── Layout ───────────────────────────────────────────────────────────────
    void onResize(int W, int H);

    // ── Helpers ──────────────────────────────────────────────────────────────
    std::string getCtrlText(HWND h) const;
    void        setCtrlText(HWND h, const std::string& t);
    void        updateStatus(const std::string& msg);
    int         getBucketSeconds() const;
    bool        isChecked(HWND h) const;
    void        showListView(int tab);

    static std::string levelName(int lvl);
    static COLORREF    levelRowColor(int lvl);
    static std::string fmtTime(time_t t);
};

} // namespace LogAnalyzer

#endif // LOG_ANALYZER_DIALOG_H

// Made with Bob
