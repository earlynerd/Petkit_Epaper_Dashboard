#include "PlotManager.h"

PlotManager::PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp) 
    : _display(disp) {}

void PlotManager::renderDashboard(const std::vector<Pet> &pets, PetDataMap &allPetData, const DateRangeInfo &range) {
    _display->fillScreen(GxEPD_WHITE);
    
    // Prepare vectors
    size_t numPets = pets.size();
    std::vector<DataPoint> pet_scatterplot[numPets];
    std::vector<float> interval_hist[numPets];
    std::vector<float> duration_hist[numPets];
    
    time_t now = time(NULL);
    time_t timeStart = now - range.seconds;

    int idx = 0;
    for (const auto &pet : pets) {
        if (allPetData.find(pet.id) == allPetData.end()) { idx++; continue; }
        
        time_t lastTimestamp = -1;
        for (auto const &recordPair : allPetData[pet.id]) {
            const LitterboxRecord &record = recordPair.second;
            if (record.timestamp < timeStart) continue;

            float weight_lbs = (float)record.weight_grams / GRAMS_PER_POUND;
            
            pet_scatterplot[idx].push_back({(float)record.timestamp, weight_lbs});
            duration_hist[idx].push_back((float)record.duration_seconds / 60.0);

            if (lastTimestamp > 0)
                interval_hist[idx].push_back(((float)(record.timestamp - lastTimestamp)) / 3600.0);
            
            lastTimestamp = record.timestamp;
        }
        idx++;
    }

    // --- Draw Histograms ---
    Histogram histInterval(_display, 0, _display->height() * 3 / 4, _display->width() / 2, _display->height() / 4);
    histInterval.setTitle("Interval (Hours)");
    histInterval.setBinCount(16);
    histInterval.setNormalization(true);
    
    Histogram histDuration(_display, _display->width() / 2, _display->height() * 3 / 4, _display->width() / 2, _display->height() / 4);
    histDuration.setTitle("Duration (Minutes)");
    histDuration.setBinCount(16);
    histDuration.setNormalization(true);

    for (int i = 0; i < numPets; ++i) {
        histInterval.addSeries(pets[i].name.c_str(), interval_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        histDuration.addSeries(pets[i].name.c_str(), duration_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
    }
    
    histInterval.plot();
    histDuration.plot();

    // --- Draw ScatterPlot ---
    ScatterPlot plot(_display, 0, 0, EPD_WIDTH, EPD_HEIGHT * 3/ 4);
    char title[64];
    sprintf(title, "Weight (lb) - %s", range.name);
    plot.setLabels(title, "Date", "Weight(lb)");
    
    int xticks = (range.type == LAST_7_DAYS) ? 10 : 18; // Simplified logic
    
    for (int i = 0; i < numPets; ++i) {
        plot.addSeries(pets[i].name.c_str(), pet_scatterplot[i], _petColors[i % 4].color, _petColors[i % 4].background, xticks, 10);
    }
    plot.draw();
}