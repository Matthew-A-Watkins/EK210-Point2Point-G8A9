#ifndef TRANSMITRECIEVE_H
#define TRANSMITRECIEVE_H

#include <Arduino.h>
#define EOT 0x04  // End of Transmission character (ASCII EOT):contentReference[oaicite:9]{index=9}
#define SOT 0x21  // Start of Transmission indicator ('!' in ASCII)
#define ACK 0x06  // Acknowledge character (ASCII ACK):contentReference[oaicite:10]{index=10}
#define NAK 0x15  // Not-Acknowledge character (ASCII NAK):contentReference[oaicite:11]{index=11}
#define dt  10000 // Bit period in microseconds (transmission rate timing)

extern int transmitPin;  // pin used for transmitting IR (to IR LED or test wire)
extern int sensorPin;    // pin used for receiving IR (from photodiode or test wire)

// Transmit a message buffer of given length (last byte will be set to EOT).
// Sends each bit with timing dt and returns the checksum (sum of all bits mod 256).
char transmit(char* msg, int length) {
  msg[length - 1] = EOT;  // ensure the last character is EOT
  char checkSum = 0;
  for (int i = 0; i < length; i++) {
    char c = msg[i];
    for (int j = 7; j >= 0; --j) {
      bool curBit = (c >> j) & 1;       // extract j-th bit (MSB first)
      checkSum += (curBit ? 1 : 0);     // accumulate checksum (count 1 bits)
      digitalWrite(transmitPin, curBit);
      delayMicroseconds(dt);
    }
  }
  // Optionally, turn off the transmit pin after sending the message:
  digitalWrite(transmitPin, LOW);
  return checkSum;
}

// Receive a message of given length into msg buffer using bit-shift method.
// (Note: length should include space for EOT; this function does not stop on EOT automatically.)
char recieve(char* msg, int length) {
  char checkSum = 0;
  for (int i = 0; i < length; i++) {
    char c = 0x00;
    for (int j = 0; j < 8; j++) {
      bool curBit = digitalRead(sensorPin);
      checkSum += (curBit ? 1 : 0);
      c |= (curBit << j);  // assemble byte from bits (LSB-first order as transmitted)
      delayMicroseconds(dt);
    }
    msg[i] = c;
  }
  return checkSum;
}

// Transmit a single character (8 bits) via IR or wire.
void TransmitChar(char c) {
  for (int i = 7; i >= 0; i--) {
    bool bitVal = (c >> i) & 1;
    digitalWrite(transmitPin, bitVal);
    delayMicroseconds(dt);
  }
  digitalWrite(transmitPin, LOW);  // ensure line is low after sending byte
}

// Receive a single character (8 bits) from IR sensor or wire.
char recieveChar() {
  char c = 0x00;
  for (int i = 7; i >= 0; i--) {
    bool bitVal = digitalRead(sensorPin);
    c |= (bitVal << i);
    delayMicroseconds(dt);
  }
  return c;
}

// Wait for the Start-of-Transmission pattern. 
// Reads incoming bits until the 8-bit SOT byte is recognized. Returns true when SOT is detected.
bool awaitTransmission() {
  char c = 0;
  while (true) {
    bool curBit = digitalRead(sensorPin);
    c = (c << 1) | (curBit ? 1 : 0);   // shift in the new bit (keep last 8 bits in 'c')
    delayMicroseconds(dt);
    if (c == SOT) {
      return true;  // SOT sequence detected
    }
  }
  // (Function will return once SOT is found. If needed, a timeout could be added to prevent infinite loop.)
}

// Wait for either an ACK or NAK response from the other side. 
// Returns true if ACK received, false if NAK received.
bool seekACK() {
  char c = 0;
  while (true) {
    bool curBit = digitalRead(sensorPin);
    c = (c << 1) | (curBit ? 1 : 0);
    delayMicroseconds(dt);
    if (c == ACK)  return true;
    if (c == NAK)  return false;
  }
}

#endif
