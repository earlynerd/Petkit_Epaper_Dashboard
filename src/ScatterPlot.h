// ScatterPlot.h

#ifndef SCATTER_PLOT_H
#define SCATTER_PLOT_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#if (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#elif (EPD_SELECT == 1001)
#include <GxEPD2_BW.h>
#endif
#include <Fonts/FreeMonoBold9pt7b.h>


// A simple struct to hold a single data point
struct DataPoint {
    float x;
    float y;
};

// A struct to hold all info for a single series
struct PlotSeries {
    String name;
    std::vector<DataPoint> data;
    uint16_t color;
    uint16_t background;
    float xMin, xMax;
    float yMin, yMax;
};

class ScatterPlot {
public:
    // Constructor
    ScatterPlot(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>* disp, int x, int y, int width, int height);

    /**
     * @brief Add a data series to be plotted.
     * @param name The name of the series (for the legend).
     * @param data A vector of DataPoints for the series.
     * @param color The color to use for this series' markers.
     */
    void addSeries(const String& name, const std::vector<DataPoint>& data, uint16_t color, uint16_t bgcolor, int xticks, int yticks);

    // Set labels for the plot
    void setLabels(const String& title, const String& xLabel, const String& yLabel);

    // The main function to draw the plot
    void draw();
    
    // Helper to draw text easily
    void drawString(int x, int y, const String& text, const GFXfont* font, uint8_t color);

private:
    // Framebuffer and plot dimensions
    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>* display;
    int _x, _y, _width, _height;
    int _xticks, _yticks;
    // Plot data and labels
    std::vector<PlotSeries> _series; // Use a vector of series
    String _title, _xLabel, _yLabel;

    // Helper functions for drawing
    void drawAxes(float xMin, float xMax, float yMin, float yMax);
    void plotDataPoints(float xMin, float xMax, float yMin, float yMax);
    void drawLegend();
    void drawDashedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, uint16_t dashLength, uint16_t spaceLength);
    void mapPoint(const DataPoint &p, int &screenX, int &screenY, float xMin, float xMax, float yMin, float yMax);
    // Generic marker drawing function
    void drawMarker(int x, int y, uint16_t color, uint16_t background);

    //add the present time to show when display last refreshed
    void add_refresh_timestamp();
};

#endif // SCATTER_PLOT_H
