#include "AeroServo.h"
#include "Log.h"
#include "PWMServo.h" // Servo.h is reliant on quick interrupts, switch (and test) if PWMServo is not good enough

namespace Aero {

static const LOG_TAG ID = "AeroServo";

// TODO: check that these values are okay
static const int steerLMin = PINS_VOLT_TO_ANALOG(1.5);
static const int steerLMax = PINS_VOLT_TO_ANALOG(0);
static const int steerRMin = PINS_VOLT_TO_ANALOG(3.5);
static const int steerRMax = PINS_VOLT_TO_ANALOG(5);

static const int angleMin = 30; // 30 is min possible servo angle, 180 is max
static const int angleMax = 72; // angle of attack is 42

static PWMServo servo1;
static PWMServo servo2;

static int servoVal;

void setup() {
    Log.i(ID, "Initializing Aero servo pins");
    servo1.attach(PINS_BACK_SERVO1_PWM);
    servo2.attach(PINS_BACK_SERVO2_PWM);
    Log.i(ID, "Done");
}

int getServoValue(){
    return servoVal;
}

void run(int breakPressure, int steeringAngle) { // Add 10 millisec delay
    // Brake pressure
    int breakPos = map(breakPressure, PINS_ANALOG_MIN, PINS_ANALOG_MAX, angleMin, angleMax);

    // Steering angle sensor
    int steerPos = 0;

    if (steeringAngle <= steerLMin) {
        steerPos = map(steeringAngle, steerLMax, steerLMin, angleMax, angleMin);
    } else if (steeringAngle >= steerRMin) {
        steerPos = map(steeringAngle, steerRMin, steerRMax, angleMin, angleMax);
    }

    static int lastValue = angleMin;
    servoVal = (max(breakPos, steerPos) + lastValue * 3) / 4;
    lastValue = servoVal;

    servo1.write(servoVal);
    servo2.write(servoVal);
}

} // namespace Aero