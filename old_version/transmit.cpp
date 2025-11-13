#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include "TransmitRecieve.h"

// ** Transmitter Pin Assignments ** 
const int IR_LED_PIN   = 3;   // IR LED output pin for IR transmission 
const int IR_SENSOR_PIN= 9;   // IR photodiode input pin for IR reception (ack/alignment)
const int SERVO_PIN    = 10;  // Servo control pin (pan/tilt IR LED)
const int TEST_TX_PIN  = 7;   // alternate output pin for wired test mode 
const int TEST_RX_PIN  = 8;   // alternate input pin for wired test mode 

// Global variables for pins (used by TransmitRecieve.h functions)
int transmitPin;
int sensorPin;

// Create LCD and Servo objects
LiquidCrystal_I2C lcd(0x27, 20, 4);   // 20x4 LCD at I2C address 0x27 (adjust if needed)
Servo alignServo;

// Message buffer and state
const int MAX_MSG_LEN = 80;          // Maximum message length (characters)
char messageBuffer[MAX_MSG_LEN + 1]; // Buffer for message content (null-terminated)
int msgLength = 0;                   // Current length of message (characters)

// Mode identifiers for clarity
enum Mode { IDLE=0, ALIGNMENT, EDIT_MESSAGE, TRANSMIT, TEST } mode = IDLE;

// Retry attempt counter for transmission
int attemptCount = 0;

void setup() {
  // Initialize hardware pins
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TEST_TX_PIN, OUTPUT);
  pinMode(TEST_RX_PIN, INPUT);
  alignServo.attach(SERVO_PIN);  // attach servo on SERVO_PIN

  // Initialize LCD and Serial
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial to be ready */ }

  // Set initial transmit/receive pins to IR (default mode)
  transmitPin = IR_LED_PIN;
  sensorPin   = IR_SENSOR_PIN;

  // Welcome message and mode selection prompt
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Select mode: ");
  lcd.setCursor(0,1);
  lcd.print("[A]lign [M]sg");
  lcd.setCursor(0,2);
  lcd.print("[S]end [W]ire");
  Serial.println("IR Transmitter ready.");
  Serial.println("Enter mode: A=Align, M=Edit Message, S=Send Message, W=Wired Test");
}

void loop() {
  // If in idle mode, wait for user to choose a mode via serial
  if (mode == IDLE) {
    if (Serial.available()) {
      char ch = Serial.read();
      ch = toupper(ch);
      switch(ch) {
        case 'A':  // Enter Alignment mode
          mode = ALIGNMENT;
          lcd.clear();
          lcd.print("Mode: Alignment");
          Serial.println("** Alignment Mode **");
          Serial.println("Servo scanning for receiver signal...");
          break;
        case 'M':  // Enter Message Editing mode
          mode = EDIT_MESSAGE;
          msgLength = 0;
          messageBuffer[0] = '\0';  // reset message buffer
          lcd.clear();
          lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1);
          lcd.print("(Enter to finish)");
          Serial.println("** Message Editing Mode **");
          Serial.println("Type your message. Press Enter when done.");
          break;
        case 'S':  // Enter IR Transmission mode
          mode = TRANSMIT;
          attemptCount = 0;
          // Ensure IR pins are in use
          transmitPin = IR_LED_PIN;
          sensorPin   = IR_SENSOR_PIN;
          lcd.clear();
          lcd.print("Mode: Transmit");
          Serial.println("** Transmission Mode (IR) **");
          break;
        case 'W':  // Enter Wired Test mode
          mode = TEST;
          attemptCount = 0;
          // Switch to test pins for direct wiring
          transmitPin = TEST_TX_PIN;
          sensorPin   = TEST_RX_PIN;
          lcd.clear();
          lcd.print("Mode: Test (wired)");
          Serial.println("** Test Mode (Wired) **");
          Serial.println("Transmitter using direct wire connections.");
          break;
        default:
          // Unrecognized input (ignore)
          break;
      }
    }
  }

  // *** Servo Alignment Mode *** 
  else if (mode == ALIGNMENT) {
    bool signalDetected = false;
    int detectedAngle = -1;
    // Sweep servo from 0 to 180 degrees to find the receiver's signal
    for (int angle = 0; angle <= 180; angle += 5) {
      alignServo.write(angle);
      delay(100);  // small delay to allow servo to move
      // Read IR sensor for a short duration to detect pulses
      int pulseCount = 0;
      unsigned long startTime = millis();
      while (millis() - startTime < 200) {       // 200ms sample at this angle
        pulseCount += digitalRead(IR_SENSOR_PIN);
        delay(5);
      }
      // Update LCD and Serial with current angle status
      lcd.clear();
      lcd.print("Angle:");
      lcd.print(angle);
      lcd.print(" Sig:");
      lcd.print(pulseCount > 0 ? "YES" : "NO");
      Serial.print("Angle ");
      Serial.print(angle);
      Serial.print(" -> ");
      Serial.println(pulseCount > 0 ? "Signal DETECTED" : "No signal");
      if (pulseCount > 0) {
        signalDetected = true;
        detectedAngle = angle;
        // (Optional) we could break here on first detection, 
        // but continuing sweep might find a stronger signal or confirm alignment.
      }
      // Check if user wants to exit alignment mode early
      if (Serial.available()) {
        char c = Serial.read();
        c = toupper(c);
        if (c == 'Q') {  // 'Q' to quit alignment
          Serial.println("Alignment mode aborted by user.");
          break;
        } else if (c == 'M' || c == 'S' || c == 'W' || c == 'A') {
          // If user entered another mode letter, break to handle mode switch
          ch = c;  // reuse variable to trigger mode change outside (not ideal, but handle after loop)
          break;
        }
        // (Any other input is ignored in alignment mode)
      }
    }
    // After sweep or break, handle results or mode switch
    if (Serial.available()) {
      // If user entered a mode command during alignment, switch to that mode
      char c = toupper(Serial.read());
      if (c == 'M' || c == 'S' || c == 'W' || c == 'A') {
        mode = IDLE;
        Serial.write(c);  // put the command back for processing in idle (optionally)
        continue;
      }
    }
    if (mode != ALIGNMENT) {
      // Alignment loop was interrupted for a mode change
      lcd.clear();
      lcd.print("Alignment stopped");
      delay(1000);
      lcd.clear();
      continue;
    }
    // If alignment completed normally, report result
    lcd.clear();
    if (signalDetected) {
      lcd.print("Aligned! Angle=");
      lcd.print(detectedAngle);
      Serial.print("Receiver signal found at ~");
      Serial.print(detectedAngle);
      Serial.println(" degrees.");
    } else {
      lcd.print("No signal found");
      Serial.println("No receiver signal detected in sweep.");
    }
    // Remain in Alignment mode until user changes mode (sweeps can repeat)
    delay(1000);
  }

  // *** Message Editing Mode *** 
  else if (mode == EDIT_MESSAGE) {
    if (Serial.available()) {
      char ch = Serial.read();
      if (ch == '\r' || ch == '\n') {
        // Enter pressed -> finish editing
        lcd.clear();
        lcd.print("Msg saved (");
        lcd.print(msgLength);
        lcd.print(")");
        Serial.print("\nMessage finalized (");
        Serial.print(msgLength);
        Serial.println(" characters).");
        // Return to idle (waiting for next command, likely 'S' to send)
        mode = IDLE;
        delay(1000);
        lcd.clear();
        lcd.print("Select mode: ");
        lcd.setCursor(0,1);
        lcd.print("[A] [M] [S] [W]");
      } 
      else if ((ch == 8) || (ch == 127)) {
        // Backspace pressed
        if (msgLength > 0) {
          msgLength--;
          messageBuffer[msgLength] = '\0';
          // Update LCD (clear current line and print updated text)
          lcd.clear();
          lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1);
          lcd.print(messageBuffer);
          // Move cursor after text (if we want to show editing cursor, optional)
          Serial.print("\b \b");  // remove last char in serial monitor
        }
      } 
      else if (isprint(ch)) {
        // Normal printable character input
        if (msgLength < MAX_MSG_LEN) {
          messageBuffer[msgLength] = ch;
          msgLength++;
          messageBuffer[msgLength] = '\0';  // maintain null-terminated string
          // Display typed character on LCD and Serial
          lcd.clear();
          lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1);
          // If message exceeds LCD width, it will wrap to next line automatically
          lcd.print(messageBuffer);
          Serial.print(ch);
        } else {
          // Buffer full (80 chars reached)
          Serial.println("\n[Message length limit reached]");
          lcd.setCursor(0,2);
          lcd.print("** Msg max length **");
        }
      }
    }
  }

  // *** Transmission Mode (IR or Test) *** 
  else if (mode == TRANSMIT || mode == TEST) {
    // Only enter if a message has been composed (msgLength > 0 or user explicitly chose send)
    if (msgLength == 0) {
      Serial.println("No message to send. Enter 'M' to compose a message first.");
      lcd.clear();
      lcd.print("No message to send!");
      delay(2000);
      // Go back to idle if nothing to send
      mode = IDLE;
      lcd.clear();
      lcd.print("Select mode: ");
      continue;
    }

    // Compose the full message frame with SOT and EOT
    int totalLength = msgLength + 2;  // including SOT and EOT
    char txBuffer[MAX_MSG_LEN + 2];
    txBuffer[0] = SOT;
    for (int i = 0; i < msgLength; ++i) {
      txBuffer[i+1] = messageBuffer[i];
    }
    // (EOT will be placed at txBuffer[totalLength-1] by transmit function or manually)

    bool success = false;
    char expectedChecksum = 0;
    for (attemptCount = 1; attemptCount <= 10; ++attemptCount) {
      // Log attempt number
      lcd.clear();
      lcd.print("Sending (Try ");
      lcd.print(attemptCount);
      lcd.print(")...");
      Serial.print("Transmission attempt ");
      Serial.print(attemptCount);
      Serial.println("...");

      // Transmit each character bit-by-bit
      expectedChecksum = 0;
      for (int i = 0; i < totalLength; ++i) {
        char outChar = (i < totalLength - 1) ? txBuffer[i] : EOT;  // last char is EOT
        // Show status on LCD/Serial for content characters
        if (outChar == SOT) {
          lcd.clear();
          lcd.print("Transmitting SOT");
          Serial.println("[TX] Start-of-Transmission (SOT) sent");
        } else if (outChar == EOT) {
          lcd.clear();
          lcd.print("Transmitting EOT");
          Serial.println("[TX] End-of-Transmission (EOT) sent");
        } else {
          lcd.clear();
          lcd.print("Transmitting char ");
          lcd.print(outChar);
          Serial.print("[TX] Transmitting char '");
          Serial.print(outChar);
          Serial.println("'");
        }
        // Send the byte bit-by-bit
        for (int bit = 7; bit >= 0; --bit) {
          bool bitVal = (outChar >> bit) & 1;
          digitalWrite(transmitPin, bitVal);
          delayMicroseconds(dt);            // bit timing (dt defined in TransmitRecieve.h)
          expectedChecksum += (bitVal ? 1 : 0);  // accumulate checksum (count of '1' bits)
        }
      }
      // Ensure IR LED is off after transmission
      digitalWrite(transmitPin, LOW);

      // Now wait for acknowledgment from receiver
      lcd.clear();
      lcd.print("Waiting for checksum");
      Serial.println("Waiting for response from receiver...");
      delay(5);  // small gap before reading reply

      // Read one byte from the receiver (either checksum or NAK)
      char reply = recieveChar();  // read 8-bit reply from sensorPin
      if (reply == NAK) {
        // Receiver explicitly signaled a problem
        Serial.println("[RX] NAK received – will retry.");
        lcd.clear();
        lcd.print("Received NAK");
      } else if (reply == expectedChecksum) {
        // Checksums match – success!
        Serial.println("[RX] Checksum matched. Transmission successful!");
        lcd.clear();
        lcd.print("Transmission OK!");
        success = true;
      } else {
        // Some other byte received (could be wrong checksum or random noise)
        Serial.print("[RX] Unexpected checksum (got ");
        Serial.print((int)reply);
        Serial.print(", expected ");
        Serial.print((int)expectedChecksum);
        Serial.println("). Will retry.");
        lcd.clear();
        lcd.print("Checksum mismatch");
      }

      if (success) break;  // exit retry loop on success
      // If not successful, prepare for next attempt (if any)
      delay(500);  // short delay before re-transmitting (allow receiver to reset if needed)
    }  // end of retry loop

    if (!success) {
      Serial.println("ERROR: Transmission failed after 10 attempts.");
      lcd.clear();
      lcd.print("Transmission FAILED");
    }

    // After transmission attempts, go back to idle for new commands
    mode = IDLE;
    delay(2000);
    lcd.clear();
    lcd.print("Select mode: ");
    lcd.setCursor(0,1);
    lcd.print("[A] [M] [S] [W]");
  }
}
