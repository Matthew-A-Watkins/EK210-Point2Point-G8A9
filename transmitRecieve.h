#pragma once

#include <Arduino.h>
#define EOT 0x04 // end transmission character
#define SOT 0x21 // Start of transmission character
#define ACK 0x06 // aknowlege character
#define NAK 0x15 // not aknowlege
#define dt 10000 // period in us
#define sensorPin 9


// transmits a message and returns the sum of the bits as a char 
// NOTE THE LAST CHARACTER OF MSG WILL BE SET TO EOT
char transmit(char* msg, int length){
  msg[length - 1] = EOT; // set the end of transmission value
  char checkSum = 0;
  for (int i = 0; i < length; i++){
    char c = msg[i];
    for (int j = 7; j >= 0; --j){
      bool curBit = c >> j & 1; // read the jth bit
      checkSum += curBit; // increment checksum
      digitalWrite(transmitPin, curBit);
      delayMicroseconds(dt);
    }
  }
  return (checksum);
}

// uses the bitshift method to recieve the bits dirrectly into the message, returns checksum(sum of all bits)
char recieve(char* msg, int length + 1){
  char checkSum = 0;
  for (int i = 0; i < length; i++){
    char c = 0x00;
    bool curBit = 0;
    // go bit by bit
    for (int j = 0; j < 8; j++){
      // read the sensor
      curBit = digitalRead(sensorPin);
      checkSum += curBit;
      c += curBit << j; // add the current bit shifted by its place
      delayMicroseconds(dt);
    }
    msg[i] = c;
  }
  return checkSum;
}

// transmits a single character
void TransmitChar(char c){
  for (int i = 7; i >= 0; i--){
    digitalWrite(transmitPin, c >> i & 1);
    delayMicroseconds(dt);
  }
}

// recieves a single char
char recieveChar(){
  char c = 0x00;
  for(int i = 7; i >= 0; i--){
    c += digitalRead(sensorPin) << i; // add the value * 2^i
    delayMicroseconds(dt);
  }
  return(c);
}

// gets the start of transmission, returns 1 when it has recieved the go code
bool awaitTransmission() {
  char c = 0;
  while(decodeBits(buffer) != SOT){
    bool curBit = digitalRead(sensorPin);
    c = c << 1 + curBit;
    delayMicroseconds(dt);
  }
  return 1;
}

// looks for an aknowledge or a not aknowledge response and returns such
bool seekAKG(){
  char c = 0;
  while(c != AKG && c != NAK){
    bool curBit = digitalRead(sensorPin);
    c = c << 1 + curBit;
    delayMicroseconds(dt);
  }
  if (c == AKG) return true;
  return false;
}

// old buffer based system (It's kinda cheeks tbh)

// recieves the message into a binary array and returns the checksum
char recieveOld(bool* signal, int length) {
  char checkSum = 0x00;
  bool curBit = 0;
  // read the bits and add them to the message
  for(int i = 0; i < length; i++){
    curBit = digitalRead(sensorPin);
    checkSum += curBit;
    delayMicroseconds(dt);
    signal[i] = curBit;
  }
}

// transmits the binary signal and returns the checksum
char TransmitOld(bool* signal, int length){
  char checkSum = 0;
  bool curBit = 0;
  // send the start signal
  digitalWrite(transmitPin, 1);
  delay(dt);
  for (int i = 0; i < length * 8; i++){
    curBit = signal[i];
    checkSum += curBit;
    digitalWrite(transmitPin, curBit);
    delayMicroseconds(dt);
  }
}
