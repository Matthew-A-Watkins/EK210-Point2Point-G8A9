#ifndef TRANSMITRECEIVE_H
#define TRANSMITRECEIVE_H

#include <Arduino.h>
#include <stdint.h>

// Protocol control characters
#define SOT 0x11   // Start of Transmission (0x11, DC1 control code)
#define EOT 0x04   // End of Transmission (0x04, EOT control code)
#define NAK 0x15   // Not-Acknowledge (0x15, NAK control code)

#define BIT_PERIOD_US 100000  // Bit period in microseconds (100 ms per bit)

extern int transmitPin;   // Pin used for transmitting bits
extern int sensorPin;     // Pin used for receiving bits

// Transmit a single byte (8 bits), MSB-first, on the transmit pin
static inline void TransmitChar(uint8_t data) {
    for (int i = 7; i >= 0; --i) {
        bool bitVal = (data >> i) & 0x01;
        digitalWrite(transmitPin, bitVal ? HIGH : LOW);
        delayMicroseconds(BIT_PERIOD_US);
    }
    // Ensure the line is driven LOW after the byte (idle state)
    digitalWrite(transmitPin, LOW);
}

// Receive a single byte (8 bits), MSB-first, from the sensor pin.
// Samples each bit at the midpoint of its period for reliability.
// Returns the byte via the reference parameter.
static inline bool receiveByte(uint8_t &outByte) {
    uint8_t value = 0;
    unsigned long bitStart = micros();
    for (int i = 7; i >= 0; --i) {
        // Wait until the middle of the current bit period
        unsigned long midBit = bitStart + BIT_PERIOD_US/2;
        while (micros() < midBit) {
            // busy-wait until midpoint
        }
        bool bitVal = digitalRead(sensorPin);
        value |= (static_cast<uint8_t>(bitVal) << i);
        bitStart += BIT_PERIOD_US;
    }
    outByte = value;
    return true;
}

// Wait for the Start-of-Transmission byte (SOT) on the sensor pin.
// Continuously reads bits into a shift register until SOT (0x11) is detected.
static inline bool awaitTransmissionStart() {
    uint8_t shiftReg = 0;
    while (true) {
        bool bitVal = digitalRead(sensorPin);
        shiftReg = static_cast<uint8_t>(((shiftReg << 1) | (bitVal ? 1 : 0)) & 0xFF);
        delayMicroseconds(BIT_PERIOD_US);
        if (shiftReg == SOT) {
            return true;
        }
    }
}

#endif
