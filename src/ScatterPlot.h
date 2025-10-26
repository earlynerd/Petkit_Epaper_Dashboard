// ScatterPlot.h

#ifndef SCATTER_PLOT_H
#define SCATTER_PLOT_H

#include <Arduino.h>
#include <vector>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "config.h"

// A simple struct to hold a single data point
struct DataPoint {
    float x;
    float y;
};

class ScatterPlot {
public:
    // Constructor now takes a framebuffer pointer
    ScatterPlot(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>* disp, int x, int y, int width, int height);

    // Set data for the two series
    void setDataSeries1(const std::vector<DataPoint>& data);
    void setDataSeries2(const std::vector<DataPoint>& data);

    // Set labels for the plot
    void setLabels(const String& title, const String& xLabel, const String& yLabel);
    void setLegend(const String& series1Name, const String& series2Name);

    // The main function to draw the plot
    void draw();
    // Helper to draw text easily
    void drawString(int x, int y, const String& text, const GFXfont* font, uint8_t color);

private:
    // Framebuffer and plot dimensions
    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>* display;
    int _x, _y, _width, _height;

    // Plot data and labels
    std::vector<DataPoint> _series1Data;
    std::vector<DataPoint> _series2Data;
    String _title, _xLabel, _yLabel;
    String _series1Name, _series2Name;

    // Helper functions for drawing
    void drawAxes(float xMin, float xMax, float yMin, float yMax);
    void plotDataPoints(float xMin, float xMax, float yMin, float yMax);
    void drawLegend();
    void drawDashedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, uint16_t dashLength, uint16_t spaceLength);
    
    // Marker drawing functions
    void drawMarker1(int x, int y); // Circle
    void drawMarker2(int x, int y); // Cross

    //add the present time to show when display last refreshed
    void add_refresh_timestamp();

    
};

#endif // SCATTER_PLOT_H