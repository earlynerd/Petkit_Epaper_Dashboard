#ifndef EPAPER_HISTOGRAM_H
#define EPAPER_HISTOGRAM_H
#include "config.h"
#if (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#elif (EPD_SELECT == 1001)
#include <GxEPD2_BW.h>
#endif
#include <vector>


 struct HistogramSeries {
        const char* name;
        std::vector<float> data;
        std::vector<int> bins;
        uint16_t color;
        uint16_t backcolor;
        int seriesMaxFreq = 0; // Max frequency for this specific series
    };

class Histogram {
public:
    /**
     * @brief Construct a new Epaper Histogram object
     * @param gfx Pointer to your initialized Adafruit_GFX compatible display object.
     * @param x The x-coordinate for the top-left corner of the chart area.
     * @param y The y-coordinate for the top-left corner of the chart area.
     * @param w The width of the chart area.
     * @param h The height of the chart area.
     */
    Histogram(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);

    /**
     * @brief Set the main title of the histogram.
     * @param title The title string.
     */
    void setTitle(const char* title);
    
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
     * @brief Set the number of bins to divide the data into.
     * @param bins The number of bins (default is 10).
     */
    void setBinCount(int bins);

    /**
     * @brief Add a data series to be plotted.
     * @param name The name of the series (for the legend).
     * @param data A vector of floats for the data series.
     * @param color The color to use for this series' bars.
     */
    void addSeries(const char* name, const std::vector<float>& data, uint16_t color, uint16_t background);

    /**
     * @brief Enable or disable normalization.
     * If enabled, each series will be scaled to its own max (0-100%).
     * If disabled (default), all series share a single Y-axis scale.
     * @param enabled Set to true to enable normalization.
     */
    void setNormalization(bool enabled);

    /**
     * @brief Draw the histogram on the display.
     */
    void plot();

private:
    // A struct to hold all info for a single series
   

    void processData();
    void drawAxes();
    void drawBars();
    void drawLegend();
    void drawPatternRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2);
    void drawHatchRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2);
    void drawCheckerRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color1, uint16_t color2);
    Adafruit_GFX* _gfx;
    int16_t _x, _y, _w, _h; // Overall widget position and size

    // Plotting area (inside the widget area, with padding for labels/title)
    int16_t _plotX, _plotY, _plotW, _plotH;

    const char* _title = nullptr;
    const char* _xAxisLabel = nullptr;
    const char* _yAxisLabel = nullptr;

    int _numBins = 20;
    std::vector<HistogramSeries> _series; // Use a vector of series

    float _minVal = 0.0f;
    float _maxVal = 0.0f;
    int _maxFreq = 0; // Single max frequency (global or 100% if normalized)

    bool _normalize = false; // Normalization flag

    // Constants for layout and styling
    const int PADDING_TOP = 20;
    const int PADDING_BOTTOM = 15;
    const int PADDING_LEFT = 29;
    const int PADDING_RIGHT = 15; 
    const uint16_t AXIS_COLOR = GxEPD_BLACK;
    const uint16_t TEXT_COLOR = GxEPD_BLACK;
};

#endif // EPAPER_HISTOGRAM_H
