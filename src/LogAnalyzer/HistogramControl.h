#ifndef HISTOGRAM_CONTROL_H
#define HISTOGRAM_CONTROL_H

#include "DataModels.h"
#include <windows.h>
#include <vector>
#include <string>
#include <functional>

namespace LogAnalyzer {

// Window class name — register once via HistogramControl::registerClass()
#define HISTOGRAM_CLASS_NAME "LogAnalyzerHistogram"

// Posted to the parent window when the user completes a drag-selection.
// wParam = (time_t)startTime  (lower 32 bits on x86, full 64-bit on x64 via LPARAM pair)
// We use a COPYDATASTRUCT / WM_APP approach instead — simpler:
#define WM_HISTOGRAM_RANGE (WM_APP + 1)
// LOWORD(lParam)=unused; selection is queried via HistogramControl::getSelection(hwnd)

struct HistogramSelection {
    time_t startTime = 0;
    time_t endTime   = 0;
    bool   active    = false; // false = no selection (show all)
};

class HistogramControl {
public:
    static void registerClass(HINSTANCE hInst);

    // Call after creating the window to push data into it.
    static void setData(HWND hwnd,
                        const std::map<time_t, BucketData>& buckets,
                        int bucketSizeSeconds);

    // Retrieve current time-range selection.
    static HistogramSelection getSelection(HWND hwnd);

    // Clear selection programmatically.
    static void clearSelection(HWND hwnd);

    // Toggle per-level series visibility (level 0-5).
    static void setSeriesVisible(HWND hwnd, int level, bool visible);

private:
    // Internal per-window state stored via GWLP_USERDATA
    struct State {
        std::vector<std::pair<time_t, BucketData>> buckets;
        int  bucketSecs  = 3600;
        int  maxTotal    = 1;
        int  yMax        = 1;   // rounded-up nice maximum for Y axis
        int  hoverIdx    = -1;
        bool selecting   = false;
        int  selStartPx  = -1;
        int  selEndPx    = -1;
        bool seriesVis[LOG_LEVEL_COUNT] = {true,true,true,true,true,true};
        HWND hwnd        = nullptr;
        HWND hwndParent  = nullptr;
        HFONT hFont      = nullptr;
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    static void  onPaint    (HWND hwnd, State& s);
    static void  onMouseMove(HWND hwnd, State& s, int x, int y);
    static void  onLDown    (HWND hwnd, State& s, int x);
    static void  onLUp      (HWND hwnd, State& s, int x);
    static void  onDblClick (HWND hwnd, State& s);

    static int   bucketIdxAt(const State& s, int px, const RECT& rc);
    static RECT  bucketRect (const State& s, int idx, const RECT& rc);
    static void  drawTooltip(HDC hdc, const RECT& rc, const State& s);

    static COLORREF levelColor(int lvl, bool opaque);
    static State*   getState(HWND hwnd);
};

} // namespace LogAnalyzer

#endif // HISTOGRAM_CONTROL_H

// Made with Bob
