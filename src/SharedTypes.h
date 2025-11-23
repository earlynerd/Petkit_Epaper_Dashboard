#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "PetKitApi.h" // Ensure this library is available

// Forward declaration to avoid including full heavy headers if possible
// but for these structs we usually need the definitions.

// Define the structure for our data map: PetID -> Timestamp -> Record
typedef std::map<int, std::map<time_t, LitterboxRecord>> PetDataMap;

enum DateRangeEnum {
  LAST_7_DAYS,
  LAST_30_DAYS,
  LAST_90_DAYS,
  LAST_365_DAYS,
  Date_Range_Max
};

struct DateRangeInfo {
  DateRangeEnum type;
  char name[32];
  long seconds;
};

// Global constants for NVS keys
#define NVS_NAMESPACE "petkitplotter"
#define NVS_PLOT_RANGE_KEY "plotrange"
#define NVS_PETS_KEY "pets"

#endif