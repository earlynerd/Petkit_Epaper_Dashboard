#include "histogram.h"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <Fonts/FreeSansBold12pt7b.h>

Histogram::Histogram(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h)
    : _gfx(gfx), _x(x), _y(y), _w(w), _h(h) {}

void Histogram::setTitle(const char *title) { _title = title; }
void Histogram::setXAxisLabel(const char *label) { _xAxisLabel = label; }
void Histogram::setYAxisLabel(const char *label) { _yAxisLabel = label; }
void Histogram::setBinCount(int bins) { _numBins = bins > 0 ? bins : 1; }

void Histogram::addSeries(const char *name, const std::vector<float> &data, uint16_t color)
{
    // Add a new series to our vector, initializing seriesMaxFreq to 0
    HistogramSeries newSeries;
    newSeries.name = name;
    newSeries.data = data;
    newSeries.bins = std::vector<int>(); // Explicitly create empty vector
    newSeries.color = color;
    newSeries.seriesMaxFreq = 0;

    _series.push_back(newSeries);
}

void Histogram::setNormalization(bool enabled)
{
    _normalize = enabled;
}

void Histogram::plot()
{
    if (_series.empty())
    {
        _gfx->setCursor(_x + 10, _y + 20);
        _gfx->print("No data to plot.");
        return;
    }

    // 1. Pre-process data to find ranges and frequencies
    processData();

    // 3. Draw the histogram bars
    drawBars();

    // 2. Draw the chart framework
    drawAxes();

    // 4. Draw legend
    // drawLegend();
}

void Histogram::processData()
{
    if (_series.empty())
        return;

    // Determine data range (min/max) across ALL datasets for a shared X-axis
    _minVal = HUGE_VALF;
    _maxVal = -HUGE_VALF;

    for (const auto &s : _series)
    {
        if (!s.data.empty())
        {
            _minVal = std::min(_minVal, *std::min_element(s.data.begin(), s.data.end()));
            _maxVal = std::max(_maxVal, *std::max_element(s.data.begin(), s.data.end()));
        }
    }

    // Handle case where all values are the same
    if (_minVal == _maxVal)
    {
        _minVal -= 1.0f;
        _maxVal += 1.0f;
    }
    _minVal = 0; // Forcing 0 min-val as in original code

    // Bin the data for each series
    float binWidth = (_maxVal - _minVal) / _numBins;
    _maxFreq = 0; // This will be the global max (if not normalizing) or 100 (if normalizing)

    for (auto &s : _series)
    {
        s.bins.assign(_numBins, 0);
        s.seriesMaxFreq = 0; // Reset series-specific max
        if (s.data.empty())
            continue;

        for (float val : s.data)
        {
            int binIndex = static_cast<int>((val - _minVal) / binWidth);
            if (binIndex >= _numBins)
                binIndex = _numBins - 1; // Put max value in last bin
            if (binIndex >= 0)
            {
                s.bins[binIndex]++;
            }
        }

        // Find the max frequency for *this* series
        if (!s.bins.empty())
        {
            if (_normalize)
            {
                s.seriesMaxFreq =  *std::max_element(s.bins.begin(), s.bins.end()) * 100 / s.data.size();
            }
            else s.seriesMaxFreq = *std::max_element(s.bins.begin(), s.bins.end());
        }

        // Update the global max frequency (only used if not normalizing)
        if (s.seriesMaxFreq > _maxFreq)
        {
            _maxFreq = s.seriesMaxFreq;
        }
    }

    // Now, set the axis scale
    if (_normalize)
    {
        if (_maxFreq == 0)
            _maxFreq = 10;
        else
            _maxFreq = static_cast<int>(ceil(_maxFreq * 1.05f));
    }
    else
    {
        // Add padding to the top of the Y axis (raw count mode)
        if (_maxFreq == 0)
            _maxFreq = 10;
        else
            _maxFreq = static_cast<int>(ceil(_maxFreq * 1.05f));
    }
}

void Histogram::drawAxes()
{
    // Define the actual plotting area inside paddings
    _plotX = _x + PADDING_LEFT;
    _plotY = _y + PADDING_TOP;
    _plotW = _w - PADDING_LEFT - PADDING_RIGHT;
    _plotH = _h - PADDING_TOP - PADDING_BOTTOM;

    // Draw axes lines
    _gfx->drawRect(_plotX, _plotY, _plotW, _plotH, AXIS_COLOR);

    // Draw Title
    if (_title)
    {
        int16_t tx, ty;
        uint16_t tw, th;
        _gfx->setFont(&FreeSansBold12pt7b);
        _gfx->setTextSize(0);
        _gfx->getTextBounds(_title, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_x + (_w - tw) / 2, _plotY - th / 2); // Adjusted y
        _gfx->setTextColor(TEXT_COLOR);
        _gfx->print(_title);
        _gfx->setTextSize(1);
        _gfx->setFont(NULL);
    }

    // Draw primary Y-axis (left) labels and ticks (shared axis)
    int numYTicks = 5;
    for (int i = 0; i <= numYTicks; ++i)
    {
        int16_t yPos = _plotY + _plotH - (i * _plotH / numYTicks);
        if (i == 0)
            _gfx->drawLine(_plotX - 5, _plotY + _plotH - 1, _plotX, _plotY + _plotH - 1, AXIS_COLOR);
        else
            _gfx->drawLine(_plotX - 5, yPos, _plotX, yPos, AXIS_COLOR);

        int labelVal;
        char label[10];

        if (_normalize)
        {
            labelVal = static_cast<int>(ceil((i * _maxFreq) / static_cast<float>(numYTicks)));
            sprintf(label, "%d%%", labelVal); // Add a percent sign
        }
        else
        {
            labelVal = static_cast<int>(ceil((i * _maxFreq) / static_cast<float>(numYTicks)));
            itoa(labelVal, label, 10);
        }

        int16_t tx, ty;
        uint16_t tw, th;
        _gfx->getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_plotX - tw - 8, yPos - th / 2); // Adjusted y
        _gfx->setTextColor(TEXT_COLOR);                  // Use standard text color
        _gfx->print(label);
    }
    // if (_yAxisLabel) {
    //_gfx->setCursor(_x + 5, _y + PADDING_TOP + _plotH/2);
    //_gfx->print(_yAxisLabel);
    //}
    _gfx->setTextColor(GxEPD_BLACK);

    // Draw X-axis labels and ticks
    int numXTicks = 8;
    // Adjust numXTicks if plot is too narrow
    if (_plotW < 250)
        numXTicks = 4;

    for (int i = 0; i <= numXTicks; ++i)
    {
        int16_t xPos = _plotX + (i * _plotW / numXTicks);
        if (i == numXTicks)
            _gfx->drawLine(_plotX + _plotW - 1, _plotY + _plotH, _plotX + _plotW - 1, _plotY + _plotH + 5, AXIS_COLOR);
        else
            _gfx->drawLine(xPos, _plotY + _plotH, xPos, _plotY + _plotH + 5, AXIS_COLOR);

        float labelVal = _minVal + (i * (_maxVal - _minVal) / numXTicks);
        char label[10];
        dtostrf(labelVal, 4, 1, label);

        int16_t tx, ty;
        uint16_t tw, th;
        _gfx->getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(xPos - tw / 2, _plotY + _plotH + 8); // Adjusted y
        _gfx->print(label);
    }
    if (_xAxisLabel)
    {
        int16_t tx, ty;
        uint16_t tw, th;
        _gfx->getTextBounds(_xAxisLabel, 0, 0, &tx, &ty, &tw, &th);
        _gfx->setCursor(_plotX + (_plotW - tw) / 2, _y + _h - th);
        _gfx->print(_xAxisLabel);
    }
}

void Histogram::drawBars()
{
    // Ensure plot dimensions are set
    _plotX = _x + PADDING_LEFT;
    _plotY = _y + PADDING_TOP;
    _plotW = _w - PADDING_LEFT - PADDING_RIGHT;
    _plotH = _h - PADDING_TOP - PADDING_BOTTOM;

    if (_maxFreq == 0 || _series.empty())
        return; // Nothing to draw

    int numSeries = _series.size();
    float barSlotWidth = static_cast<float>(_plotW) / _numBins;
    float barPadding = barSlotWidth * 0.1f;                   // 10% of the slot on each side is padding
    float drawableBarWidth = barSlotWidth - (2 * barPadding); // The total width for the bar(s) in a slot
    float barWidth = floor(drawableBarWidth / numSeries);     // Width of a single bar

    if (barWidth < 1)
        barWidth = 1;

    for (int i = 0; i < _numBins; ++i)
    {
        // Calculate the starting X for this bin slot
        int16_t binStartX = _plotX + (i * barSlotWidth) + barPadding;

        for (int j = 0; j < numSeries; ++j)
        {

            const auto &s = _series[j];
            int16_t barH = 0;

            if (_normalize)
            {
                if (s.seriesMaxFreq > 0)
                {
                    // Calculate height as a percentage of this series's max
                    // barH = static_cast<int16_t>((static_cast<float>(s.bins[i]) / s.seriesMaxFreq) * _plotH);
                    barH = static_cast<int16_t>(((static_cast<float>(s.bins[i]) / s.data.size()) * 100.0 / _maxFreq) * _plotH);
                }
            }
            else
            {
                if (_maxFreq > 0)
                {
                    // Calculate height based on global max frequency
                    barH = static_cast<int16_t>((static_cast<float>(s.bins[i]) / _maxFreq) * _plotH);
                }
            }

            if (barH > 0)
            {
                int16_t barStartX = binStartX + (j * barWidth);
#if (EPD_SELECT == 1002)
                _gfx->fillRect(barStartX, _plotY + _plotH - barH, barWidth, barH, s.color);
#elif (EPD_SELECT == 1001)
                switch (s.color)
                {
                case EPD_RED:
                    _gfx->fillRect(barStartX, _plotY + _plotH - barH, barWidth, barH, EPD_BLACK);
                    break;
                case EPD_BLUE:
                    drawCheckerRect(barStartX, _plotY + _plotH - barH, barWidth, barH);
                    break;
                case EPD_GREEN:
                    drawPatternRect(barStartX, _plotY + _plotH - barH, barWidth, barH);
                    break;
                case EPD_YELLOW:
                    drawHatchRect(barStartX, _plotY + _plotH - barH, barWidth, barH);
                    break;
                case EPD_BLACK:
                    _gfx->drawRect(barStartX, _plotY + _plotH - barH, barWidth, barH, EPD_BLACK);
                    break;
                }
#endif
            }
        }
    }
}

void Histogram::drawLegend()
{
    int16_t legendX = _plotX + 10;
    int16_t legendY = _y + 8; // Position legend near the top, below title padding
    int16_t markerW = 15;
    int16_t markerH = 10;
    int16_t spacing = 8;

    _gfx->setFont(NULL);
    _gfx->setTextSize(1);

    for (const auto &s : _series)
    {
#if (EPD_SELECT == 1002)
        _gfx->fillRect(legendX, legendY, markerW, markerH, s.color);
#elif (EPD_SELECT == 1001)
        switch (s.color)
        {
        case EPD_RED:
            _gfx->fillRect(legendX, legendY, markerW, markerH, EPD_BLACK);
            break;
        case EPD_BLUE:
            drawCheckerRect(legendX, legendY, markerW, markerH);
            break;
        case EPD_GREEN:
            drawPatternRect(legendX, legendY, markerW, markerH);
            break;
        case EPD_YELLOW:
            drawHatchRect(legendX, legendY, markerW, markerH);
            break;
        case EPD_BLACK:
            _gfx->drawRect(legendX, legendY, markerW, markerH, EPD_BLACK);
            break;
        }
#endif
        _gfx->setTextColor(TEXT_COLOR);
        _gfx->setCursor(legendX + markerW + 5, legendY + markerH / 2 - 4);
        _gfx->print(s.name);

        int16_t tx, ty;
        uint16_t tw, th;
        _gfx->getTextBounds(s.name, 0, 0, &tx, &ty, &tw, &th);
        legendX += markerW + tw + spacing + 10; // Move X for next legend item
    }
}

void Histogram::drawPatternRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    _gfx->drawRect(x, y, w, h, EPD_BLACK);

    if (w <= 0 || h <= 0)
        return;

    // Draw diagonal fill lines /// by drawing parallel lines of the form x-y=k.
    // We iterate k through the box dimensions and calculate the clipped start
    // and end points for the line segment that falls within the rectangle.
    for (int16_t k = 4; k < w + h; k += 4)
    { // Start at 4 to not draw on the border

        // Calculate the start point (bottom-left of the line segment)
        int16_t x1_rel = std::max(0, k - h);
        int16_t y1_rel = k - x1_rel - 1;

        // Calculate the end point (top-right of the line segment)
        int16_t x2_rel = std::min(w, k) - 1;
        int16_t y2_rel = k - x2_rel;

        // Convert relative coordinates to absolute screen coordinates and draw the line.
        _gfx->drawLine(x + x1_rel, y + y1_rel, x + x2_rel, y + y2_rel, EPD_BLACK);
    }
}

void Histogram::drawHatchRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    _gfx->drawRect(x, y, w, h, EPD_BLACK);

    if (w <= 0 || h <= 0)
        return;

    // Draw diagonal fill lines /// by drawing parallel lines of the form x-y=k.
    // We iterate k through the box dimensions and calculate the clipped start
    // and end points for the line segment that falls within the rectangle.
    for (int16_t k = 4; k < w + h; k += 4)
    { // Start at 4 to not draw on the border

        // Calculate the start point (bottom-left of the line segment)
        int16_t x1_rel = std::max(0, k - h);
        int16_t y1_rel = k - x1_rel - 1;

        // Calculate the end point (top-right of the line segment)
        int16_t x2_rel = std::min(w, k) - 1;
        int16_t y2_rel = k - x2_rel;

        // Convert relative coordinates to absolute screen coordinates and draw the line.
        _gfx->drawLine(x + x1_rel, y + y1_rel, x + x2_rel, y + y2_rel, EPD_BLACK);
    }
}

void Histogram::drawCheckerRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    bool checker = false;
    for (int y1 = y; y1 < y + h; y1++)
    {
        for (int x1 = x; x1 < x + w; x1++)
        {
            if (checker ^ ((x1 - x) % 2))
                _gfx->drawPixel(x1, y1, EPD_BLACK);
        }
        checker = !checker;
    }
    _gfx->drawRect(x, y, w, h, EPD_BLACK);
}