#define NOMINMAX
#include "LogAnalyzerDialog.h"
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#include <uxtheme.h>

namespace LogAnalyzer {

// ── Level metadata ────────────────────────────────────────────────────────────

std::string LogAnalyzerDialog::levelName(int lvl) {
    static const char* n[] = { "TRACE","DEBUG","INFO","WARN","ERROR","FATAL" };
    return (lvl >= 0 && lvl < 6) ? n[lvl] : "UNKNOWN";
}

COLORREF LogAnalyzerDialog::levelRowColor(int lvl) {
    switch (lvl) {
        case LOG_FATAL: return RGB(255, 200, 200);
        case LOG_ERROR: return RGB(255, 235, 235);
        case LOG_WARN:  return RGB(255, 250, 220);
        case LOG_DEBUG: return RGB(235, 245, 255);
        case LOG_TRACE: return RGB(245, 235, 255);
        default:        return RGB(255, 255, 255);
    }
}

std::string LogAnalyzerDialog::fmtTime(time_t t) {
    if (t == 0) return "(no time)";
    struct tm tm{};
    localtime_s(&tm, &t);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// ── Construction ──────────────────────────────────────────────────────────────

LogAnalyzerDialog::LogAnalyzerDialog()
    : DockingDlgInterface(IDD_LOG_ANALYZER_DIALOG)
    , m_hFolder(nullptr), m_hBrowse(nullptr)
    , m_hRegex(nullptr), m_hTsFmt(nullptr)
    , m_hRecursive(nullptr), m_hProcess(nullptr)
    , m_hFileFilter(nullptr)
    , m_hBucketUnit(nullptr), m_hBucketVal(nullptr)
    , m_hSearch(nullptr), m_hSearchRegex(nullptr)
    , m_hLevelCombo(nullptr), m_hKeywords(nullptr), m_hFilter(nullptr)
    , m_hHistogram(nullptr), m_hTabs(nullptr)
    , m_hLogsList(nullptr), m_hPatternsList(nullptr)
    , m_hStatus(nullptr) {
}

LogAnalyzerDialog::~LogAnalyzerDialog() {
    // Make sure the analysis worker has finished before we tear down.
    if (m_hAnalyzeThread) {
        WaitForSingleObject(m_hAnalyzeThread, 5000);
        CloseHandle(m_hAnalyzeThread);
        m_hAnalyzeThread = nullptr;
    }
}

void LogAnalyzerDialog::init(HINSTANCE hInst, HWND parent) {
    _hInst = hInst; _hParent = parent;
    DockingDlgInterface::init(hInst, parent);
}

void LogAnalyzerDialog::display(bool toShow) {
    DockingDlgInterface::display(toShow);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string LogAnalyzerDialog::getCtrlText(HWND h) const {
    if (!h) return "";
    int len = GetWindowTextLengthA(h);
    if (len == 0) return "";
    std::string s(len + 1, '\0');
    GetWindowTextA(h, &s[0], len + 1);
    s.resize(len);
    return s;
}

void LogAnalyzerDialog::setCtrlText(HWND h, const std::string& t) {
    if (h) SetWindowTextA(h, t.c_str());
}

void LogAnalyzerDialog::updateStatus(const std::string& msg) {
    setCtrlText(m_hStatus, msg);
}

bool LogAnalyzerDialog::isChecked(HWND h) const {
    return h && SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

int LogAnalyzerDialog::getBucketSeconds() const {
    int unit = (int)SendMessage(m_hBucketUnit, CB_GETCURSEL, 0, 0);
    int val  = 1;
    try { std::string s = getCtrlText(m_hBucketVal);
          if (!s.empty()) val = std::stoi(s); } catch (...) {}
    if (val <= 0) val = 1;
    if (unit == 1) return val * 60;
    if (unit == 2) return val * 3600;
    return val;
}

void LogAnalyzerDialog::showListView(int tab) {
    ShowWindow(m_hLogsList,       tab == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hPatternsList,   tab == 1 ? SW_SHOW : SW_HIDE);
}

// ── Control setup ─────────────────────────────────────────────────────────────

static LRESULT CALLBACK ListWhiteBkSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

void LogAnalyzerDialog::createControls() {
    m_hFolder      = GetDlgItem(_hSelf, IDC_FOLDER_PATH);
    m_hBrowse      = GetDlgItem(_hSelf, IDC_BROWSE_FOLDER);
    m_hRegex       = GetDlgItem(_hSelf, IDC_REGEX_PATTERN);
    m_hTsFmt       = GetDlgItem(_hSelf, IDC_TIMESTAMP_FMT);
    m_hRecursive   = GetDlgItem(_hSelf, IDC_RECURSIVE);
    m_hFileFilter  = GetDlgItem(_hSelf, IDC_FILE_FILTER);
    m_hProcess     = GetDlgItem(_hSelf, IDC_PROCESS);
    m_hBucketUnit  = GetDlgItem(_hSelf, IDC_BUCKET_UNIT);
    m_hBucketVal   = GetDlgItem(_hSelf, IDC_BUCKET_VALUE);
    m_hSearch      = GetDlgItem(_hSelf, IDC_SEARCH_EDIT);
    m_hSearchRegex = GetDlgItem(_hSelf, IDC_SEARCH_REGEX);
    m_hLevelCombo  = GetDlgItem(_hSelf, IDC_LEVEL_COMBO);
    m_hKeywords    = GetDlgItem(_hSelf, IDC_KEYWORDS);
    m_hFilter      = GetDlgItem(_hSelf, IDC_FILTER);
    m_hHistogram   = GetDlgItem(_hSelf, IDC_HISTOGRAM);
    m_hTabs        = GetDlgItem(_hSelf, IDC_TABS);
    m_hLogsList    = GetDlgItem(_hSelf, IDC_LOGS_LIST);
    m_hPatternsList    = GetDlgItem(_hSelf, IDC_PATTERNS_LIST);
    m_hStatus      = GetDlgItem(_hSelf, IDC_STATUS);

    // Defaults
    setCtrlText(m_hRegex, "(\\d{4}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}:\\d{2}).*?\\s(TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\\s+(.*)");
    setCtrlText(m_hTsFmt, "%Y-%m-%d %H:%M:%S");
    setCtrlText(m_hFileFilter, "*.log;*.txt;*.out");

    SendMessageA(m_hBucketUnit, CB_ADDSTRING, 0, (LPARAM)"Seconds");
    SendMessageA(m_hBucketUnit, CB_ADDSTRING, 0, (LPARAM)"Minutes");
    SendMessageA(m_hBucketUnit, CB_ADDSTRING, 0, (LPARAM)"Hours");
    SendMessageA(m_hBucketUnit, CB_SETCURSEL, 2, 0);
    setCtrlText(m_hBucketVal, "1");

    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"All Levels");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"TRACE");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"DEBUG");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"INFO");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"WARN");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"ERROR");
    SendMessageA(m_hLevelCombo, CB_ADDSTRING, 0, (LPARAM)"FATAL");
    SendMessageA(m_hLevelCombo, CB_SETCURSEL, 0, 0);

    // Tabs
    TCITEMA ti = {};
    ti.mask    = TCIF_TEXT;
    ti.pszText = const_cast<char*>("Logs");
    SendMessageA(m_hTabs, TCM_INSERTITEMA, 0, (LPARAM)&ti);
    ti.pszText = const_cast<char*>("Patterns");
    SendMessageA(m_hTabs, TCM_INSERTITEMA, 1, (LPARAM)&ti);

    setupLogsColumns();
    setupPatternsColumns();

    // Disable visual theming so Notepad++ dark mode cannot override our colours.
    // "Explorer" style gives the modern look; empty strings disable dark-mode hooks.
    auto fixListColors = [](HWND lv) {
        SetWindowTheme(lv, L"Explorer", L"Explorer");
        ListView_SetBkColor    (lv, RGB(255, 255, 255));
        ListView_SetTextBkColor(lv, RGB(255, 255, 255));
        ListView_SetTextColor  (lv, RGB(20,  20,  20));
    };
    fixListColors(m_hLogsList);
    fixListColors(m_hPatternsList);

    // Subclass to force a white background even when Notepad++ dark mode
    // re-themes the controls after initialisation.
    SetWindowSubclass(m_hLogsList,     ListWhiteBkSubclass, 1, 0);
    SetWindowSubclass(m_hPatternsList, ListWhiteBkSubclass, 1, 0);

    // Re-assert white now that the intercepting subclass is in place
    // (this corrects any dark colour NPP applied before our subclass).
    ListView_SetBkColor    (m_hLogsList,     RGB(255, 255, 255));
    ListView_SetTextBkColor(m_hLogsList,     RGB(255, 255, 255));
    ListView_SetBkColor    (m_hPatternsList, RGB(255, 255, 255));
    ListView_SetTextBkColor(m_hPatternsList, RGB(255, 255, 255));

    // Determinate progress bar (0-100%) — on top of the histogram, hidden until busy.
    m_hProgress = CreateWindowExA(
        0, PROGRESS_CLASSA, "",
        WS_CHILD | PBS_SMOOTH,
        0, 0, 10, 10, _hSelf, nullptr, _hInst, nullptr);
    SendMessage(m_hProgress, PBM_SETRANGE32, 0, 100);

    // Percentage label shown above the progress bar while busy.
    m_hProgressLabel = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | SS_CENTER,
        0, 0, 10, 10, _hSelf, nullptr, _hInst, nullptr);
    SendMessage(m_hProgressLabel, WM_SETFONT,
                (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    showListView(0);
    updateStatus("Ready - browse to a log folder and click Process.");
}

// ── Column setup ──────────────────────────────────────────────────────────────

static void insertColA(HWND lv, int idx, const char* text, int cx, int fmt = LVCFMT_LEFT) {
    LVCOLUMNA c = {};
    c.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    c.fmt     = fmt;
    c.cx      = cx;
    c.pszText = const_cast<char*>(text);
    SendMessageA(lv, LVM_INSERTCOLUMNA, (WPARAM)idx, (LPARAM)&c);
}

// Forces a white background regardless of Notepad++ dark-mode subclassing.
// NPP repeatedly re-sets the ListView background to a dark colour via
// LVM_SETBKCOLOR/LVM_SETTEXTBKCOLOR; we intercept those and force white so the
// list always paints light.
static LRESULT CALLBACK ListWhiteBkSubclass(HWND h, UINT msg, WPARAM w, LPARAM l,
                                            UINT_PTR /*id*/, DWORD_PTR /*ref*/) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC dc = (HDC)w;
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
        return 1;
    }
    case LVM_SETBKCOLOR:
    case LVM_SETTEXTBKCOLOR:
        // Ignore whatever colour is requested — always white.
        return DefSubclassProc(h, msg, w, (LPARAM)RGB(255, 255, 255));
    case LVM_SETTEXTCOLOR:
        return DefSubclassProc(h, msg, w, (LPARAM)RGB(20, 20, 20));
    }
    return DefSubclassProc(h, msg, w, l);
}

void LogAnalyzerDialog::setupLogsColumns() {
    // No double-buffer: we want our WM_ERASEBKGND subclass to control the fill.
    ListView_SetExtendedListViewStyle(m_hLogsList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    insertColA(m_hLogsList, 0, "#",         36, LVCFMT_RIGHT);
    insertColA(m_hLogsList, 1, "Timestamp", 140);
    insertColA(m_hLogsList, 2, "Level",      52);
    insertColA(m_hLogsList, 3, "Source",     80);
    insertColA(m_hLogsList, 4, "Message",   600);
}

void LogAnalyzerDialog::setupPatternsColumns() {
    ListView_SetExtendedListViewStyle(m_hPatternsList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    insertColA(m_hPatternsList, 0, "Count",      60, LVCFMT_RIGHT);
    insertColA(m_hPatternsList, 1, "Pattern",   380);
    insertColA(m_hPatternsList, 2, "First Seen",140);
    insertColA(m_hPatternsList, 3, "Last Seen", 140);
}

// ── Event handlers ────────────────────────────────────────────────────────────

void LogAnalyzerDialog::onBrowse() {
    BROWSEINFOA bi = {};
    bi.hwndOwner = _hSelf;
    bi.lpszTitle = "Select Log Folder";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) setCtrlText(m_hFolder, path);
        CoTaskMemFree(pidl);
    }
}

void LogAnalyzerDialog::onProcess() {
    if (m_processing || m_analyzing) return;

    LogConfig cfg;
    cfg.folderPath      = getCtrlText(m_hFolder);
    cfg.regexPattern    = getCtrlText(m_hRegex);
    cfg.timestampFormat = getCtrlText(m_hTsFmt);
    cfg.fileFilter      = getCtrlText(m_hFileFilter);
    cfg.recursive       = isChecked(m_hRecursive);

    if (!m_core.configure(cfg)) {
        MessageBoxA(_hSelf, m_core.getLastError().c_str(), "Configuration Error", MB_ICONERROR | MB_OK);
        return;
    }
    m_core.setTimeBucketSize(getBucketSeconds());
    m_processing        = true;

    // Clear the virtual list so it stops requesting rows while data changes.
    m_filteredIndices.clear();
    ListView_SetItemCountEx(m_hLogsList, 0, 0);

    setBusy(true, "Reading and parsing log files...");

    // Read + parse + index happens on a worker thread.
    // WM_LOGANALYZER_DONE fires on the UI thread when finished.
    m_core.processLogsAsync(_hSelf, [this](int pct, const std::string& s) {
        // wParam = percentage, lParam = status text (freed by the handler)
        std::string* msg = new std::string(s);
        PostMessage(_hSelf, WM_LA_PROGRESS, (WPARAM)pct, (LPARAM)msg);
        return true;
    });
}

void LogAnalyzerDialog::onFilter() {
    if (m_core.getTotalLogCount() == 0) {
        updateStatus("No logs loaded - click Process first.");
        return;
    }
    startAnalyze();
}

// Builds filter criteria on the UI thread, then runs the heavy analysis
// (filter + aggregate + patterns + exceptions) on a background thread so the
// Notepad++ window stays responsive.
void LogAnalyzerDialog::startAnalyze() {
    if (m_analyzing) return;

    FilterCriteria criteria;
    int levelIdx = (int)SendMessage(m_hLevelCombo, CB_GETCURSEL, 0, 0);
    criteria.level = (levelIdx == 0) ? -1 : (levelIdx - 1);

    std::string kw = getCtrlText(m_hKeywords);
    if (!kw.empty()) {
        std::istringstream iss(kw);
        std::string w;
        while (iss >> w) criteria.keywords.push_back(w);
    }
    criteria.searchText    = getCtrlText(m_hSearch);
    criteria.searchIsRegex = isChecked(m_hSearchRegex);

    if (m_histSel.active) {
        criteria.startTime = m_histSel.startTime;
        criteria.endTime   = m_histSel.endTime;
    }

    m_pendingCriteria = criteria;
    m_analyzing       = true;

    // Stop the virtual list from reading m_filteredIndices while the worker
    // rewrites it.
    ListView_SetItemCountEx(m_hLogsList, 0, 0);

    setBusy(true, "Analyzing logs...");

    if (m_hAnalyzeThread) { CloseHandle(m_hAnalyzeThread); m_hAnalyzeThread = nullptr; }
    m_hAnalyzeThread = CreateThread(nullptr, 0, analyzeThreadProc, this, 0, nullptr);
}

DWORD WINAPI LogAnalyzerDialog::analyzeThreadProc(LPVOID param) {
    reinterpret_cast<LogAnalyzerDialog*>(param)->analyzeWorkerBody();
    return 0;
}

void LogAnalyzerDialog::analyzeWorkerBody() {
    // All heavy work — runs OFF the UI thread.
    // Post coarse milestone percentages so the bar keeps advancing.
    auto post = [this](int pct, const char* text) {
        PostMessage(_hSelf, WM_LA_PROGRESS, (WPARAM)pct, (LPARAM)new std::string(text));
    };

    post(20, "Filtering...");
    m_filteredIndices = m_core.filterLogs(m_pendingCriteria);

    post(40, "Aggregating...");
    m_aggregation = m_core.aggregate(m_filteredIndices);

    // Pattern detection is the slow phase — map its internal 0-100 progress
    // onto the 70-99% range so the bar keeps moving.
    m_core.analyzePatterns(m_filteredIndices, [this](int p) {
        int overall = 70 + (p * 29) / 100;          // 70 -> 99
        PostMessage(_hSelf, WM_LA_PROGRESS, (WPARAM)overall,
                    (LPARAM)new std::string("Detecting patterns..."));
    });

    post(100, "Rendering...");
    PostMessage(_hSelf, WM_LA_ANALYZED, 0, 0);
}

void LogAnalyzerDialog::onAnalyzeDone() {
    m_analyzing = false;
    if (m_hAnalyzeThread) { CloseHandle(m_hAnalyzeThread); m_hAnalyzeThread = nullptr; }

    refreshLogsVirtualList();
    populatePatternsTab();
    updateHistogram();
    updateStatus();

    setBusy(false);
}

// Show / hide the determinate progress bar and toggle the action buttons.
void LogAnalyzerDialog::setBusy(bool busy, const std::string& msg) {
    if (!msg.empty()) updateStatus(msg);

    EnableWindow(m_hProcess, busy ? FALSE : TRUE);
    EnableWindow(m_hFilter,  busy ? FALSE : TRUE);

    if (m_hProgress) {
        if (busy) {
            // Position the bar across the middle of the histogram area.
            RECT hr; GetWindowRect(m_hHistogram, &hr);
            MapWindowPoints(HWND_DESKTOP, _hSelf, reinterpret_cast<POINT*>(&hr), 2);
            int areaW = hr.right - hr.left;
            int areaH = hr.bottom - hr.top;
            int barW  = areaW / 2;
            int barH  = 20;
            int x = hr.left + (areaW - barW) / 2;
            int y = hr.top  + (areaH - barH) / 2;

            // Label sits just above the bar
            if (m_hProgressLabel) {
                SetWindowPos(m_hProgressLabel, HWND_TOP,
                             hr.left + 8, y - 22, areaW - 16, 18, SWP_SHOWWINDOW);
                SetWindowTextA(m_hProgressLabel, "0%");
            }
            SetWindowPos(m_hProgress, HWND_TOP, x, y, barW, barH, SWP_SHOWWINDOW);
            SendMessage(m_hProgress, PBM_SETPOS, 0, 0);
        } else {
            ShowWindow(m_hProgress, SW_HIDE);
            if (m_hProgressLabel) ShowWindow(m_hProgressLabel, SW_HIDE);
        }
    }
}

// ── Virtual log list ──────────────────────────────────────────────────────────

void LogAnalyzerDialog::refreshLogsVirtualList() {
    // Virtual list just needs item count; data served via LVN_GETDISPINFO
    ListView_SetItemCountEx(m_hLogsList, (int)m_filteredIndices.size(),
                            LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    ListView_SetItemState(m_hLogsList, -1, 0, LVIS_SELECTED);
    InvalidateRect(m_hLogsList, NULL, TRUE);
}

// Notepad++ compiles as Unicode, so the ListView sends LVN_GETDISPINFOW.
// We receive a NMLVDISPINFOW and must write into its WCHAR buffer.
void LogAnalyzerDialog::onGetDispInfoW(NMLVDISPINFOW* pdi) {
    if (!(pdi->item.mask & LVIF_TEXT)) return;

    int row = pdi->item.iItem;
    if (row < 0 || row >= (int)m_filteredIndices.size()) return;

    int logIdx = m_filteredIndices[row];
    const auto& logs = m_core.getLogs();
    if (logIdx < 0 || logIdx >= (int)logs.size()) return;
    const LogEntry& e = logs[logIdx];

    std::string text;
    switch (pdi->item.iSubItem) {
        case 0: text = std::to_string(row + 1);                               break;
        case 1: text = fmtTime(e.timestamp);                                  break;
        case 2: text = levelName(e.level);                                    break;
        case 3: text = e.source;                                               break;
        case 4: text = e.message.size() > 512
                           ? e.message.substr(0, 512) + "..."
                           : e.message;                                        break;
        default: break;
    }

    // Convert narrow string → wide and copy into the buffer the ListView provides.
    MultiByteToWideChar(CP_ACP, 0,
                        text.c_str(), -1,
                        pdi->item.pszText, pdi->item.cchTextMax);
}

// ── Patterns tab ──────────────────────────────────────────────────────────────

void LogAnalyzerDialog::populatePatternsTab() {
    const auto& patterns = m_core.getPatterns();
    SendMessage(m_hPatternsList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(m_hPatternsList);

    int row = 0;
    for (const auto& p : patterns) {
        std::string countStr = std::to_string(p.count);

        LVITEMA lvi = {};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = row;
        lvi.pszText  = const_cast<char*>(countStr.c_str());
        SendMessageA(m_hPatternsList, LVM_INSERTITEMA, 0, (LPARAM)&lvi);

        auto setSubA = [&](int col, const std::string& t) {
            LVITEMA si = {}; si.iSubItem = col;
            si.pszText = const_cast<char*>(t.c_str());
            SendMessageA(m_hPatternsList, LVM_SETITEMTEXTA, (WPARAM)row, (LPARAM)&si);
        };
        std::string pat = p.pattern.size() > 120 ? p.pattern.substr(0, 120) + "..." : p.pattern;
        setSubA(1, pat);
        setSubA(2, fmtTime(p.firstSeen));
        setSubA(3, fmtTime(p.lastSeen));
        row++;
        if (row >= 5000) break;
    }

    SendMessage(m_hPatternsList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hPatternsList, NULL, TRUE);
}

// ── Histogram ─────────────────────────────────────────────────────────────────

void LogAnalyzerDialog::updateHistogram() {
    if (!m_hHistogram) return;
    HistogramControl::setData(m_hHistogram, m_aggregation.timelineBuckets, getBucketSeconds());
}

void LogAnalyzerDialog::onHistogramRange() {
    m_histSel = HistogramControl::getSelection(m_hHistogram);
    // Re-run analysis with the new time window (off the UI thread)
    if (m_core.getTotalLogCount() > 0) startAnalyze();
}

// ── Status bar ────────────────────────────────────────────────────────────────

void LogAnalyzerDialog::updateStatus() {
    std::ostringstream oss;
    oss << "Showing " << m_filteredIndices.size()
        << " / " << m_core.getTotalLogCount() << " entries";

    const char* lvlNames[] = { "TRACE","DEBUG","INFO","WARN","ERROR","FATAL" };
    for (int i = 0; i < 6; i++) {
        auto it = m_aggregation.levelCounts.find(i);
        if (it != m_aggregation.levelCounts.end() && it->second > 0)
            oss << "  |  " << lvlNames[i] << ": " << it->second;
    }

    // Spike/anomaly indicator
    int errors = 0;
    auto eit = m_aggregation.levelCounts.find(LOG_ERROR);
    if (eit != m_aggregation.levelCounts.end()) errors = eit->second;
    int fatals = 0;
    auto fit = m_aggregation.levelCounts.find(LOG_FATAL);
    if (fit != m_aggregation.levelCounts.end()) fatals = fit->second;
    if (fatals > 0) oss << "  *** " << fatals << " FATAL(S) ***";
    else if (errors > 0 && (double)errors / std::max(1, m_aggregation.totalLogs) > 0.1)
        oss << "  !! High error rate";

    if (m_histSel.active) oss << "  [Time filter active - dbl-click histogram to reset]";

    setCtrlText(m_hStatus, oss.str());
}

// ── Double-click / detail dialogs ────────────────────────────────────────────

void LogAnalyzerDialog::showDetailDialog(int logIdx) {
    const auto& logs = m_core.getLogs();
    if (logIdx < 0 || logIdx >= (int)logs.size()) return;
    const LogEntry& e = logs[logIdx];

    // Simple modal dialog
    HWND hDlg = CreateDialogParamA(_hInst, MAKEINTRESOURCEA(IDD_LOG_DETAIL), _hSelf,
        [](HWND hD, UINT msg, WPARAM wp, LPARAM /*lp*/) -> INT_PTR {
            if (msg == WM_COMMAND && LOWORD(wp) == IDOK) { EndDialog(hD, 0); return TRUE; }
            if (msg == WM_INITDIALOG) return TRUE;
            return FALSE;
        }, 0);

    if (!hDlg) {
        // Fallback: simple message box if dialog resource is not yet compiled in
        std::string info = "Time: " + fmtTime(e.timestamp) + "\nLevel: " + levelName(e.level)
                         + "\nSource: " + e.source + "\nMessage:\n" + e.message
                         + "\n\nRaw line:\n" + e.rawLine;
        MessageBoxA(_hSelf, info.c_str(), "Log Entry Detail", MB_OK);
        return;
    }

    SetDlgItemTextA(hDlg, IDC_DETAIL_TIME,   fmtTime(e.timestamp).c_str());
    SetDlgItemTextA(hDlg, IDC_DETAIL_LEVEL,  levelName(e.level).c_str());
    SetDlgItemTextA(hDlg, IDC_DETAIL_SOURCE, e.source.c_str());
    SetDlgItemTextA(hDlg, IDC_DETAIL_MSG,    e.message.c_str());
    SetDlgItemTextA(hDlg, IDC_DETAIL_RAW,    e.rawLine.c_str());

    ShowWindow(hDlg, SW_SHOW);
    // Convert to modal
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
    }
}

void LogAnalyzerDialog::onLogsDoubleClick(int itemIdx) {
    if (itemIdx < 0 || itemIdx >= (int)m_filteredIndices.size()) return;
    showDetailDialog(m_filteredIndices[itemIdx]);
}

void LogAnalyzerDialog::showPatternDetail(int patIdx) {
    const auto& patterns = m_core.getPatterns();
    if (patIdx < 0 || patIdx >= (int)patterns.size()) return;
    const LogPattern& p = patterns[patIdx];
    std::string info = "Pattern: " + p.pattern
                     + "\n\nOccurrences: " + std::to_string(p.count)
                     + "\nFirst seen: " + fmtTime(p.firstSeen)
                     + "\nLast seen:  " + fmtTime(p.lastSeen)
                     + "\n\nSample message:\n" + p.sample;
    MessageBoxA(_hSelf, info.c_str(), "Pattern Detail", MB_OK);
}

void LogAnalyzerDialog::onPatternsDoubleClick(int itemIdx) {
    showPatternDetail(itemIdx);
}

// ── Column sort (logs tab) ────────────────────────────────────────────────────

void LogAnalyzerDialog::onColumnClick(int col) {
    if (m_sortCol == col) m_sortAsc = !m_sortAsc;
    else { m_sortCol = col; m_sortAsc = true; }

    const auto& logs = m_core.getLogs();
    std::sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [&](int a, int b) {
            if (a < 0 || a >= (int)logs.size() ||
                b < 0 || b >= (int)logs.size()) return false;
            const LogEntry& la = logs[a];
            const LogEntry& lb = logs[b];
            bool less = false;
            switch (m_sortCol) {
                case 0: less = a < b; break;
                case 1: less = la.timestamp < lb.timestamp; break;
                case 2: less = la.level < lb.level; break;
                case 3: less = la.source  < lb.source;  break;
                case 4: less = la.message < lb.message; break;
                default: less = a < b;
            }
            return m_sortAsc ? less : !less;
        });

    refreshLogsVirtualList();
}

// ── Context menu ──────────────────────────────────────────────────────────────

void LogAnalyzerDialog::onContextMenu(HWND hList, POINT pt) {
    int itemIdx = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (itemIdx < 0) return;

    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, 1, "Copy Row");
    AppendMenuA(hMenu, MF_STRING, 2, "Copy Message");
    AppendMenuA(hMenu, MF_STRING, 3, "Copy Raw Line");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, 4, "Show Detail...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, _hSelf, NULL);
    DestroyMenu(hMenu);

    if (cmd == 0 || hList != m_hLogsList) return;
    if (itemIdx >= (int)m_filteredIndices.size()) return;
    int logIdx = m_filteredIndices[itemIdx];
    const auto& logs = m_core.getLogs();
    if (logIdx < 0 || logIdx >= (int)logs.size()) return;
    const LogEntry& e = logs[logIdx];

    std::string text;
    if (cmd == 1) {
        text = fmtTime(e.timestamp) + "\t" + levelName(e.level)
             + "\t" + e.source + "\t" + e.message;
    } else if (cmd == 2) {
        text = e.message;
    } else if (cmd == 3) {
        text = e.rawLine;
    } else if (cmd == 4) {
        showDetailDialog(logIdx);
        return;
    }

    if (!text.empty() && OpenClipboard(_hSelf)) {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hg) {
            memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
            GlobalUnlock(hg);
            SetClipboardData(CF_TEXT, hg);
        }
        CloseClipboard();
    }
}

// ── Layout / resize ───────────────────────────────────────────────────────────

void LogAnalyzerDialog::onResize(int W, int H) {
    if (W < 10 || H < 10) return;

    // W and H are in pixels (client area from WM_SIZE).
    // Instead of hardcoding pixel Y positions (which break at different DPI /
    // font sizes), we read the actual positions the RC dialog-unit layout
    // already placed the controls at, then only resize what needs to grow.

    auto getRC = [&](HWND h, RECT& r) {
        if (!h) return;
        GetWindowRect(h, &r);
        MapWindowPoints(HWND_DESKTOP, _hSelf, reinterpret_cast<POINT*>(&r), 2);
    };

    RECT histRc = {}, tabsRc = {};
    getRC(m_hHistogram, histRc);
    getRC(m_hTabs,      tabsRc);

    const int M        = 4;   // left/right margin (px)
    const int STATUS_H = 18;

    auto move = [](HWND h, int x, int y, int w, int ht) {
        if (h) SetWindowPos(h, nullptr, x, y, w, ht, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // Histogram – keep its Y from the RC, just stretch the width
    move(m_hHistogram, M,
         histRc.top,
         W - M * 2,
         histRc.bottom - histRc.top);

    // Tab control – keep its Y, stretch width
    move(m_hTabs, M,
         tabsRc.top,
         W - M * 2,
         tabsRc.bottom - tabsRc.top);

    // ListViews – fill from bottom of tabs to above the status bar
    int listY = tabsRc.bottom;
    int listH = H - listY - STATUS_H - M;
    if (listH < 40) listH = 40;

    move(m_hLogsList,       M, listY, W - M * 2, listH);
    move(m_hPatternsList,   M, listY, W - M * 2, listH);

    // Status bar – pin to bottom
    move(m_hStatus, M, H - STATUS_H - 2, W - M * 2, STATUS_H);
}

// ── Dialog procedure ──────────────────────────────────────────────────────────

INT_PTR CALLBACK LogAnalyzerDialog::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

    case WM_INITDIALOG:
        createControls();
        return TRUE;

    case WM_SIZE:
        onResize(LOWORD(lParam), HIWORD(lParam));
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
            case IDC_BROWSE_FOLDER: onBrowse();  return TRUE;
            case IDC_PROCESS:       onProcess(); return TRUE;
            case IDC_FILTER:        onFilter();  return TRUE;
        }
        break;

    // Progress update from a background thread:
    //   wParam = percentage (0-100), lParam = std::string* status text
    case WM_LA_PROGRESS: {
        int pct = (int)wParam;
        if (pct < 0) pct = 0; if (pct > 100) pct = 100;
        if (m_hProgress) SendMessage(m_hProgress, PBM_SETPOS, (WPARAM)pct, 0);

        std::string* msg = reinterpret_cast<std::string*>(lParam);
        std::ostringstream oss;
        oss << pct << "%";
        if (msg && !msg->empty()) oss << " - " << *msg;

        // Big centered label over the histogram + the bottom status bar
        if (m_hProgressLabel) SetWindowTextA(m_hProgressLabel, oss.str().c_str());
        updateStatus(oss.str());
        delete msg;
        return TRUE;
    }

    // File reading / parsing / indexing completed (worker thread)
    case WM_LOGANALYZER_DONE: {
        m_processing = false;
        if (lParam) {
            // Heavy filter/pattern/exception analysis runs on its own worker.
            startAnalyze();
        } else {
            setBusy(false);
            MessageBoxA(_hSelf, m_core.getLastError().c_str(), "Load Error", MB_ICONERROR | MB_OK);
            updateStatus("Error: " + m_core.getLastError());
        }
        return TRUE;
    }

    // Analysis worker completed
    case WM_LA_ANALYZED:
        onAnalyzeDone();
        return TRUE;

    // Histogram drag-select completed
    case WM_HISTOGRAM_RANGE:
        onHistogramRange();
        return TRUE;

    case WM_NOTIFY: {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);

        // ── Tab change ──
        if (pnmh->hwndFrom == m_hTabs && pnmh->code == TCN_SELCHANGE) {
            m_activeTab = (int)SendMessage(m_hTabs, TCM_GETCURSEL, 0, 0);
            showListView(m_activeTab);
            return TRUE;
        }

        // ── Logs list virtual data (Unicode project → W notification) ──
        if (pnmh->hwndFrom == m_hLogsList && pnmh->code == LVN_GETDISPINFOW) {
            onGetDispInfoW(reinterpret_cast<NMLVDISPINFOW*>(lParam));
            return TRUE;
        }

        // ── Column click / sort ──
        if (pnmh->hwndFrom == m_hLogsList && pnmh->code == LVN_COLUMNCLICK) {
            onColumnClick(reinterpret_cast<LPNMLISTVIEW>(lParam)->iSubItem);
            return TRUE;
        }

        // ── Double-click ──
        if (pnmh->code == NM_DBLCLK) {
            LPNMITEMACTIVATE nmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
            if (pnmh->hwndFrom == m_hLogsList)       onLogsDoubleClick(nmia->iItem);
            if (pnmh->hwndFrom == m_hPatternsList)   onPatternsDoubleClick(nmia->iItem);
            return TRUE;
        }

        // ── Custom draw (row colours for logs list) ──
        if (pnmh->code == NM_CUSTOMDRAW) {
            LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
            if (pnmh->hwndFrom != m_hLogsList) break;

            switch (cd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                    return TRUE;
                case CDDS_ITEMPREPAINT: {
                    int row = (int)cd->nmcd.dwItemSpec;
                    if (row >= 0 && row < (int)m_filteredIndices.size()) {
                        int logIdx = m_filteredIndices[row];
                        const auto& logs = m_core.getLogs();
                        if (logIdx >= 0 && logIdx < (int)logs.size()) {
                            int lvl = logs[logIdx].level;
                            cd->clrTextBk = levelRowColor(lvl);
                            if (lvl == LOG_ERROR || lvl == LOG_FATAL)
                                cd->clrText = RGB(160, 0, 0);
                            else if (lvl == LOG_WARN)
                                cd->clrText = RGB(130, 70, 0);
                            else
                                cd->clrText = RGB(20, 20, 20);
                        }
                    }
                    SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, CDRF_NEWFONT);
                    return TRUE;
                }
            }
        }
        break;
    }

    case WM_THEMECHANGED: {
        // Notepad++ dark/light theme switch — re-pin our colours.
        auto repin = [](HWND lv) {
            SetWindowTheme(lv, L"Explorer", L"Explorer");
            ListView_SetBkColor    (lv, RGB(255,255,255));
            ListView_SetTextBkColor(lv, RGB(255,255,255));
            ListView_SetTextColor  (lv, RGB(20,20,20));
            InvalidateRect(lv, NULL, TRUE);
        };
        if (m_hLogsList)       repin(m_hLogsList);
        if (m_hPatternsList)   repin(m_hPatternsList);
        return TRUE;
    }

    case WM_CONTEXTMENU: {
        HWND hCtrl = (HWND)wParam;
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (hCtrl == m_hLogsList || hCtrl == m_hPatternsList)
            onContextMenu(hCtrl, pt);
        return TRUE;
    }

    } // switch(message)

    return FALSE;
}

} // namespace LogAnalyzer

// Made with Bob
