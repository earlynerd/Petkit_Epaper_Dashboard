// ScatterPlot.cpp

#include "ScatterPlot.h"
#include <time.h> // For timestamp formatting
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// Define plot margins for labels and axes
const int MARGIN_TOP = 35;
const int MARGIN_BOTTOM = 25;
const int MARGIN_LEFT = 35;
const int MARGIN_RIGHT = 35;

// Define colors (0=black, 15=white for this library)
const int PLOT_BLACK = 0;
const int PLOT_WHITE = 15;

// Constructor: Initializes the plot with its position and a reference to the framebuffer
ScatterPlot::ScatterPlot(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp, int x, int y, int width, int height)
    : display(disp), _x(x), _y(y), _width(width), _height(height) {}

void ScatterPlot::setDataSeries1(const std::vector<DataPoint> &data)
{
    _series1Data = data;
}

void ScatterPlot::setDataSeries2(const std::vector<DataPoint> &data)
{
    _series2Data = data;
}

void ScatterPlot::setLabels(const String &title, const String &xLabel, const String &yLabel)
{
    _title = title;
    _xLabel = xLabel;
    _yLabel = yLabel;
}

void ScatterPlot::setLegend(const String &series1Name, const String &series2Name)
{
    _series1Name = series1Name;
    _series2Name = series2Name;
}

void ScatterPlot::draw()
{
    // 1. Find min and max values for auto-scaling
    display->setTextSize(1);
    display->setFont(NULL);
    display->setTextColor(EPD_BLACK);

    if (_series1Data.empty() && _series2Data.empty())
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
            if (p.x < xMin)
                xMin = p.x;
            if (p.x > xMax)
                xMax = p.x;
            if (p.y < yMin)
                yMin = p.y;
            if (p.y > yMax)
                yMax = p.y;
        }
    };
    //yMin = 0;
    findMinMax(_series1Data);
    findMinMax(_series2Data);

    // Add a 5% padding to the ranges
    float xRange = xMax - xMin;
    float yRange = yMax - yMin;
    xMin -= xRange * 0.05;
    xMax += xRange * 0.05;
    yMin -= yRange * 0.05;
    yMax += yRange * 0.05;

    if (xMax == xMin)
    {
        xMax += 1.0;
        xMin -= 1.0;
    }
    if (yMax == yMin)
    {
        yMax += 1.0;
        yMin -= 1.0;
    }

    // 2. Clear the plot area (fill with white)
    //display->clearScreen();
    display->fillRect(_x, _y, _width, _height, GxEPD_WHITE);
    //yMin = 0;
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
        display->drawLine(plotAreaX - 5, yPos, plotAreaX, yPos, EPD_BLACK); // Tick mark
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
        display->setTextColor(EPD_BLACK);
        display->setCursor(plotAreaX - w - 8, yPos);
        display->setTextColor(EPD_BLUE);
        display->print(buffer);
    }

    // --- Draw X-Axis Ticks and Labels ---
    const int numXTicks = 12;
    for (int i = 0; i <= numXTicks; ++i)
    {
        int xPos = plotAreaX + (plotAreaWidth * i) / numXTicks;
        display->drawLine(xPos, plotAreaY + plotAreaHeight, xPos, plotAreaY + plotAreaHeight + 5, EPD_BLACK);
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
        display->setCursor(xPos - w + 16, plotAreaY + plotAreaHeight + h + 7);
        display->print(buffer);
    }

    // --- Draw Title and Axis Labels ---
    int16_t x = plotAreaX, y = plotAreaY, x1 = 100, y1 = 100;
    uint16_t w = 0, h = 0;
    display->setFont(&FreeSansBold12pt7b);
    display->getTextBounds(_title.c_str(), x, y, &x1, &y1, &w, &h);
    display->setTextColor(EPD_BLACK);
    display->setCursor(_x + _width / 2 - w / 2 - 20, MARGIN_TOP / 2 + h / 2);
    display->print(_title);
    //display->setCursor(_x + _width / 2 - _xLabel.length() * 3, _y + _height - 15);
    //display->print(_xLabel);
    display->setTextColor(EPD_BLACK);
    display->setFont(NULL);
    //display->setRotation(3);
    //display->setCursor(_x + (_height / 2) - (w / 2), _y + h + 3);
    //display->print(_yLabel);
    //display->setRotation(0);
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

    for (const auto &p : _series1Data)
    {
        int sx, sy;
        mapPoint(p, sx, sy);
        drawMarker1(sx, sy);
    }

    for (const auto &p : _series2Data)
    {
        int sx, sy;
        mapPoint(p, sx, sy);
        drawMarker2(sx, sy);
    }
}
/*
void ScatterPlot::drawLegend()
{
    int16_t x = _x, y = _y, x1 = 0, y1 = 0;
    uint16_t series1width, series1height, series2width, series2height;
    display->setTextSize(1);
    display->setFont(NULL);
    display->setTextColor(EPD_BLACK);
    display->getTextBounds(_series1Name.c_str(), x, y, &x1, &y1, &series1width, &series1height);
    display->getTextBounds(_series2Name.c_str(), x, y, &x1, &y1, &series2width, &series2height);
    int32_t widest = (series1width > series2width) ? series1width : series2width;
    int legendX = _x + _width - widest - 5 - MARGIN_RIGHT - 3;
    int legendY = _y + MARGIN_TOP + _height / 2 - (series1height + series2height + 16 + 10)/2;
    display->fillRect(legendX - 8, legendY - series1height - 8, widest + 16 + 15, series1height + series2height + 16 + 10, EPD_WHITE);
    display->drawRect(legendX - 8, legendY - series1height - 8, widest + 16 + 15, series1height + series2height + 16 + 10, EPD_BLACK);
    drawMarker1(legendX, legendY - series1height / 2);
    display->setCursor(legendX + 15, legendY);
    display->print(_series1Name);
    drawMarker2(legendX, legendY + series1height + 10 - series2height / 2);
    display->setCursor(legendX + 15, legendY + series1height + 10);
    display->print(_series2Name);
}
*/
void ScatterPlot::drawLegend() {
    int markerw = 15, markerh = 10;
    int16_t legendX = _x +MARGIN_LEFT ;
    int16_t legendY = _y + MARGIN_TOP/2 - markerh/2;
    display->setFont(&FreeSans9pt7b);
    display->setTextSize(0);
    int16_t  x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    // Series 1
    display->getTextBounds(_series1Name, legendX, legendY, &x1, &y1, &w, &h);
    display->fillRect(legendX, legendY, markerw, markerh, EPD_RED);
    display->setCursor(legendX + markerw +5,  _y + MARGIN_TOP/2 - h/2 + 12);    // minus text heighh
    display->print(_series1Name);

    // Series 2
    display->fillRect(legendX + markerw + 40 + w, legendY, markerw, markerh, EPD_BLUE);
    display->setCursor(legendX + 2* markerw + 40 + w + 10, _y + MARGIN_TOP/2 -h/2 + 12);
    display->print(_series2Name);
}

void ScatterPlot::drawMarker1(int x, int y)
{
    display->fillCircle(x, y, 2, EPD_RED);

}

void ScatterPlot::drawMarker2(int x, int y)
{
    display->fillCircle(x, y, 2, EPD_BLUE);
}

// Helper function to simplify drawing text
void ScatterPlot::drawString(int x, int y, const String &text, const GFXfont *font, uint8_t color)
{
    int cursor_x = x;
    int cursor_y = y;
    display->setFont(NULL);
    display->setTextSize(1);
    display->setTextColor(EPD_BLACK);
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
    //y = 2;
    drawString(x, h+1, strftime_buf, &FreeMonoBold9pt7b, EPD_BLACK);

  
    int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    float battery_voltage = (mv / 1000.0) * 2;

    if (battery_voltage >= 4.2)
    {
        battery_voltage = 4.2;
    }
    char buffer[32];
    sprintf(buffer, "Battery: %.2fV", battery_voltage);
    display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
    drawString(x , 2*h + 4, buffer, &FreeMonoBold9pt7b, EPD_BLACK);
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