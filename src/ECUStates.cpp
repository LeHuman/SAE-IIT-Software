#include "ECUStates.hpp"
#include "AeroServo.h"
#include "ECUGlobalConfig.h"
#include "Echo.h"
#include "Faults.h"
#include "Heartbeat.h"
#include "Log.h"
#include "Mirror.h"
#include "MotorControl.h"

static bool FaultCheck() { // NOTE: Will only return true if hardfault occurs
    if (Fault::softFault())
        Fault::logFault();
    if (Fault::hardFault())
        return true;
    return false;
}

static LOG_TAG globalID = "BACK ECU";

// static void printMCFaults(uint32_t add, volatile uint8_t *buf) {
//     if (add == ADD_MC0_FAULTS) {
//         if (buf[0])
//             Log.d(globalID, "mc 0 fault 0", buf[0]);
//         if (buf[1])
//             Log.d(globalID, "mc 0 fault 1", buf[1]);
//         if (buf[2])
//             Log.d(globalID, "mc 0 fault 2", buf[2]);
//         if (buf[3])
//             Log.d(globalID, "mc 0 fault 3", buf[3]);
//         if (buf[4])
//             Log.d(globalID, "mc 0 fault 4", buf[4]);
//         if (buf[5])
//             Log.d(globalID, "mc 0 fault 5", buf[5]);
//         if (buf[6])
//             Log.d(globalID, "mc 0 fault 6 ", buf[6]);
//         if (buf[7])
//             Log.d(globalID, "mc 0 fault 7", buf[7]);
//     }
//     if (add == ADD_MC1_FAULTS) {
//         if (buf[0])
//             Log.d(globalID, "mc 1 fault 0", buf[0]);
//         if (buf[1])
//             Log.d(globalID, "mc 1 fault 1", buf[1]);
//         if (buf[2])
//             Log.d(globalID, "mc 1 fault 2", buf[2]);
//         if (buf[3])
//             Log.d(globalID, "mc 1 fault 3", buf[3]);
//         if (buf[4])
//             Log.d(globalID, "mc 1 fault 4", buf[4]);
//         if (buf[5])
//             Log.d(globalID, "mc 1 fault 5", buf[5]);
//         if (buf[6])
//             Log.d(globalID, "mc 1 fault 6 ", buf[6]);
//         if (buf[7])
//             Log.d(globalID, "mc 1 fault 7", buf[7]);
//     }
// }

static void updateFaultLights() {
    Pins::setInternalValue(PINS_INTERNAL_BMS_FAULT, Pins::getPinValue(PINS_BACK_BMS_FAULT));
    Pins::setInternalValue(PINS_INTERNAL_IMD_FAULT, Pins::getPinValue(PINS_BACK_IMD_FAULT));
}

State::State_t *ECUStates::Initialize_State::run(void) {
    Log.i(ID, "Teensy 3.6 SAE BACK ECU Initalizing");
    Log.i(ID, "Setup canbus");
    Canbus::setup(); // allocate and organize addresses
    Log.i(ID, "Initialize pins");
    Pins::initialize(); // setup predefined pins
    Log.i(ID, "Waiting for sync");
    while (!Pins::getCanPinValue(PINS_INTERNAL_SYNC)) {
        Canbus::sendData(ADD_MC0_CTRL);
        Canbus::sendData(ADD_MC1_CTRL);
        delay(100);
    }
    Log.i(ID, "Setup faults");
    Fault::setup(); // load all buffers
    Aero::setup();
    MC::setup();
#ifdef CONF_ECU_DEBUG
    Mirror::setup();
    Echo::setup();
#endif
    Heartbeat::addCallback(updateFaultLights);
    Heartbeat::beginBeating();

    // Front teensy should know when to blink start light
    Log.d(ID, "Checking for Inital fault");

    // NOTE: IMD Fault does not matter in initalizing state
    if (!Pins::getPinValue(PINS_BACK_IMD_FAULT) && FaultCheck()) {
        Log.e(ID, "Inital fault check tripped");
        return &ECUStates::FaultState;
    }

    // Log.d(ID, "Setting MC fault callbacks");
    // Canbus::addCallback(ADD_MC0_CTRL, printMCFaults);
    // Canbus::addCallback(ADD_MC1_CTRL, printMCFaults);

    // TSV
    Log.i(ID, "Waiting for shutdown signal");
    elapsedMillis shutdownBounce;

    // while (true) {
    //     if (Pins::getPinValue(PINS_BACK_SHUTDOWN_SIGNAL)) {
    //         if (shutdownBounce > 50)
    //             break;
    //     } else {
    //         shutdownBounce = 0;
    //     }
    // }

    while (!Serial.available()) {
    }

    Log.i(ID, "Shutdown signal received");

    Log.d(ID, "Finshed Setup");
    return &ECUStates::PreCharge_State;
};

State::State_t *ECUStates::PreCharge_State::PreCharFault(void) {
    Log.w(ID, "Opening Air1, Air2 and Precharge Relay");
    Pins::setPinValue(PINS_BACK_AIR1, LOW);
    Pins::setPinValue(PINS_BACK_AIR2, LOW);
    Pins::setPinValue(PINS_BACK_PRECHARGE_RELAY, LOW);
    Log.e(ID, "Precharge Faulted during precharge");
    return &ECUStates::FaultState;
}

bool ECUStates::PreCharge_State::voltageCheck() {
    int16_t BMSVolt = BMS_DATA_Buffer.getShort(2) / 10; // Byte 2-3: Pack Instant Voltage
    int16_t MC0Volt = MC0_VOLT_Buffer.getShort(0) / 10; // Bytes 0-1: DC BUS MC Voltage
    int16_t MC1Volt = MC1_VOLT_Buffer.getShort(0) / 10; // Bytes 0-1: DC BUS MC Voltage

    return 0.9 * BMSVolt <= (MC0Volt + MC1Volt) / 2;
}

void ECUStates::PreCharge_State::getBuffers() {
    BMS_DATA_Buffer.init();
    MC1_VOLT_Buffer.init();
    MC0_VOLT_Buffer.init();
};

State::State_t *ECUStates::PreCharge_State::run(void) { // NOTE: Low = Closed, High = Open
    Log.i(ID, "Loading Buffers");
    getBuffers();
    Log.i(ID, "Precharge running");

    if (FaultCheck()) {
        Log.e(ID, "Precharge Faulted before precharge");
        return &ECUStates::FaultState;
    }

    // NOTE: Assuming Air2 is correct
    Log.w(ID, "Closing Air2 and Precharge Relay and opening Air1");
    Pins::setPinValue(PINS_BACK_AIR2, PINS_ANALOG_HIGH);
    Pins::setPinValue(PINS_BACK_PRECHARGE_RELAY, PINS_ANALOG_HIGH);
    Pins::setPinValue(PINS_BACK_AIR1, LOW);

    if (FaultCheck()) {
        return PreCharFault();
    }

    elapsedMillis timeElapsed;

    Log.d(ID, "Running precharge loop");

    // while (true) {
    //     int16_t BMSVolt = BMS_DATA_Buffer.getShort(2) / 10; // Byte 2-3: Pack Instant Voltage
    //     int16_t MC0Volt = MC0_VOLT_Buffer.getShort(0) / 10; // Bytes 0-1: DC BUS MC Voltage
    //     int16_t MC1Volt = MC1_VOLT_Buffer.getShort(0) / 10; // Bytes 0-1: DC BUS MC Voltage
    //     Log.d(ID, "BMSVolt", BMSVolt);
    //     Log.d(ID, "MC0Volt", MC0Volt);
    //     Log.d(ID, "MC1Volt", MC1Volt);
    //     delay(250);
    // }

    while (timeElapsed <= 10000) {
        if (timeElapsed >= 1000 && voltageCheck()) { // NOTE: will always pass if submodules are disconnected from CAN net
            Log.w(ID, "Opening precharge relay");
            Pins::setPinValue(PINS_BACK_PRECHARGE_RELAY, LOW);
            Log.i(ID, "Precharge Finished, closing Air1");
            Pins::setPinValue(PINS_BACK_AIR1, PINS_ANALOG_HIGH);
            return &ECUStates::Idle_State;
        }
    }

    Log.e(ID, "Precharge timed out");
    return PreCharFault();
};

State::State_t *ECUStates::Idle_State::run(void) {
    Log.i(ID, "Waiting for Button not to be pressed");
    while (!Pins::getCanPinValue(PINS_FRONT_BUTTON_INPUT_OFF)) {
    }

    Log.i(ID, "Waiting for Button or Charging Press");

    // Front teensy should already be blinking start light

    elapsedMillis waiting;

    while (true) {
        if (!Pins::getCanPinValue(PINS_FRONT_BUTTON_INPUT_OFF)) {
            Log.i(ID, "Button Pressed");
            // Front teensy will stop blinking start light
            return &ECUStates::Button_State;
        } else if (Pins::getCanPinValue(PINS_INTERNAL_CHARGE_SIGNAL)) {
            Log.i(ID, "Charging Pressed");
            // Front teensy will continue blinking start light in charge state
            return &ECUStates::Charging_State;
        } else if (FaultCheck()) {
            Log.w(ID, "Fault in idle state");
            break;
        }
    }
    return &ECUStates::FaultState;
}

State::State_t *ECUStates::Charging_State::run(void) {
    Pins::setPinValue(PINS_BACK_CHARGING_RELAY, HIGH);
    Log.i(ID, "Charging on");

    elapsedMillis voltLogNotify;

    while (Pins::getCanPinValue(PINS_INTERNAL_CHARGE_SIGNAL)) {
        // IMPROVE: Don't use fault to stop charging
        if (FaultCheck()) {
            Pins::setPinValue(PINS_BACK_CHARGING_RELAY, LOW);
            Log.e(ID, "Charging faulted, turning off");
            return &ECUStates::FaultState;
        }
    }

    Pins::setPinValue(PINS_BACK_CHARGING_RELAY, LOW);
    Log.i(ID, "Charging turning off");

    return &ECUStates::Idle_State;
}

State::State_t *ECUStates::Button_State::run(void) {
    Log.i(ID, "Waiting for Button not to be pressed");
    while (!Pins::getCanPinValue(PINS_FRONT_BUTTON_INPUT_OFF)) {
    }
    Log.i(ID, "Playing sound");

    Pins::setPinValue(PINS_BACK_SOUND_DRIVER, PINS_ANALOG_HIGH);

    elapsedMillis soundTimer;

    while (soundTimer < 2000) {
        if (FaultCheck()) {
            Log.e(ID, "Failed to play sound");
            Pins::setPinValue(PINS_BACK_SOUND_DRIVER, LOW);
            return &ECUStates::FaultState;
        }
    }

    Pins::setPinValue(PINS_BACK_SOUND_DRIVER, LOW);
    Log.i(ID, "Playing sound finished");

    return &ECUStates::Driving_Mode_State;
}

void ECUStates::Driving_Mode_State::sendMCCommand(uint32_t MC_ADD, int torque, bool direction, bool enableBit) {
    int percentTorque = constrain(map(torque, 0, PINS_ANALOG_MAX, 0, 10), 0, 10); // separate func for negative vals (regen)
    uint8_t *bytes = (uint8_t *)&percentTorque;
    Canbus::sendData(MC_ADD, bytes[0], bytes[1], 0, 0, direction, enableBit);
}

void ECUStates::Driving_Mode_State::torqueVector(int torques[2], int pedal0, int pedal1, int brakeVal, int steerVal) {
    // TODO: Add Torque vectoring algorithms
    int pedalVal = (pedal0 + pedal1) / 2;
    torques[0] = pedalVal; // TODO: must be in percentage value?
    torques[1] = pedalVal;
}

void ECUStates::Driving_Mode_State::carCooling(bool enable) { // NOTE: Cooling values are all static
    Pins::setPinValue(PINS_BACK_PUMP_DAC, enable * 2470);
    int fanSet = enable * PINS_ANALOG_MAX / 2;
    Pins::setPinValue(PINS_BACK_FAN1_PWM, fanSet);
    Pins::setPinValue(PINS_BACK_FAN2_PWM, fanSet);
    Pins::setPinValue(PINS_BACK_FAN3_PWM, fanSet);
    Pins::setPinValue(PINS_BACK_FAN4_PWM, fanSet);
}

State::State_t *ECUStates::Driving_Mode_State::DrivingModeFault(void) {
    Log.i(ID, "Fault happened in driving state");
    // carCooling(false);
    Log.i(ID, "Starting MC heartbeat");
    MC::enableMotorBeating(true);
    return &ECUStates::FaultState;
}

// NOTE: MCs weak faults also cause a true fault here
State::State_t *ECUStates::Driving_Mode_State::run(void) {
    Log.i(ID, "Driving mode on");
    Log.i(ID, "Cooling on");
    // carCooling(true); // BROKEN: uncomment

    elapsedMillis controlDelay;
    size_t counter = 0;

    // volatile uint8_t *mc0F = Canbus::getBuffer(ADD_MC0_FAULTS);
    // volatile uint8_t *mc1F = Canbus::getBuffer(ADD_MC1_FAULTS);

    Log.i(ID, "Loading Buffers");
    MC0_VOLT_Buffer.init();
    MC1_VOLT_Buffer.init();

    Log.i(ID, "Stopping MC heartbeat");
    MC::enableMotorBeating(false);

    Log.d(ID, "Sending Fault reset to MCs complete");
    MC::clearFaults(); // Clear fault if any

    Log.d(ID, "Entering drive loop");
    while (true) { // TODO: Stop if any motor controller fault happens
        if (controlDelay > 20) {
            controlDelay = 0;

            if (Fault::softFault() || Fault::hardFault()) {
                return DrivingModeFault();
            }

            if (((MC0_VOLT_Buffer.getShort(0) / 10) + (MC1_VOLT_Buffer.getShort(0) / 10)) / 2 < 90) { // 'HVD Fault'
                Log.e(ID, "'HVD Fault' MC voltage < 90");
                return DrivingModeFault();
            }

            int breakVal = Pins::getCanPinValue(PINS_FRONT_BRAKE);
            int steerVal = Pins::getCanPinValue(PINS_FRONT_STEER);

            Pins::setPinValue(PINS_BACK_BRAKE_LIGHT, PINS_ANALOG_HIGH * ((breakVal / PINS_ANALOG_MAX) > 4));

            int pedal0 = Pins::getCanPinValue(PINS_FRONT_PEDAL0);
            int pedal1 = Pins::getCanPinValue(PINS_FRONT_PEDAL1);

            // pedal0 = map(pedal0, 19, maxPed, 0, PINS_ANALOG_MAX);
            // pedal1 = map(pedal1, 19, maxPed, 0, PINS_ANALOG_MAX);

            if (abs(pedal1 - pedal0 - 8) > abs((pedal1 * 10) / 100) && (abs(pedal0) + abs(pedal1)) > 50) {
                Log.e(ID, "Pedal value offset > 10%");
                Log.i(ID, "", pedal0);
                Log.i(ID, "", pedal1);
                // return DrivingModeFault(); // FIXME: actually fault
            }

            int MotorTorques[2] = {1, 1};
            torqueVector(MotorTorques, pedal0, pedal1, breakVal, steerVal);
            sendMCCommand(ADD_MC0_CTRL, MotorTorques[0], 0, 1); // MC 1
            sendMCCommand(ADD_MC1_CTRL, MotorTorques[1], 1, 1); // MC 2F

            if (++counter > 5) {
                counter = 0;
                Log.i(ID, "Aero servo position:", Aero::getServoValue());
                if (Fault::softFault()) {
                    Fault::logFault();
                }
                // printMCFaults(ADD_MC0_FAULTS, mc0F);
                // printMCFaults(ADD_MC1_FAULTS, mc1F);
            }

            Aero::run(breakVal, steerVal);
        }

        if (!Pins::getCanPinValue(PINS_FRONT_BUTTON_INPUT_OFF)) {
            Log.w(ID, "Going back to Idle state");
            break;
        }
    }

    Log.i(ID, "Starting MC heartbeat");
    MC::enableMotorBeating(true);

    // carCooling(false);

    Log.i(ID, "Driving mode off");
    return &ECUStates::Idle_State;
}

State::State_t *ECUStates::FaultState::run(void) {
    Pins::setInternalValue(PINS_INTERNAL_GEN_FAULT, 1);
    Canbus::enableInterrupts(false);

    Log.w(ID, "Opening Air1, Air2 and Precharge Relay");
    Pins::setPinValue(PINS_BACK_AIR1, LOW);
    Pins::setPinValue(PINS_BACK_AIR2, LOW);
    Pins::setPinValue(PINS_BACK_PRECHARGE_RELAY, LOW);

    Log.w(ID, "Resetting pins");
    Pins::resetPhysicalPins();

    if (getLastState() == &ECUStates::PreCharge_State) {
        Log.f(ID, "Precharge fault");
        while (true) {
            Pins::setInternalValue(PINS_INTERNAL_GEN_FAULT, 1);
            Fault::logFault();
            delay(1000);
        }
    } else {
        Log.e(ID, "FAULT STATE");
        Fault::logFault();
        delay(10000);
    }
    Pins::setInternalValue(PINS_INTERNAL_GEN_FAULT, 0);
    return &ECUStates::Initialize_State;
}

State::State_t *ECUStates::Logger_t::run(void) {

    static elapsedMillis timeElapsed;

    if (timeElapsed >= 2000) {
        timeElapsed = timeElapsed - 2000;
        Log(ID, "A7 Pin Value:", Pins::getPinValue(0));
        Log("FAKE ID", "A7 Pin Value:");
        Log(ID, "whaAAAT?");
        Log(ID, "", 0xDEADBEEF);
        Log(ID, "Notify code: ", getNotify());
    }

    return &ECUStates::Bounce;
};

State::State_t *ECUStates::Bounce_t::run(void) {
    delay(250);
    Log.i(ID, "Bounce!");
    notify(random(100));
    delay(250);
    return getLastState();
}