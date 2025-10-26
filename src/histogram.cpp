#include "histogram.h"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <Fonts/FreeSansBold12pt7b.h>


EpaperHistogram::EpaperHistogram(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h)
    : _gfx(gfx), _x(x), _y(y), _w(w), _h(h) {}

void EpaperHistogram::setTitle(const char* title) { _title = title; }
void EpaperHistogram::setXAxisLabel(const char* label) { _xAxisLabel = label; }
void EpaperHistogram::setYAxisLabel(const char* label) { _yAxisLabel = label; }
void EpaperHistogram::setY2AxisLabel(const char* label) { _y2AxisLabel = label; }
void EpaperHistogram::setDataLabels(const char* label1, const char* label2) { _Series1Label = label1; _Series2Label = label2;}
void EpaperHistogram::setBinCount(int bins) { _numBins = bins > 0 ? bins : 1; }

void EpaperHistogram::setData(const std::vector<float>& data1, const std::vector<float>& data2) {
    _data1 = data1;
    _data2 = data2;
}

void EpaperHistogram::plot() {
    if (_data1.empty() && _data2.empty()) {
        _gfx->setCursor(_x + 10, _y + 20);
        _gfx->print("No data to plot.");
        return;
    }
    //_gfx->setFont(&FreeSans9pt7b);
    //_gfx->setTextSize(0);
    // 1. Pre-process data to find ranges and frequencies
    processData();
    
    // 3. Draw the histogram bars
    drawBars();

    // 2. Draw the chart framework
    drawAxes();
    
    

    // 4. Draw legend if there are two datasets
    //if (!_data2.empty()) {
    //    drawLegend();
    //}
}

void EpaperHistogram::processData() {
    if (_data1.empty() && _data2.empty()) return;

    // Determine data range (min/max) across both datasets for a shared X-axis
    _minVal = HUGE_VALF;
    _maxVal = -HUGE_VALF;

    auto find_min_max = [&](const std::vector<float>& data) {
        if (!data.empty()) {
            _minVal = std::min(_minVal, *std::min_element(data.begin(), data.end()));
            _maxVal = std::max(_maxVal, *std::max_element(data.begin(), data.end()));
        }
    };

    find_min_max(_data1);
    find_min_max(_data2);
    
    // Handle case where all values are the same
    if (_minVal == _maxVal) {
        _minVal -= 1.0f;
        _maxVal += 1.0f;
    }
    _minVal = 0;
    // Bin the data
    _bins1.assign(_numBins, 0);
    _bins2.assign(_numBins, 0);
    float binWidth = (_maxVal - _minVal) / _numBins;

    auto fill_bins = [&](const std::vector<float>& data, std::vector<int>& bins) {
        for (float val : data) {
            int binIndex = static_cast<int>((val - _minVal) / binWidth);
            if (binIndex >= _numBins) binIndex = _numBins - 1; // Put max value in last bin
            if (binIndex >= 0) {
                 bins[binIndex]++;
            }
        }
    };
    
    if (!_data1.empty()) fill_bins(_data1, _bins1);
    if (!_data2.empty()) fill_bins(_data2, _bins2);

    // Find max frequency for Y-axis scaling.
    _maxFreq = 0;
    _maxFreq2 = 0;
    if (!_bins1.empty()) { _maxFreq = *std::max_element(_bins1.begin(), _bins1.end()); }
    if (!_bins2.empty()) { _maxFreq2 = *std::max_element(_bins2.begin(), _bins2.end()); }

    // Add padding to the top of the Y axes by scaling the max frequency up.
    if (_maxFreq == 0) _maxFreq = 10; // Default axis max if no data
    else _maxFreq = static_cast<int>(ceil(_maxFreq * 1.15f));

    if (!_data2.empty()) {
        if (_maxFreq2 == 0) _maxFreq2 = 10;
        else _maxFreq2 = static_cast<int>(ceil(_maxFreq2 * 1.15f));
    }
}


void EpaperHistogram::drawAxes() {
    // Define the actual plotting area inside paddings
    

    // Draw axes lines
    _gfx->drawRect(_plotX, _plotY, _plotW, _plotH, AXIS_COLOR);

    // Draw Title
    if (_title) {
        int16_t tx, ty; uint16_t tw, th;
        _gfx->setFont(&FreeSansBold12pt7b);
        _gfx->setTextSize(0);
        _gfx->getTextBounds(_title, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_x + (_w - tw) / 2, _y + PADDING_TOP - th/2);
        _gfx->setTextColor(TEXT_COLOR);
        _gfx->print(_title);
        _gfx->setTextSize(1);
        _gfx->setFont(NULL);
    }

    // Draw primary Y-axis (left) labels and ticks
    int numYTicks = 5;
    for (int i = 0; i <= numYTicks; ++i) {
        int16_t yPos = _plotY + _plotH - (i * _plotH / numYTicks);
        _gfx->drawLine(_plotX - 5, yPos, _plotX, yPos, AXIS_COLOR);
        
        int labelVal = static_cast<int>(ceil((i * _maxFreq) / static_cast<float>(numYTicks)));
        char label[10];
        itoa(labelVal, label, 10);
        _gfx->setTextColor(BAR_COLOR_1);
        int16_t tx, ty; uint16_t tw, th;
        _gfx->getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_plotX - tw - 8, yPos - th / 2);
        _gfx->print(label);
    }
     if (_yAxisLabel) {
        _gfx->setCursor(_x + 5, _y + PADDING_TOP + _plotH/2);
        _gfx->print(_yAxisLabel);
    }
    _gfx->setTextColor(GxEPD_BLACK);
    // Draw secondary Y-axis (right) if data2 exists
    if (!_data2.empty()) {
        for (int i = 0; i <= numYTicks; ++i) {
            int16_t yPos = _plotY + _plotH - (i * _plotH / numYTicks);
            _gfx->drawLine(_plotX + _plotW, yPos, _plotX + _plotW + 5, yPos, AXIS_COLOR);

            int labelVal = static_cast<int>(ceil((i * _maxFreq2) / static_cast<float>(numYTicks)));
            char label[10];
            itoa(labelVal, label, 10);
            _gfx->setTextColor(BAR_COLOR_2);
            int16_t tx, ty; uint16_t tw, th;
            _gfx->getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
            _gfx->setCursor(_plotX + _plotW + 8, yPos - th/2);
            _gfx->print(label);
            
        }
        if (_y2AxisLabel) {
            _gfx->setCursor(_x + _w - 35, _y + PADDING_TOP + _plotH/2);
            _gfx->print(_y2AxisLabel);
        }
        _gfx->setTextColor(GxEPD_BLACK);
    }


    // Draw X-axis labels and ticks
    int numXTicks = 8;
    for (int i = 0; i <= numXTicks; ++i) {
        int16_t xPos = _plotX + (i * _plotW / numXTicks);
        _gfx->drawLine(xPos, _plotY + _plotH, xPos, _plotY + _plotH + 5, AXIS_COLOR);

        float labelVal = _minVal + (i * (_maxVal - _minVal) / numXTicks);
        char label[10];
        dtostrf(labelVal, 4, 1, label);
        
        int16_t tx, ty; uint16_t tw, th;
        _gfx->getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(xPos - tw / 2, _plotY + _plotH + 8);
        _gfx->print(label);
    }
     if (_xAxisLabel) {
        int16_t tx, ty; uint16_t tw, th;
        _gfx->getTextBounds(_xAxisLabel, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_plotX + (_plotW - tw) / 2, _y + _h - th );
        _gfx->print(_xAxisLabel);
    }
}

void EpaperHistogram::drawBars() {
    _plotX = _x + PADDING_LEFT;
    _plotY = _y + PADDING_TOP;
    _plotW = _w - PADDING_LEFT - PADDING_RIGHT;
    _plotH = _h - PADDING_TOP - PADDING_BOTTOM;
    
    if (_maxFreq == 0 && _maxFreq2 == 0) return; // Nothing to draw

    float barSlotWidth = static_cast<float>(_plotW) / _numBins;
    float barPadding = barSlotWidth * 0.1f; // 10% of the slot on each side is padding
    float drawableBarWidth = barSlotWidth - (2 * barPadding); // The total width for the bar(s) in a slot
    
    for (int i = 0; i < _numBins; ++i) {
        // Calculate the starting X for the padded bar(s)
        int16_t barStartX = _plotX + (i * barSlotWidth) + barPadding;
        
        if (_data2.empty()) { // Single data series
            if (_maxFreq > 0) {
                int16_t barH = static_cast<int16_t>((static_cast<float>(_bins1[i]) / _maxFreq) * _plotH);
                if (barH > 0) {
                     _gfx->fillRect(barStartX, _plotY + _plotH - barH, floor(drawableBarWidth), barH, BAR_COLOR_1);
                }
            }
        } else { // Two data series, side-by-side with independent scales
            float halfDrawableWidth = floor(drawableBarWidth / 2.0f);

            // Series 1 (scaled against left axis)
            if (_maxFreq > 0) {
                int16_t barH1 = static_cast<int16_t>((static_cast<float>(_bins1[i]) / _maxFreq) * _plotH);
                if(barH1 > 0) {
                     _gfx->fillRect(barStartX, _plotY + _plotH - barH1, halfDrawableWidth, barH1, BAR_COLOR_1);
                }
            }
           
            // Series 2 (scaled against right axis)
            if (_maxFreq2 > 0) {
                int16_t barH2 = static_cast<int16_t>((static_cast<float>(_bins2[i]) / _maxFreq2) * _plotH);
                if(barH2 > 0) {
                     _gfx->fillRect(barStartX + halfDrawableWidth, _plotY + _plotH - barH2, halfDrawableWidth, barH2, BAR_COLOR_2);
                }
            }
        }
    }
}

void EpaperHistogram::drawLegend() {
    int16_t legendX = _plotX + 10;
    int16_t legendY = _plotY - 20;

    // Series 1
    _gfx->fillRect(legendX, legendY, 15, 10, BAR_COLOR_1);
    _gfx->setCursor(legendX + 20, legendY + 2);
    _gfx->print(_Series1Label);

    // Series 2
    _gfx->fillRect(legendX + 80, legendY, 15, 10, BAR_COLOR_2);
    _gfx->setCursor(legendX + 100, legendY + 2);
    _gfx->print(_Series2Label);
}


void EpaperHistogram::drawPatternRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    _gfx->drawRect(x, y, w, h, color);

    if (w <= 0 || h <= 0) return;

    // Draw diagonal fill lines /// by drawing parallel lines of the form x-y=k.
    // We iterate k through the box dimensions and calculate the clipped start
    // and end points for the line segment that falls within the rectangle.
    for (int16_t k = 4; k < w + h; k += 4) { // Start at 4 to not draw on the border
        
        // Calculate the start point (bottom-left of the line segment)
        int16_t x1_rel = std::max(0, k - h);
        int16_t y1_rel = k - x1_rel;

        // Calculate the end point (top-right of the line segment)
        int16_t x2_rel = std::min(w, k);
        int16_t y2_rel = k - x2_rel;

        // Convert relative coordinates to absolute screen coordinates and draw the line.
        _gfx->drawLine(x + x1_rel, y + y1_rel, x + x2_rel, y + y2_rel, color);
    }
}


