#ifndef PLOT_MANAGER_H
#define PLOT_MANAGER_H

#include "SharedTypes.h"
#include "config.h"
#include "ScatterPlot.h"
#include "histogram.h"

class PlotManager {
public:
    PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display);
    
    void renderDashboard(const std::vector<Pet> &pets, 
                         PetDataMap &allPetData, 
                         const DateRangeInfo &range);

private:
    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *_display;
    
    // Constants for colors, layout, etc.
    struct ColorPair {
        uint16_t color;
        uint16_t background;
    };
    const std::vector<ColorPair> _petColors = {
        {EPD_RED, EPD_YELLOW}, {EPD_BLUE, EPD_BLUE}, 
        {EPD_GREEN, EPD_YELLOW}, {EPD_BLACK, EPD_WHITE}
    };
};

#endif