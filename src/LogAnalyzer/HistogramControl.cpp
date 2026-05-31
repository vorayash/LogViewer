#define NOMINMAX
#include "HistogramControl.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#pragma comment(lib, "msimg32.lib")

namespace LogAnalyzer {

// ── Design tokens (OpenSearch-inspired) ───────────────────────────────────────
static const COLORREF CLR_BG       = RGB(250, 251, 252); // near-white background
static const COLORREF CLR_PANEL    = RGB(255, 255, 255); // chart area
static const COLORREF CLR_GRID     = RGB(232, 236, 240); // subtle grid lines
static const COLORREF CLR_AXIS_TXT = RGB(100, 110, 120); // axis label colour
static const COLORREF CLR_BORDER   = RGB(210, 215, 220); // outer border

// Level bar colours.
// DEBUG and INFO intentionally share the same green — they are not
// visually distinguished in the histogram.
static const COLORREF LVL_CLR[LOG_LEVEL_COUNT] = {
    RGB(156,  39, 176), // TRACE  – purple
    RGB( 52, 168,  83), // DEBUG  – same green as INFO (no distinction)
    RGB( 52, 168,  83), // INFO   – green
    RGB(251, 140,   0), // WARN   – amber
    RGB(229,  57,  53), // ERROR  – red
    RGB(117,  29,  29), // FATAL  – dark-red
};
static const COLORREF CLR_SPIKE    = RGB(229,  57,  53); // anomaly marker
static const COLORREF CLR_SEL_FILL = RGB( 25, 118, 210); // selection overlay fill
static const COLORREF CLR_SEL_EDGE = RGB( 13,  71, 161); // selection edge

// Padding (pixels)
static const int PL = 46, PR = 12, PT = 12, PB = 30;

// ── Helpers ───────────────────────────────────────────────────────────────────

static HBRUSH br(COLORREF c)  { return CreateSolidBrush(c); }
static HPEN   pen(COLORREF c, int w = 1) { return CreatePen(PS_SOLID, w, c); }

static void fillRect(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = br(c); FillRect(dc, &r, b); DeleteObject(b);
}

static std::string fmtTime(time_t t) {
    if (t <= 0) return "";
    struct tm tm{}; localtime_s(&tm, &t);
    char buf[20]; strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}
static std::string fmtDateTime(time_t t) {
    if (t <= 0) return "";
    struct tm tm{}; localtime_s(&tm, &t);
    char buf[24]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}
static std::string fmtCount(int n) {
    if (n >= 1000000) return std::to_string(n/1000000) + "M";
    if (n >= 1000)    return std::to_string(n/1000)    + "k";
    return std::to_string(n);
}

// Nice round Y-axis ceiling
static int niceMax(int raw) {
    if (raw <= 0) return 10;
    int mag = 1;
    while (mag * 10 < raw) mag *= 10;
    int step = mag;
    if      (raw <= 2  * mag) step = mag / 5;
    else if (raw <= 5  * mag) step = mag / 2;
    int ceil = ((raw + step - 1) / step) * step;
    return std::max(ceil, 1);
}

// ── State accessors ───────────────────────────────────────────────────────────

HistogramControl::State* HistogramControl::getState(HWND hwnd) {
    return reinterpret_cast<State*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
}

// ── Public API ────────────────────────────────────────────────────────────────

void HistogramControl::registerClass(HINSTANCE hInst) {
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = NULL; // we paint everything ourselves
    wc.lpszClassName = HISTOGRAM_CLASS_NAME;
    RegisterClassExA(&wc);
}

void HistogramControl::setData(HWND hwnd,
                               const std::map<time_t, BucketData>& src,
                               int bucketSecs)
{
    State* s = getState(hwnd);
    if (!s) return;

    s->buckets.clear();
    s->buckets.reserve(src.size());
    for (const auto& p : src) s->buckets.push_back(p);

    s->bucketSecs = bucketSecs > 0 ? bucketSecs : 3600;
    s->maxTotal   = 1;
    for (const auto& p : s->buckets)
        s->maxTotal = std::max(s->maxTotal, p.second.total);
    s->yMax = niceMax(s->maxTotal);

    s->hoverIdx  = -1;
    s->selStartPx = s->selEndPx = -1;
    s->selecting  = false;

    InvalidateRect(hwnd, NULL, TRUE);
}

HistogramSelection HistogramControl::getSelection(HWND hwnd) {
    State* s = getState(hwnd);
    HistogramSelection sel;
    if (!s || s->selStartPx < 0 || s->buckets.empty()) return sel;

    RECT rc; GetClientRect(hwnd, &rc);
    int lo = std::min(s->selStartPx, s->selEndPx);
    int hi = std::max(s->selStartPx, s->selEndPx);
    int iA = bucketIdxAt(*s, lo, rc);
    int iB = bucketIdxAt(*s, hi, rc);
    if (iA < 0 || iB < 0) return sel;
    iA = std::max(0, iA);
    iB = std::min((int)s->buckets.size() - 1, iB);

    sel.startTime = s->buckets[iA].first;
    sel.endTime   = s->buckets[iB].first + (time_t)s->bucketSecs;
    sel.active    = true;
    return sel;
}

void HistogramControl::clearSelection(HWND hwnd) {
    State* s = getState(hwnd);
    if (!s) return;
    s->selStartPx = s->selEndPx = -1;
    s->selecting  = false;
    InvalidateRect(hwnd, NULL, TRUE);
}

void HistogramControl::setSeriesVisible(HWND hwnd, int level, bool visible) {
    State* s = getState(hwnd);
    if (!s || level < 0 || level >= LOG_LEVEL_COUNT) return;
    s->seriesVis[level] = visible;
    InvalidateRect(hwnd, NULL, TRUE);
}

// ── Geometry ──────────────────────────────────────────────────────────────────

int HistogramControl::bucketIdxAt(const State& s, int px, const RECT& rc) {
    int chartW = rc.right - rc.left - PL - PR;
    if (chartW <= 0 || s.buckets.empty()) return -1;
    int n = (int)s.buckets.size();
    float slotW = (float)chartW / n;
    int idx = (int)((px - PL) / slotW);
    return (idx >= 0 && idx < n) ? idx : -1;
}

RECT HistogramControl::bucketRect(const State& s, int idx, const RECT& rc) {
    int chartW = rc.right - rc.left - PL - PR;
    int chartH = rc.bottom - rc.top - PT - PB;
    int n = (int)s.buckets.size();
    float slotW = (float)chartW / n;
    // Bar occupies 75% of slot, centred
    float gap  = slotW * 0.12f;
    int   x0   = PL + (int)(idx * slotW + gap);
    int   x1   = PL + (int)((idx + 1) * slotW - gap);
    if (x1 <= x0) x1 = x0 + 1;
    int barH = s.yMax > 0 ? (chartH * s.buckets[idx].second.total / s.yMax) : 0;
    int y0   = PT + chartH - barH;
    int y1   = PT + chartH;
    return { x0, y0, x1, y1 };
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void HistogramControl::onPaint(HWND hwnd, State& s) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int chartW = W - PL - PR;
    int chartH = H - PT - PB;

    // ── Double-buffer ──────────────────────────────────────────────────────
    HDC     mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    auto   oBmp = (HBITMAP)SelectObject(mem, bmp);
    if (s.hFont) SelectObject(mem, s.hFont);

    // Overall background
    fillRect(mem, rc, CLR_BG);

    // Chart area – white panel
    RECT panel = { PL, PT, PL + chartW, PT + chartH };
    fillRect(mem, panel, CLR_PANEL);

    // ── "No data" message ─────────────────────────────────────────────────
    if (s.buckets.empty()) {
        SetTextColor(mem, CLR_AXIS_TXT);
        SetBkMode(mem, TRANSPARENT);
        DrawTextA(mem, "No data - process logs first", -1,
                  &panel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        // Border
        HPEN bp = pen(CLR_BORDER);
        auto op = (HPEN)SelectObject(mem, bp);
        SelectObject(mem, GetStockObject(NULL_BRUSH));
        Rectangle(mem, PL, PT, PL + chartW, PT + chartH);
        SelectObject(mem, op); DeleteObject(bp);
        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oBmp); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return;
    }

    // ── Horizontal grid lines + Y labels ──────────────────────────────────
    const int GRID_STEPS = 4;
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, CLR_AXIS_TXT);

    for (int i = 0; i <= GRID_STEPS; i++) {
        int yv = s.yMax * i / GRID_STEPS;
        int y  = PT + chartH - (chartH * i / GRID_STEPS);

        HPEN gp = pen(CLR_GRID);
        auto op = (HPEN)SelectObject(mem, gp);
        MoveToEx(mem, PL, y, NULL); LineTo(mem, PL + chartW, y);
        SelectObject(mem, op); DeleteObject(gp);

        char lbl[16]; sprintf_s(lbl, "%s", fmtCount(yv).c_str());
        RECT lr = { 0, y - 7, PL - 4, y + 7 };
        DrawTextA(mem, lbl, -1, &lr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // ── Anomaly detection (2.5σ spike threshold) ──────────────────────────
    int n = (int)s.buckets.size();
    double mean = 0;
    for (const auto& p : s.buckets) mean += p.second.total;
    mean /= std::max(1, n);
    double var = 0;
    for (const auto& p : s.buckets) var += std::pow(p.second.total - mean, 2.0);
    double sigma = std::sqrt(var / std::max(1, n));
    double spikeThr = mean + 2.5 * sigma;

    // ── Bars (stacked by level, drawn bottom-up) ──────────────────────────
    // Draw order – bottom-most first so higher-severity sits on top visually
    static const int DRAW_ORDER[] = { LOG_TRACE, LOG_DEBUG, LOG_INFO,
                                      LOG_WARN,  LOG_ERROR, LOG_FATAL };
    float slotW = (float)chartW / n;
    float gap   = slotW * 0.12f;

    for (int i = 0; i < n; i++) {
        const BucketData& bd = s.buckets[i].second;
        if (bd.total == 0) continue;

        int x0  = PL + (int)(i * slotW + gap);
        int x1  = PL + (int)((i + 1) * slotW - gap);
        if (x1 <= x0) x1 = x0 + 1;
        int base = PT + chartH;  // bar bottom

        bool isSpike  = (sigma > 0 && bd.total > spikeThr);
        bool isHover  = (i == s.hoverIdx);

        // Total bar silhouette (very light grey base so empty levels show)
        int totalH = s.yMax > 0 ? (chartH * bd.total / s.yMax) : 0;
        if (totalH > 0) {
            COLORREF baseFill = isSpike ? RGB(255, 235, 235) : RGB(232, 240, 248);
            RECT br0 = { x0, base - totalH, x1, base };
            fillRect(mem, br0, baseFill);
        }

        // Stacked level segments (bottom-up)
        int yBottom = base;
        for (int li = 0; li < LOG_LEVEL_COUNT; li++) {
            int lvl = DRAW_ORDER[li];
            if (!s.seriesVis[lvl]) continue;
            int cnt = bd.counts[lvl];
            if (cnt == 0) continue;
            int segH = std::max(1, chartH * cnt / s.yMax);
            RECT seg = { x0, yBottom - segH, x1, yBottom };

            COLORREF col = LVL_CLR[lvl];
            // Hover: brighten
            if (isHover) {
                int r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
                col = RGB(std::min(255, r + 40),
                          std::min(255, g + 40),
                          std::min(255, b + 40));
            }
            fillRect(mem, seg, col);
            yBottom -= segH;
        }

        // Spike triangle above bar
        if (isSpike) {
            int cx = (x0 + x1) / 2;
            int ty = base - totalH - 8;
            POINT tri[3] = {{ cx, ty }, { cx-4, ty+7 }, { cx+4, ty+7 }};
            HBRUSH tb = br(CLR_SPIKE);
            HPEN   tp = pen(CLR_SPIKE);
            auto   ob = (HBRUSH)SelectObject(mem, tb);
            auto   op = (HPEN)SelectObject(mem, tp);
            Polygon(mem, tri, 3);
            SelectObject(mem, ob); DeleteObject(tb);
            SelectObject(mem, op); DeleteObject(tp);
        }
    }

    // ── Selection overlay ─────────────────────────────────────────────────
    if (s.selStartPx >= 0 && s.selEndPx >= 0) {
        int lx = std::max(std::min(s.selStartPx, s.selEndPx), PL);
        int rx = std::min(std::max(s.selStartPx, s.selEndPx), PL + chartW);
        if (rx > lx) {
            // Semi-transparent blue fill via AlphaBlend
            HDC     tmp  = CreateCompatibleDC(mem);
            HBITMAP tbmp = CreateCompatibleBitmap(mem, rx - lx, chartH);
            auto    otmp = (HBITMAP)SelectObject(tmp, tbmp);
            HBRUSH  sb   = br(CLR_SEL_FILL);
            RECT    tr   = { 0, 0, rx - lx, chartH };
            FillRect(tmp, &tr, sb); DeleteObject(sb);
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 55, 0 };
            AlphaBlend(mem, lx, PT, rx - lx, chartH, tmp, 0, 0, rx - lx, chartH, bf);
            SelectObject(tmp, otmp); DeleteObject(tbmp); DeleteDC(tmp);

            // Edge lines
            HPEN ep = pen(CLR_SEL_EDGE, 1);
            auto op = (HPEN)SelectObject(mem, ep);
            MoveToEx(mem, lx, PT, NULL); LineTo(mem, lx, PT + chartH);
            MoveToEx(mem, rx, PT, NULL); LineTo(mem, rx, PT + chartH);
            SelectObject(mem, op); DeleteObject(ep);
        }
    }

    // ── Axes ──────────────────────────────────────────────────────────────
    HPEN ap = pen(CLR_BORDER, 1);
    auto ao = (HPEN)SelectObject(mem, ap);
    // Left axis
    MoveToEx(mem, PL, PT, NULL); LineTo(mem, PL, PT + chartH);
    // Bottom axis
    MoveToEx(mem, PL, PT + chartH, NULL); LineTo(mem, PL + chartW, PT + chartH);
    SelectObject(mem, ao); DeleteObject(ap);

    // ── X-axis time labels ────────────────────────────────────────────────
    int maxLabels = chartW / 72;
    int labelStep = std::max(1, n / std::max(1, maxLabels));
    SetTextColor(mem, CLR_AXIS_TXT);
    for (int i = 0; i < n; i += labelStep) {
        int cx = PL + (int)((i + 0.5f) * slotW);
        std::string t = fmtTime(s.buckets[i].first);
        RECT lr = { cx - 36, PT + chartH + 4, cx + 36, PT + chartH + PB - 2 };
        DrawTextA(mem, t.c_str(), -1, &lr, DT_CENTER | DT_SINGLELINE);
        // Tick mark
        HPEN tp = pen(CLR_BORDER);
        auto op = (HPEN)SelectObject(mem, tp);
        MoveToEx(mem, cx, PT + chartH, NULL);
        LineTo  (mem, cx, PT + chartH + 3);
        SelectObject(mem, op); DeleteObject(tp);
    }

    // ── Hover tooltip ─────────────────────────────────────────────────────
    if (s.hoverIdx >= 0 && s.hoverIdx < n)
        drawTooltip(mem, rc, s);

    // Blit
    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oBmp); DeleteObject(bmp); DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

// ── Tooltip ───────────────────────────────────────────────────────────────────

void HistogramControl::drawTooltip(HDC hdc, const RECT& rc, const State& s) {
    if (s.hoverIdx < 0 || s.hoverIdx >= (int)s.buckets.size()) return;
    const auto&     bkt = s.buckets[s.hoverIdx];
    const BucketData& bd = bkt.second;

    // Build tooltip lines.
    // DEBUG and INFO share a colour so combine them into one row.
    std::ostringstream oss;
    oss << fmtDateTime(bkt.first) << "\n"
        << "Total:      " << bd.total << "\n";

    if (bd.counts[LOG_FATAL] > 0)
        oss << "FATAL:      " << bd.counts[LOG_FATAL] << "\n";
    if (bd.counts[LOG_ERROR] > 0)
        oss << "ERROR:      " << bd.counts[LOG_ERROR] << "\n";
    if (bd.counts[LOG_WARN]  > 0)
        oss << "WARN:       " << bd.counts[LOG_WARN]  << "\n";

    // Merged DEBUG + INFO
    int debugInfo = bd.counts[LOG_DEBUG] + bd.counts[LOG_INFO];
    if (debugInfo > 0)
        oss << "DEBUG/INFO: " << debugInfo << "\n";

    if (bd.counts[LOG_TRACE] > 0)
        oss << "TRACE:      " << bd.counts[LOG_TRACE] << "\n";

    std::string tip = oss.str();

    // Measure
    HFONT oldFont = nullptr;
    if (s.hFont) oldFont = (HFONT)SelectObject(hdc, s.hFont);
    RECT mr = { 0,0,200,200 };
    DrawTextA(hdc, tip.c_str(), -1, &mr, DT_CALCRECT | DT_LEFT);
    int tw = mr.right  + 16;
    int th = mr.bottom + 10;

    int chartW = rc.right - rc.left - PL - PR;
    float slotW = (float)chartW / (int)s.buckets.size();
    int bx = PL + (int)((s.hoverIdx + 0.5f) * slotW);

    // Prefer right of bar; flip left if it would overflow
    int tx = bx + 12;
    if (tx + tw + 4 > rc.right) tx = bx - tw - 10;
    // Hard-clamp so tooltip never exits the control on either side.
    // Cast RECT members (LONG) to int to satisfy std::min/max template.
    int rcRight  = static_cast<int>(rc.right);
    int rcBottom = static_cast<int>(rc.bottom);
    tx = std::max(PL + 2, std::min(tx, rcRight - tw - 4));

    int ty = PT + 6;
    if (ty + th + 4 > rcBottom) ty = rcBottom - th - 4;
    if (ty < 2) ty = 2;

    // Shadow (2px offset dark rect)
    RECT shadow = { tx+2, ty+2, tx+tw+2, ty+th+2 };
    fillRect(hdc, shadow, RGB(0, 0, 0));

    // Background
    RECT box = { tx, ty, tx+tw, ty+th };
    fillRect(hdc, box, RGB(25, 30, 40));

    // Border
    HPEN bp = pen(RGB(60, 70, 90));
    auto op = (HPEN)SelectObject(hdc, bp);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, tx, ty, tx+tw, ty+th);
    SelectObject(hdc, op); DeleteObject(bp);

    // Text
    RECT tr = { tx+8, ty+5, tx+tw-8, ty+th-5 };
    SetTextColor(hdc, RGB(220, 225, 235));
    SetBkMode(hdc, TRANSPARENT);
    DrawTextA(hdc, tip.c_str(), -1, &tr, DT_LEFT);

    if (oldFont) SelectObject(hdc, oldFont);
}

// ── Interaction ───────────────────────────────────────────────────────────────

void HistogramControl::onMouseMove(HWND hwnd, State& s, int x, int y) {
    RECT rc; GetClientRect(hwnd, &rc);
    int ni = bucketIdxAt(s, x, rc);
    if (ni != s.hoverIdx) { s.hoverIdx = ni; InvalidateRect(hwnd, NULL, FALSE); }
    if (s.selecting)      { s.selEndPx = x;  InvalidateRect(hwnd, NULL, FALSE); }
    // Track mouse leave
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
    TrackMouseEvent(&tme);
    (void)y;
}

void HistogramControl::onLDown(HWND hwnd, State& s, int x) {
    SetCapture(hwnd);
    s.selecting  = true;
    s.selStartPx = x;
    s.selEndPx   = x;
    InvalidateRect(hwnd, NULL, FALSE);
}

void HistogramControl::onLUp(HWND hwnd, State& s, int x) {
    ReleaseCapture();
    if (!s.selecting) return;
    s.selecting = false;
    s.selEndPx  = x;
    // Tiny drag (< 5px) = click → clear
    if (std::abs(s.selEndPx - s.selStartPx) < 5)
        s.selStartPx = s.selEndPx = -1;
    InvalidateRect(hwnd, NULL, FALSE);
    if (s.hwndParent) PostMessage(s.hwndParent, WM_HISTOGRAM_RANGE, 0, 0);
}

void HistogramControl::onDblClick(HWND hwnd, State& s) {
    s.selStartPx = s.selEndPx = -1;
    s.selecting  = false;
    InvalidateRect(hwnd, NULL, FALSE);
    if (s.hwndParent) PostMessage(s.hwndParent, WM_HISTOGRAM_RANGE, 0, 0);
}

// ── Window procedure ──────────────────────────────────────────────────────────

LRESULT CALLBACK HistogramControl::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    State* s = getState(hwnd);

    switch (msg) {
    case WM_CREATE: {
        State* ns  = new State();
        ns->hwnd   = hwnd;
        ns->hwndParent = GetParent(hwnd);
        // Slightly smaller font for axis labels
        LOGFONTA lf = {};
        GetObjectA(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
        lf.lfHeight = -10; // 10pt
        ns->hFont = CreateFontIndirectA(&lf);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)ns);
        return 0;
    }
    case WM_DESTROY:
        if (s) { if (s->hFont) DeleteObject(s->hFont); delete s; }
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        return 0;
    case WM_PAINT:
        if (s) onPaint(hwnd, *s);
        else { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); }
        return 0;
    case WM_ERASEBKGND:
        return 1; // prevent flicker — we repaint everything
    case WM_MOUSEMOVE:
        if (s) onMouseMove(hwnd, *s, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONDOWN:
        if (s) onLDown(hwnd, *s, (short)LOWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        if (s) onLUp(hwnd, *s, (short)LOWORD(lp));
        return 0;
    case WM_LBUTTONDBLCLK:
        if (s) onDblClick(hwnd, *s);
        return 0;
    case WM_MOUSELEAVE:
        if (s) { s->hoverIdx = -1; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

} // namespace LogAnalyzer

// Made with Bob
