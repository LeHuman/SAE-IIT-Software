#ifndef __CAN_HANDLERS_H__
#define __CAN_HANDLERS_H__

#include "FlexCAN_T4.h"
#include "WProgram.h"
#include <stdint.h>
#include <stdlib.h>

void canSniff(const CAN_message_t &msg) {
    Serial.print("MB ");
    Serial.print(msg.mb);
    Serial.print("  OVERRUN: ");
    Serial.print(msg.flags.overrun);
    Serial.print("  LEN: ");
    Serial.print(msg.len);
    Serial.print(" EXT: ");
    Serial.print(msg.flags.extended);
    Serial.print(" TS: ");
    Serial.print(msg.timestamp);
    Serial.print(" ID: ");
    Serial.print(msg.id, HEX);
    Serial.print(" Buffer: ");
    for (uint8_t i = 0; i < msg.len; i++) {
        Serial.print(msg.buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

template <int T>
void readTest(const CAN_message_t &msg) {
    Serial.print("MB: ");
    Serial.println(T);
}

#endif // __CAN_HANDLERS_H__