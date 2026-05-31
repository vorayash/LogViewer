#include "ChartTemplate.h"
#include <sstream>

namespace LogAnalyzer {

const char* ChartTemplate::getHTMLTemplate() {
    // Self-contained: no CDN dependencies, draws charts with plain Canvas 2D API.
    return R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Log Analyzer Charts</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0; padding: 20px;
            background-color: #f0f2f5;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #1a1a2e; margin-bottom: 20px; font-size: 22px; }
        .stats {
            display: flex; gap: 16px; margin-bottom: 24px; flex-wrap: wrap;
        }
        .stat-box {
            background: white; border-radius: 10px; padding: 18px 24px;
            text-align: center; flex: 1; min-width: 120px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.08);
        }
        .stat-value { font-size: 36px; font-weight: 700; }
        .stat-label { font-size: 13px; color: #666; margin-top: 4px; }
        .stat-total .stat-value { color: #1976D2; }
        .stat-error .stat-value { color: #D32F2F; }
        .stat-warn  .stat-value { color: #F57C00; }
        .stat-info  .stat-value { color: #388E3C; }
        .chart-card {
            background: white; border-radius: 10px; padding: 20px;
            margin-bottom: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.08);
        }
        .chart-card h2 {
            margin: 0 0 16px 0; font-size: 16px; color: #333;
            border-bottom: 2px solid #4CAF50; padding-bottom: 8px;
        }
        canvas { display: block; width: 100%; }
        .no-data { color: #999; font-style: italic; padding: 20px 0; text-align: center; }
    </style>
</head>
<body>
<div class="container">
    <h1>Log Analyzer &mdash; Visualization</h1>
    <div class="stats">
        <div class="stat-box stat-total">
            <div class="stat-value" id="totalLogs">0</div>
            <div class="stat-label">Total Logs</div>
        </div>
        <div class="stat-box stat-error">
            <div class="stat-value" id="errorCount">0</div>
            <div class="stat-label">Errors</div>
        </div>
        <div class="stat-box stat-warn">
            <div class="stat-value" id="warnCount">0</div>
            <div class="stat-label">Warnings</div>
        </div>
        <div class="stat-box stat-info">
            <div class="stat-value" id="infoCount">0</div>
            <div class="stat-label">Info</div>
        </div>
    </div>
    <div class="chart-card">
        <h2>Log Level Distribution</h2>
        <canvas id="levelCanvas" height="260"></canvas>
    </div>
    <div class="chart-card">
        <h2>Timeline &mdash; Logs Over Time</h2>
        <canvas id="timelineCanvas" height="280"></canvas>
    </div>
</div>
<script>
    const levelData    = __LEVEL_DATA__;
    const timelineData = __TIMELINE_DATA__;

    document.getElementById('totalLogs').textContent  = levelData.total  || 0;
    document.getElementById('errorCount').textContent = levelData.ERROR  || 0;
    document.getElementById('warnCount').textContent  = levelData.WARN   || 0;
    document.getElementById('infoCount').textContent  = levelData.INFO   || 0;

    // ── Colour palette keyed by label ──────────────────────────────────────
    const LEVEL_COLORS = {
        TRACE:   { fill:'rgba(156,39,176,0.75)', border:'rgb(156,39,176)' },
        DEBUG:   { fill:'rgba(33,150,243,0.75)', border:'rgb(33,150,243)' },
        INFO:    { fill:'rgba(76,175,80,0.75)',  border:'rgb(76,175,80)'  },
        WARN:    { fill:'rgba(255,152,0,0.85)',  border:'rgb(255,152,0)'  },
        ERROR:   { fill:'rgba(244,67,54,0.85)',  border:'rgb(244,67,54)'  },
        FATAL:   { fill:'rgba(139,0,0,0.90)',    border:'rgb(139,0,0)'    },
        UNKNOWN: { fill:'rgba(158,158,158,0.7)', border:'rgb(158,158,158)'}
    };

    // ── Shared drawing helpers ─────────────────────────────────────────────
    function setupCanvas(canvas) {
        const dpr = window.devicePixelRatio || 1;
        const rect = canvas.getBoundingClientRect();
        const w = rect.width  || canvas.offsetWidth  || 800;
        const h = parseInt(canvas.getAttribute('height')) || 260;
        canvas.width  = w * dpr;
        canvas.height = h * dpr;
        canvas.style.height = h + 'px';
        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);
        return { ctx, w, h };
    }

    function measureLabels(ctx, labels, font) {
        ctx.font = font;
        return labels.map(l => ctx.measureText(String(l)).width);
    }

    // ── Bar chart ─────────────────────────────────────────────────────────
    function drawBarChart(canvasId, labels, values) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;
        const { ctx, w, h } = setupCanvas(canvas);

        if (!labels || labels.length === 0) {
            ctx.fillStyle = '#aaa';
            ctx.font = '14px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('No data', w/2, h/2);
            return;
        }

        const PAD_LEFT = 52, PAD_RIGHT = 16, PAD_TOP = 16, PAD_BOTTOM = 40;
        const chartW = w - PAD_LEFT - PAD_RIGHT;
        const chartH = h - PAD_TOP  - PAD_BOTTOM;

        const maxVal = Math.max(...values, 1);
        const barW   = Math.max(4, (chartW / labels.length) * 0.65);
        const gap    = (chartW / labels.length) * 0.35;

        // Y grid lines
        const steps = 5;
        ctx.strokeStyle = '#e8e8e8';
        ctx.lineWidth   = 1;
        ctx.fillStyle   = '#888';
        ctx.font        = '11px sans-serif';
        ctx.textAlign   = 'right';
        for (let i = 0; i <= steps; i++) {
            const yv  = (maxVal / steps) * i;
            const y   = PAD_TOP + chartH - (yv / maxVal) * chartH;
            ctx.beginPath(); ctx.moveTo(PAD_LEFT, y); ctx.lineTo(PAD_LEFT + chartW, y); ctx.stroke();
            ctx.fillText(Math.round(yv), PAD_LEFT - 6, y + 4);
        }

        // Bars
        labels.forEach((label, i) => {
            const c   = LEVEL_COLORS[label] || LEVEL_COLORS.UNKNOWN;
            const x   = PAD_LEFT + i * (barW + gap) + gap / 2;
            const bh  = (values[i] / maxVal) * chartH;
            const y   = PAD_TOP + chartH - bh;

            ctx.fillStyle = c.fill;
            ctx.strokeStyle = c.border;
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.roundRect ? ctx.roundRect(x, y, barW, bh, [4,4,0,0])
                          : ctx.rect(x, y, barW, bh);
            ctx.fill(); ctx.stroke();

            // Value label on top
            if (values[i] > 0) {
                ctx.fillStyle = '#333';
                ctx.font = 'bold 11px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText(values[i], x + barW/2, y - 4);
            }

            // X axis label
            ctx.fillStyle = '#555';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText(label, x + barW/2, PAD_TOP + chartH + 18);
        });

        // Axes
        ctx.strokeStyle = '#ccc';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(PAD_LEFT, PAD_TOP);
        ctx.lineTo(PAD_LEFT, PAD_TOP + chartH);
        ctx.lineTo(PAD_LEFT + chartW, PAD_TOP + chartH);
        ctx.stroke();
    }

    // ── Line chart ────────────────────────────────────────────────────────
    function drawLineChart(canvasId, labels, values) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;
        const { ctx, w, h } = setupCanvas(canvas);

        if (!labels || labels.length === 0) {
            ctx.fillStyle = '#aaa';
            ctx.font = '14px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('No data', w/2, h/2);
            return;
        }

        const PAD_LEFT = 52, PAD_RIGHT = 16, PAD_TOP = 16, PAD_BOTTOM = 64;
        const chartW = w - PAD_LEFT - PAD_RIGHT;
        const chartH = h - PAD_TOP  - PAD_BOTTOM;
        const n      = labels.length;
        const maxVal = Math.max(...values, 1);

        // Y grid
        const steps = 5;
        ctx.strokeStyle = '#e8e8e8';
        ctx.lineWidth   = 1;
        ctx.fillStyle   = '#888';
        ctx.font        = '11px sans-serif';
        ctx.textAlign   = 'right';
        for (let i = 0; i <= steps; i++) {
            const yv = (maxVal / steps) * i;
            const y  = PAD_TOP + chartH - (yv / maxVal) * chartH;
            ctx.beginPath(); ctx.moveTo(PAD_LEFT, y); ctx.lineTo(PAD_LEFT + chartW, y); ctx.stroke();
            ctx.fillText(Math.round(yv), PAD_LEFT - 6, y + 4);
        }

        // Points
        const pts = values.map((v, i) => ({
            x: PAD_LEFT + (n === 1 ? chartW/2 : (i / (n-1)) * chartW),
            y: PAD_TOP + chartH - (v / maxVal) * chartH
        }));

        // Filled area
        ctx.beginPath();
        ctx.moveTo(pts[0].x, PAD_TOP + chartH);
        pts.forEach(p => ctx.lineTo(p.x, p.y));
        ctx.lineTo(pts[pts.length-1].x, PAD_TOP + chartH);
        ctx.closePath();
        ctx.fillStyle = 'rgba(76,175,80,0.12)';
        ctx.fill();

        // Line
        ctx.beginPath();
        ctx.strokeStyle = 'rgb(56,142,60)';
        ctx.lineWidth   = 2.5;
        ctx.lineJoin    = 'round';
        pts.forEach((p, i) => i === 0 ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y));
        ctx.stroke();

        // Dots
        pts.forEach((p, i) => {
            ctx.beginPath();
            ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
            ctx.fillStyle   = 'white';
            ctx.strokeStyle = 'rgb(56,142,60)';
            ctx.lineWidth   = 2;
            ctx.fill(); ctx.stroke();

            // Tooltip-style value
            if (values[i] > 0) {
                ctx.fillStyle = '#333';
                ctx.font = '10px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText(values[i], p.x, p.y - 10);
            }
        });

        // X axis labels (rotated 45 deg, skip to avoid overlap)
        const maxLabels = Math.floor(chartW / 80);
        const step      = Math.max(1, Math.ceil(n / maxLabels));
        ctx.fillStyle   = '#555';
        ctx.font        = '10px sans-serif';
        ctx.save();
        for (let i = 0; i < n; i += step) {
            ctx.save();
            ctx.translate(pts[i].x, PAD_TOP + chartH + 10);
            ctx.rotate(Math.PI / 4);
            ctx.textAlign = 'left';
            ctx.fillText(labels[i], 0, 0);
            ctx.restore();
        }
        ctx.restore();

        // Axes
        ctx.strokeStyle = '#ccc';
        ctx.lineWidth   = 1;
        ctx.beginPath();
        ctx.moveTo(PAD_LEFT, PAD_TOP);
        ctx.lineTo(PAD_LEFT, PAD_TOP + chartH);
        ctx.lineTo(PAD_LEFT + chartW, PAD_TOP + chartH);
        ctx.stroke();
    }

    // ── Render both charts after layout is complete ────────────────────────
    window.addEventListener('load', function() {
        drawBarChart('levelCanvas', levelData.labels, levelData.values);
        drawLineChart('timelineCanvas', timelineData.labels, timelineData.values);
    });
</script>
</body>
</html>
)";
}

std::string ChartTemplate::generateHTML(
    const std::string& levelDataJson,
    const std::string& timelineDataJson
) {
    std::string html = getHTMLTemplate();
    
    // Replace placeholders with actual data
    size_t pos = html.find("__LEVEL_DATA__");
    if (pos != std::string::npos) {
        html.replace(pos, 14, levelDataJson);
    }
    
    pos = html.find("__TIMELINE_DATA__");
    if (pos != std::string::npos) {
        html.replace(pos, 17, timelineDataJson);
    }
    
    return html;
}

} // namespace LogAnalyzer

// Made with Bob
