/**
 * @file Heartbeat.cpp
 * @author IR
 * @brief Heartbeat source file
 * @version 0.1
 * @date 2021-03-19
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "Heartbeat.h"
#include "Canbus.h"
#include "ECUGlobalConfig.h"
#include "Log.h"

namespace Heartbeat {
static IntervalTimer canbusPinUpdate;
static elapsedMillis lastBeat;
static uint lastTime = 0;

static LOG_TAG ID = "HeartBeat";

static void toggleLED() {
    static bool on = false;
    on = !on;
    digitalWriteFast(13, on);
}

static void beat() {
    Canbus::sendData(ADD_HEART);
    toggleLED();
}

void beginBeating() {
    canbusPinUpdate.priority(255);
    canbusPinUpdate.begin(beat, CONF_HEARTBEAT_INTERVAL_MICRO);
}

static void receiveBeat(uint32_t, volatile uint8_t *) {
    lastTime = lastBeat;
    lastBeat = 0;
    toggleLED();
}

void beginReceiving() {
    Canbus::addCallback(ADD_HEART, receiveBeat);
}

void checkBeat() {
    if (lastBeat > (CONF_HEARTBEAT_INTERVAL_MICRO / 1000) + CONF_HEARTBEAT_TIMEOUT_MILLI) {
        Log.w(ID, "Heartbeat is taking too long", lastBeat);
    } else {
        Log.i(ID, "Beat", lastTime);
    }
}

} // namespace Heartbeat