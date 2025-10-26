// ScatterPlot.cpp

#include "ScatterPlot.h"
#include <time.h> // For timestamp formatting
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// Define plot margins for labels and axes
const int MARGIN_TOP = 30;
const int MARGIN_BOTTOM = 25;
const int MARGIN_LEFT = 35;
const int MARGIN_RIGHT = 20;

// Define colors (0=black, 15=white for this library)
const int PLOT_BLACK = 0;
const int PLOT_WHITE = 15;

// Constructor: Initializes the plot with its position and a reference to the framebuffer
ScatterPlot::ScatterPlot(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp, int x, int y, int width, int height)
    : display(disp), _x(x), _y(y), _width(width), _height(height) {}


void ScatterPlot::addSeries(const String& name, const std::vector<DataPoint>& data, uint16_t color) {
    _series.push_back({name, data, color});
}

void ScatterPlot::setLabels(const String &title, const String &xLabel, const String &yLabel)
{
    _title = title;
    _xLabel = xLabel;
    _yLabel = yLabel;
}

void ScatterPlot::draw()
{
    // 1. Find min and max values for auto-scaling
    display->setTextSize(1);
    display->setFont(NULL);
    display->setTextColor(EPD_BLACK);

    if (_series.empty())
    {
        drawAxes(0, 10, 0, 10);
        drawLegend();
        return; // Nothing to draw
    }
    float xMin = 1.0e38, xMax = -1.0e38, yMin = 1.0e38, yMax = -1.0e38;

    auto findMinMax = [&](const std::vector<DataPoint> &data)
    {
        for (const auto &p : data)
        {
            if (p.x < xMin) xMin = p.x;
            if (p.x > xMax) xMax = p.x;
            if (p.y < yMin) yMin = p.y;
            if (p.y > yMax) yMax = p.y;
        }
    };
    
    // Iterate over all series to find global min/max
    for (const auto& s : _series) {
        findMinMax(s.data);
    }

    // Add a 5% padding to the ranges
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    xMin -= xRange * 0.05;
    xMax += xRange * 0.05;
    yMin -= yRange * 0.05;
    yMax += yRange * 0.05;

    if (xMax == xMin) { xMax += 1.0; xMin -= 1.0; }
    if (yMax == yMin) { yMax += 1.0; yMin -= 1.0; }

    // 2. Clear the plot area (fill with white)
    display->fillRect(_x, _y, _width, _height, GxEPD_WHITE);
    
    // 3. Draw components
    drawAxes(xMin, xMax, yMin, yMax);
    plotDataPoints(xMin, xMax, yMin, yMax);
    drawLegend();
    add_refresh_timestamp();
}

void ScatterPlot::drawAxes(float xMin, float xMax, float yMin, float yMax)
{
    int plotAreaX = _x + MARGIN_LEFT;
    int plotAreaY = _y + MARGIN_TOP;
    int plotAreaWidth = _width - MARGIN_LEFT - MARGIN_RIGHT;
    int plotAreaHeight = _height - MARGIN_TOP - MARGIN_BOTTOM;

    // Draw axis lines
    display->drawRect(plotAreaX, plotAreaY, plotAreaWidth, plotAreaHeight, EPD_BLACK);

    // --- Draw Y-Axis Ticks and Labels ---
    const int numYTicks = 6;
    for (int i = 0; i <= numYTicks; ++i)
    {
        int yPos = plotAreaY + (plotAreaHeight * i) / numYTicks;
        if (i == numYTicks) display->drawLine(plotAreaX - 5, plotAreaY + plotAreaHeight-1, plotAreaX, plotAreaY + plotAreaHeight-1, EPD_BLACK);
        else display->drawLine(plotAreaX - 5, yPos, plotAreaX, yPos, EPD_BLACK); // Tick mark
        if (i < numYTicks)
            drawDashedLine(plotAreaX, yPos, plotAreaX + plotAreaWidth, yPos, EPD_BLACK, 1, 2); // grid line
        float labelVal = yMax - (yMax - yMin) * i / numYTicks;
        char buffer[10];
        dtostrf(labelVal, 4, 1, buffer);
        int16_t x = plotAreaX, y = plotAreaY, x1 = 0, y1 = 0;
        uint16_t w = 0, h = 0;
        display->setFont(NULL);
        display->setTextSize(1);
        display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
        display->setTextColor(EPD_BLACK); // Original code used blue for Y-axis labels
        display->setCursor(plotAreaX - w - 8, yPos - h/2); // Centered text
        display->print(buffer);
    }

    // --- Draw X-Axis Ticks and Labels ---
    const int numXTicks = 12;
    for (int i = 0; i <= numXTicks; ++i)
    {
        int xPos = plotAreaX + (plotAreaWidth * i) / numXTicks;
        if(i == numXTicks) display->drawLine(plotAreaX+plotAreaWidth -1 , plotAreaY + plotAreaHeight, plotAreaX+plotAreaWidth -1, plotAreaY + plotAreaHeight + 5, EPD_BLACK);
        else display->drawLine(xPos, plotAreaY + plotAreaHeight, xPos, plotAreaY + plotAreaHeight + 5, EPD_BLACK);
        if (i < numXTicks)
            drawDashedLine(xPos, plotAreaY, xPos, plotAreaY + plotAreaHeight, EPD_BLACK, 1, 2);
        float labelVal = xMin + (xMax - xMin) * i / numXTicks;

        // Convert timestamp to Month/Day format
        char buffer[10];
        time_t tick_time = (time_t)labelVal;
        struct tm *tm_info = localtime(&tick_time);
        strftime(buffer, sizeof(buffer), "%m/%d", tm_info);
        int16_t x = plotAreaX, y = plotAreaY, x1 = 0, y1 = 0;
        uint16_t w = 0, h = 0;
        display->setFont(NULL);
        display->setTextSize(1);
        display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
        display->setTextColor(EPD_BLACK);
        display->setCursor(xPos - w/2, plotAreaY + plotAreaHeight + 7); // Centered text
        display->print(buffer);
    }

    // --- Draw Title and Axis Labels ---
    int16_t x = plotAreaX, y = plotAreaY, x1 = 100, y1 = 100;
    uint16_t w = 0, h = 0;
    display->setFont(&FreeSansBold12pt7b);
    display->getTextBounds(_title.c_str(), x, y, &x1, &y1, &w, &h);
    display->setTextColor(EPD_BLACK);
    display->setCursor(_x + (_width - w) / 2, MARGIN_TOP - h/2); // Centered title
    display->print(_title);
    
    display->setTextColor(EPD_BLACK);
    display->setFont(NULL);
    // Y-Axis label (rotated text is hard, skipping for brevity, was missing in original)
    // X-Axis label (was missing in original)
}

void ScatterPlot::plotDataPoints(float xMin, float xMax, float yMin, float yMax)
{
    int plotAreaX = _x + MARGIN_LEFT;
    int plotAreaY = _y + MARGIN_TOP;
    int plotAreaWidth = _width - MARGIN_LEFT - MARGIN_RIGHT;
    int plotAreaHeight = _height - MARGIN_TOP - MARGIN_BOTTOM;

    auto mapPoint = [&](const DataPoint &p, int &screenX, int &screenY)
    {
        screenX = plotAreaX + ((p.x - xMin) / (xMax - xMin)) * plotAreaWidth;
        screenY = (plotAreaY + plotAreaHeight) - ((p.y - yMin) / (yMax - yMin)) * plotAreaHeight;
    };

    // Loop over all series and plot their points
    for (const auto& s : _series) {
        for (const auto &p : s.data)
        {
            int sx, sy;
            mapPoint(p, sx, sy);
            drawMarker(sx, sy, s.color);
        }
    }
}

void ScatterPlot::drawLegend() {
    int markerw = 15, markerh = 10;
    int16_t legendX = _x + MARGIN_LEFT;
    int16_t legendY = _y + MARGIN_TOP/2 - markerh/2; // Top-left legend
    int16_t spacing = 10;

    display->setFont(&FreeSans9pt7b);
    display->setTextSize(0);
    int16_t  x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    
    for (const auto& s : _series) {
        display->getTextBounds(s.name, legendX, legendY, &x1, &y1, &w, &h);
        drawMarker(legendX + markerw/2, legendY + markerh/2, s.color);
        //display->fillRect(legendX, legendY, markerw, markerh, s.color);
        display->setCursor(legendX + markerw + 5,  _y + MARGIN_TOP/2 + h/2);
        display->print(s.name);

        // Move X for next legend item
        legendX += markerw + w + spacing + 5;
    }
}

void ScatterPlot::drawMarker(int x, int y, uint16_t color)
{
    #if (EPD_SELECT == 1002)
        // You could customize this to draw different markers based on series index
        // For now, all series use a filled circle
        display->fillCircle(x, y, 2, color);
        
    #elif (EPD_SELECT == 1001)
        switch(color)
        {
            case EPD_RED:
                display->fillCircle(x, y, 2, EPD_BLACK);
            break;
            case EPD_BLUE:
                display->drawCircle(x, y, 2, EPD_BLACK);
            break;
            case EPD_GREEN:
                display->drawRect(x-1, y+1, 4, 4, EPD_BLACK);
            break;
            case EPD_YELLOW:
                display->fillRect(x-1, y+1, 4, 4, EPD_BLACK);
            break;
            case EPD_BLACK:
                display->drawLine(x-2, y+2, x+2, y-2, EPD_BLACK);
                display->drawLine(x+1, y-1, x-1, y+1, EPD_BLACK);
            break;
        }
    #endif
}

// Helper function to simplify drawing text
void ScatterPlot::drawString(int x, int y, const String &text, const GFXfont *font, uint8_t color)
{
    int cursor_x = x;
    int cursor_y = y;
    display->setFont(font); // Use the provided font
    display->setTextSize(1);
    display->setTextColor(color); // Use the provided color
    display->setCursor(x, y);
    display->print(text);
}

void ScatterPlot::add_refresh_timestamp()
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64]; // Buffer to hold the formatted string

    int16_t x = 0, y = 0, x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;

    time(&now);                   // Get current epoch time
    localtime_r(&now, &timeinfo); // Convert to struct tm
    strftime(strftime_buf, sizeof(strftime_buf), "%m/%d/%y %H:%M", &timeinfo);
    display->setFont(NULL);
    display->setTextSize(1);
    display->getTextBounds(strftime_buf, x, y, &x1, &y1, &w, &h);
    x = EPD_WIDTH - w - 5;
    
    drawString(x, h+1, strftime_buf, NULL, EPD_BLACK); // Use NULL font for default

    int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    float battery_voltage = (mv / 1000.0) * 2;

    if (battery_voltage >= 4.2)
    {
        battery_voltage = 4.2;
    }
    char buffer[32];
    sprintf(buffer, "Battery: %.2fV", battery_voltage);
    display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
    drawString(EPD_WIDTH - w - 5 , 2*h + 4, buffer, NULL, EPD_BLACK); // Use NULL font
}

void ScatterPlot::drawDashedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, uint16_t dashLength, uint16_t spaceLength)
{
    // Ensure dash and space lengths are positive to avoid infinite loops
    if (dashLength == 0 || spaceLength == 0)
    {
        display->drawLine(x0, y0, x1, y1, color);
        return;
    }

    // Calculate the total length of the line
    float dx = x1 - x0;
    float dy = y1 - y0;
    float totalLength = sqrt(dx * dx + dy * dy);

    // Calculate the length of one dash-space cycle
    float cycleLength = dashLength + spaceLength;

    // Start at the beginning of the line
    float currentPos = 0.0;

    while (currentPos < totalLength)
    {
        // Calculate the starting point of the current dash
        float startX = x0 + (dx * currentPos) / totalLength;
        float startY = y0 + (dy * currentPos) / totalLength;

        // Determine the end position of the dash
        float dashEndPos = currentPos + dashLength;

        // If the dash goes past the end of the line, clamp it
        if (dashEndPos > totalLength)
        {
            dashEndPos = totalLength;
        }

        // Calculate the ending point of the current dash
        float endX = x0 + (dx * dashEndPos) / totalLength;
        float endY = y0 + (dy * dashEndPos) / totalLength;

        // Draw the dash segment
        display->drawLine(round(startX), round(startY), round(endX), round(endY), color);

        // Move the current position to the start of the next dash
        currentPos += cycleLength;
    }
}
