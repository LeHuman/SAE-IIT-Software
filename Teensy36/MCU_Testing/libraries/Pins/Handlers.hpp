#ifndef __MCU_PINHANDLERS_H__
#define __MCU_PINHANDLERS_H__

#include "PinConfig.def"

#define ANALOGOUTPUT PinHandlers::writeAnalog
#define ANALOGINPUT PinHandlers::readAnalog
#define DIGITALOUTPUT PinHandlers::writeDigital
#define DIGITALINPUT PinHandlers::readDigital

namespace PinHandlers {

extern void null(const int pin, int &value);
extern void readDigital(const int pin, int &value);
extern void readAnalog(const int pin, int &value);
extern void writeDigital(const int pin, int &value);
extern void writeAnalog(const int pin, int &value);

} // namespace PinHandlers

#endif // __MCU_PINHANDLERS_H__