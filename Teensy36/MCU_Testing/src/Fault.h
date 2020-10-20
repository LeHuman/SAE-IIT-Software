#include "config.def"
#include <stdint.h>

static const uint64_t BMS_HARDFAULTMASK = CONF_BITMASK_BMS_HARDFAULT;
static const uint64_t RMS_HARDFAULTMASK = CONF_BITMASK_RMS_HARDFAULT;

static int BMS_HardFaultCheck(const uint8_t raw_data[8]) {
    return ((*(uint64_t *)raw_data) & BMS_HARDFAULTMASK) == 0;
}

static int RMS_HardFaultCheck(const uint8_t raw_data[8]) {
    return ((*(uint64_t *)raw_data) & RMS_HARDFAULTMASK) == 0;
}