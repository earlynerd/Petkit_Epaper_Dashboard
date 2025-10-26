#ifndef EPAPER_HISTOGRAM_H
#define EPAPER_HISTOGRAM_H

#include <GxEPD2_7C.h>
#include <vector>
#include "config.h"


// Note: Colors are typically GxEPD_BLACK and GxEPD_WHITE for e-paper,
// but the class uses the standard Adafruit_GFX defines (BLACK/WHITE)
// for broader compatibility.

class EpaperHistogram {
public:
    /**
     * @brief Construct a new Epaper Histogram object
     * * @param gfx Pointer to your initialized Adafruit_GFX compatible display object.
     * @param x The x-coordinate for the top-left corner of the chart area.
     * @param y The y-coordinate for the top-left corner of the chart area.
     * @param w The width of the chart area.
     * @param h The height of the chart area.
     */
    EpaperHistogram(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);

    /**
     * @brief Set the main title of the histogram.
     * @param title The title string.
     */
    void setTitle(const char* title);

       /**
     * @brief Set the label for the X-axis.
     * @param label The label string.
     */
    void setDataLabels(const char* label1, const char* label2);
    /**
     * @brief Set the label for the X-axis.
     * @param label The label string.
     */
    void setXAxisLabel(const char* label);
    
    /**
     * @brief Set the label for the primary Y-axis (left side).
     * @param label The label string.
     */
    void setYAxisLabel(const char* label);

    /**
     * @brief Set the label for the secondary Y-axis (right side).
     * @param label The label string.
     */
    void setY2AxisLabel(const char* label);

    /**
     * @brief Set the number of bins to divide the data into.
     * @param bins The number of bins (default is 10).
     */
    void setBinCount(int bins);

    /**
     * @brief Provide the data series to be plotted.
     * * @param data1 A vector of floats for the first data series (plotted on left axis).
     * @param data2 (Optional) A vector of floats for the second data series (plotted on right axis).
     */
    void setData(const std::vector<float>& data1, const std::vector<float>& data2 = {});

    /**
     * @brief Draw the histogram on the display.
     * This should be called within your display update loop (e.g., between
     * display.firstPage() and display.nextPage() for GxEPD2).
     */
    void plot();

private:
    void processData();
    void drawAxes();
    void drawBars();
    void drawLegend();
    void drawPatternRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    Adafruit_GFX* _gfx;
    int16_t _x, _y, _w, _h; // Overall widget position and size

    // Plotting area (inside the widget area, with padding for labels/title)
    int16_t _plotX, _plotY, _plotW, _plotH;

    const char* _title = nullptr;
    const char* _xAxisLabel = nullptr;
    const char* _yAxisLabel = nullptr;
    const char* _y2AxisLabel = nullptr;

    const char* _Series1Label = nullptr;
    const char* _Series2Label = nullptr;

    int _numBins = 10;
    std::vector<float> _data1;
    std::vector<float> _data2;

    float _minVal = 0.0f;
    float _maxVal = 0.0f;
    int _maxFreq = 0;
    int _maxFreq2 = 0;

    std::vector<int> _bins1;
    std::vector<int> _bins2;

    // Constants for layout and styling
    const int PADDING_TOP = 35;
    const int PADDING_BOTTOM = 35;
    const int PADDING_LEFT = 35;
    const int PADDING_RIGHT = 35; // Increased to make room for secondary axis
    const uint16_t AXIS_COLOR = GxEPD_BLACK;
    const uint16_t TEXT_COLOR = GxEPD_BLACK;
    const uint16_t BAR_COLOR_1 = GxEPD_RED;
    const uint16_t BAR_COLOR_2 = GxEPD_BLUE;
};

#endif // EPAPER_HISTOGRAM_H

