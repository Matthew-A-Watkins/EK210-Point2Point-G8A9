#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TransmitRecieve.h"

// ** Receiver Pin Assignments **
const int IR_SENSOR_PIN = 9;   // IR photodiode input pin 
const int IR_LED_PIN    = 3;   // IR LED output pin (for ACK and alignment pulses)
const int TEST_TX_PIN   = 8;   // alternate output pin for wired test ACK 
const int TEST_RX_PIN   = 7;   // alternate input pin for wired test message

// Global pin variables for TransmitRecieve.h functions
int transmitPin;
int sensorPin;

LiquidCrystal_I2C lcd(0x27, 20, 4);

const int MAX_MSG_LEN = 80;         // Maximum allowed message characters (excl. SOT/EOT)
char recvBuffer[MAX_MSG_LEN + 1];   // Buffer for received message content (+null terminator)
int recvLength = 0;
bool testMode = false;              // Flag for test (wired) mode

void setup() {
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(TEST_RX_PIN, INPUT);
  pinMode(TEST_TX_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  while (!Serial) { /* wait for serial port */ }

  // Default to IR mode
  testMode = false;
  sensorPin   = IR_SENSOR_PIN;
  transmitPin = IR_LED_PIN;

  lcd.clear();
  lcd.print("Receiver Ready (IR)");
  Serial.println("IR Receiver ready. (Send 'W' for wired mode, 'I' for IR mode)");
}

void loop() {
  // Emit alignment pulses periodically (only in IR mode when idle)
  static unsigned long lastPulseTime = 0;
  if (!testMode) {
    unsigned long now = micros();
    if (now - lastPulseTime >= 500000) {  // every 500 ms
      digitalWrite(IR_LED_PIN, HIGH);
      delay(5);                            // emit a short 5ms pulse
      digitalWrite(IR_LED_PIN, LOW);
      lastPulseTime = now;
    }
  }

  // Check for user command to toggle modes
  if (Serial.available()) {
    char cmd = toupper(Serial.read());
    if (cmd == 'W') {
      // Switch to Wired Test mode
      testMode = true;
      sensorPin   = TEST_RX_PIN;
      transmitPin = TEST_TX_PIN;
      lcd.clear();
      lcd.print("Mode: Wired TEST");
      Serial.println("** Receiver in TEST mode (wired) **");
    } else if (cmd == 'I') {
      // Switch back to IR mode
      testMode = false;
      sensorPin   = IR_SENSOR_PIN;
      transmitPin = IR_LED_PIN;
      lcd.clear();
      lcd.print("Mode: IR");
      Serial.println("** Receiver in IR mode **");
    }
  }

  // Attempt to detect start-of-transmission (SOT) from transmitter
  if (!testMode) {
    // In IR mode, we continuously wait for an IR signal
    // Use awaitTransmission to block until SOT is seen (function returns true when SOT received)
    if (awaitTransmission()) {
      // SOT detected, proceed to receive the rest of the message
      handleReception();
    }
  } else {
    // In test mode, similarly wait for incoming bits on the wired line
    if (awaitTransmission()) {
      handleReception();
    }
  }
}

// Helper function to handle the reception of a message after SOT is detected
void handleReception() {
  lcd.clear();
  lcd.print("Receiving...");
  Serial.println("Incoming transmission detected. Receiving data...");
  // Turn off alignment pulses during reception (to avoid interference)
  // (Not strictly necessary here since pulses are already timed not to overlap much)
  unsigned long receiveStart = millis();

  bool errorFlag = false;
  char receivedChecksum = 0;  // will accumulate count of '1' bits
  recvLength = 0;

  // We assume SOT was already received (and not stored in recvBuffer).
  // Include SOT's bits in checksum:
  char SOTchar = SOT;
  for (int b = 0; b < 8; ++b) {
    if (SOTchar & (1 << b)) receivedChecksum++;
  }

  // Now receive bytes until EOT or error
  while (true) {
    char c = recieveChar();  // read next 8-bit character from the incoming stream
    // Add this char's bits to checksum (every '1' bit)
    for (int b = 0; b < 8; ++b) {
      if (c & (1 << b)) receivedChecksum++;
    }

    if (c == EOT) {
      // End-of-Transmission reached
      break;
    }
    // If message is too long, set error (but still continue to flush to EOT)
    if (recvLength >= MAX_MSG_LEN) {
      errorFlag = true;
      Serial.println("Error: Received message exceeds 80 characters (too long).");
      // We will ignore further content but still consume bits until EOT
      // Continue reading but not storing until EOT is found (with a safety limit to avoid infinite loop)
    }

    if (!errorFlag) {
      // Store character in buffer if no error and space available
      recvBuffer[recvLength++] = c;
    }

    // Safety check: If we've been receiving for a very long time without finding EOT, break
    if (millis() - receiveStart > 5000) {  // e.g., 5 seconds timeout
      errorFlag = true;
      Serial.println("Error: Receiving timed out (no EOT).");
      break;
    }
    // Loop continues until break on EOT or timeout
  }

  // Null-terminate the received content (for easier printing)
  if (recvLength < MAX_MSG_LEN) {
    recvBuffer[recvLength] = '\0';
  } else {
    recvBuffer[MAX_MSG_LEN] = '\0';
  }

  // If no EOT was encountered and loop ended by timeout, treat as missing EOT
  // (errorFlag would already be true in that case)

  // Send appropriate acknowledgment
  if (errorFlag) {
    // Transmission was invalid – send NAK
    TransmitChar(NAK);
    Serial.println("=> Message rejected. NAK sent to transmitter.");
    lcd.clear();
    lcd.print("Msg Error: NAK");
    delay(2000);
  } else {
    // Valid message received – send checksum as ACK
    char ackValue = receivedChecksum;
    TransmitChar(ackValue);
    Serial.print("=> Message received OK. Sent checksum 0x");
    Serial.print((int)ackValue, HEX);
    Serial.println(" as ACK.");

    // Display the received message on LCD and Serial
    lcd.clear();
    lcd.print("Message received:");
    // Print message content on subsequent LCD lines
    int idx = 0;
    for (int line = 1; line < 4 && idx < recvLength; ++line) {
      lcd.setCursor(0, line);
      for (int col = 0; col < 20 && idx < recvLength; ++col) {
        lcd.print(recvBuffer[idx++]);
      }
    }
    Serial.println("[Received Message]:");
    Serial.println(recvBuffer);
  }

  // After processing, go back to waiting for next transmission (loop restarts)
  // (Alignment pulses will resume automatically if in IR mode)
}
