#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <ctype.h>
#include "TransmitRecieve.h"

// EDIT THESE PINS AS NEEDED:

// IR mode pins
const int IR_LED_PIN    = 3;   // IR LED output
const int IR_SENSOR_PIN = 9;   // IR photodiode input
const int SERVO_PIN     = 10;  // Servo signal

// Wired test mode pins (cross-connect TX<->RX between boards)
const int TEST_TX_PIN   = 7;   // Transmitter output (wired)
const int TEST_RX_PIN   = 8;   // Transmitter input  (wired)

// (These are used by the header's bit I/O functions)
int transmitPin;
int sensorPin;

// ------------ LCD / Servo / Buffers -------------------------
LiquidCrystal_I2C lcd(0x27, 20, 4);   // change address if needed
Servo alignServo;

const int MAX_MSG_LEN = 80;
char messageBuffer[MAX_MSG_LEN + 1]; // + 1 for EOT
int  msgLength = 0;

enum Mode { IDLE=0, ALIGNMENT, EDIT_MESSAGE, TRANSMIT, TEST } mode = IDLE;
enum AlignMode { ALIGN_MENU=0, ALIGN_SWEEP, ALIGN_MANUAL } alignMode = ALIGN_MENU;

// ----------------- helpers -----------------
void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Select mode:");
  lcd.setCursor(0,1); lcd.print("[A]lign [M]sg");
  lcd.setCursor(0,2); lcd.print("[S]end [W]ire");
  Serial.println("Enter mode: A=Align, M=Edit, S=Send, W=Wired Test");
}

void showAlignMenu() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Alignment:");
  lcd.setCursor(0,1); lcd.print("[S] Sweep");
  lcd.setCursor(0,2); lcd.print("[M] Manual");
  lcd.setCursor(0,3); lcd.print("[Q] Back");
  Serial.println("Alignment: S=Auto Sweep, M=Manual, Q=Back");
}

int sampleSignal(unsigned long ms=200) {
  // sample IR sensor for 'ms' milliseconds; return number of HIGH reads
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < ms) {
    count += digitalRead(IR_SENSOR_PIN);
    delay(5);
  }
  return count;
}

void printAlignHUD_Manual(int angle, int step, int pulseCount) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Align: Manual");
  lcd.setCursor(0,1); lcd.print("Ang:"); lcd.print(angle);
  lcd.print(" Step:"); lcd.print(step);
  lcd.setCursor(0,2); lcd.print("Sig:");
  lcd.print(pulseCount>0 ? "YES" : "NO ");
  lcd.print(" Cnt:"); lcd.print(pulseCount);
  lcd.setCursor(0,3); lcd.print("J/L +/-  [/]");
  // Serial mirror
  Serial.print("[Manual] Angle="); Serial.print(angle);
  Serial.print(" Step="); Serial.print(step);
  Serial.print(" Signal="); Serial.print(pulseCount>0 ? "YES" : "NO");
  Serial.print(" Count="); Serial.println(pulseCount);
}

void printAlignHUD_Sweep(int angle, int pulseCount) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Align: Sweep");
  lcd.setCursor(0,1); lcd.print("Angle:"); lcd.print(angle);
  lcd.setCursor(0,2); lcd.print("Sig:");
  lcd.print(pulseCount>0 ? "YES" : "NO ");
  lcd.print(" Cnt:"); lcd.print(pulseCount);
  lcd.setCursor(0,3); lcd.print("[M]Manual [Q]Back");
  // Serial mirror
  Serial.print("[Sweep] Angle="); Serial.print(angle);
  Serial.print(" Signal="); Serial.print(pulseCount>0 ? "YES" : "NO");
  Serial.print(" Count="); Serial.println(pulseCount);
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Select mode:");
  lcd.setCursor(0,1); lcd.print("[A]lign [M]sg");
  lcd.setCursor(0,2); lcd.print("[S]end [W]ire");
  Serial.println("Enter mode: A=Align, M=Edit, S=Send, W=Wired Test");
}

void setup() {
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TEST_TX_PIN, OUTPUT);
  pinMode(TEST_RX_PIN, INPUT);

  alignServo.attach(SERVO_PIN);

  lcd.init();
  lcd.backlight();

  Serial.begin(9600);
  while(!Serial) { /* wait */ }

  // Default to IR pins
  transmitPin = IR_LED_PIN;
  sensorPin   = IR_SENSOR_PIN;

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Transmitter Ready");
  Serial.println("IR Transmitter ready.");
  showMenu();
}

void loop() {
  // --------------------- MODE SELECTION ----------------------
  if (mode == IDLE) {
    if (Serial.available()) {
      char sel = toupper(Serial.read());
      switch (sel) {
        case 'A':
          mode = ALIGNMENT;
          lcd.clear(); lcd.print("Mode: Alignment");
          Serial.println("** Alignment Mode **");
          break;
        case 'M':
          mode = EDIT_MESSAGE;
          msgLength = 0; messageBuffer[0] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print("(Enter to finish)");
          Serial.println("** Message Editing Mode **");
          Serial.println("Type your message. Press Enter when done.");
          break;
        case 'S':
          mode = TRANSMIT;
          transmitPin = IR_LED_PIN;
          sensorPin   = IR_SENSOR_PIN;
          lcd.clear(); lcd.print("Mode: Transmit");
          Serial.println("** Transmission Mode (IR) **");
          break;
        case 'W':
          mode = TEST;
          transmitPin = TEST_TX_PIN;
          sensorPin   = TEST_RX_PIN;
          lcd.clear(); lcd.print("Mode: Test (wired)");
          Serial.println("** Test Mode (Wired) **");
          break;
        default:
          // ignore other keys
          break;
      }
    }
    return; // wait until a mode is chosen
  }

    // --------------------- ALIGNMENT MODE ----------------------
  if (mode == ALIGNMENT) {

    // 1) Alignment sub-menu
    if (alignMode == ALIGN_MENU) {
      if (Serial.available()) {
        char s = toupper(Serial.read());
        if (s == 'S') { alignMode = ALIGN_SWEEP; }
        else if (s == 'M') { alignMode = ALIGN_MANUAL; }
        else if (s == 'Q') { mode = IDLE; showMenu(); return; }
        // refresh menu line lightly to keep LCD responsive
      }
      return;
    }

    // 2) Sweep (auto scan 0..180..repeat)
    if (alignMode == ALIGN_SWEEP) {
      for (int angle = 0; angle <= 180; angle += 5) {

        // instant switch to other modes/options
        if (Serial.available()) {
          char s = toupper(Serial.read());
          if (s=='M'){ alignMode=ALIGN_MANUAL; return; }
          if (s=='Q'){ mode=IDLE; showMenu(); return; }
          if (s=='S'){ /* already in sweep; ignore */ }
          if (s=='W'){ mode=TEST; transmitPin=TEST_TX_PIN; sensorPin=TEST_RX_PIN; lcd.clear(); lcd.print("Mode: Test (wired)"); Serial.println("-> Test Mode"); return; }
          if (s=='E' || s=='X'){ mode=EDIT_MESSAGE; lcd.clear(); lcd.print("Mode: Edit Msg"); Serial.println("-> Edit Mode"); return; }
          if (s=='T'){ mode=TRANSMIT; transmitPin=IR_LED_PIN; sensorPin=IR_SENSOR_PIN; lcd.clear(); lcd.print("Mode: Transmit"); Serial.println("-> Transmit Mode"); return; }
        }

        alignServo.write(angle);
        delay(100);
        int pulseCount = sampleSignal(200);
        printAlignHUD_Sweep(angle, pulseCount);
      }
      // After finishing a sweep, immediately loop back to continue sweeping
      return;
    }

    // 3) Manual alignment (keyboard driven)
    if (alignMode == ALIGN_MANUAL) {
      static int angle = 90;     // keep last angle while in manual
      static int step  = 5;      // default step
      // clamp & move
      if (angle < 0) angle = 0;
      if (angle > 180) angle = 180;
      alignServo.write(angle);

      // Sample at current angle and draw HUD
      int pulseCount = sampleSignal(150);
      printAlignHUD_Manual(angle, step, pulseCount);

      // Handle keyboard
      if (Serial.available()) {
        char s = Serial.read();
        char S = toupper(s);
        switch (S) {
          // coarse move
          case 'J': angle -= step; if (angle < 0) angle = 0; break;
          case 'L': angle += step; if (angle > 180) angle = 180; break;
          // fine nudge
          case '[': angle -= 1; if (angle < 0) angle = 0; break;
          case ']': angle += 1; if (angle > 180) angle = 180; break;
          // step size adjust
          case '+': if (step < 30) step++; break;
          case '-': if (step > 1)  step--; break;
          // presets
          case 'C': angle = 90; break;
          case 'Z': angle = 0;  break;
          case 'X': angle = 180; break;
          // switch / exit
          case 'S': alignMode = ALIGN_SWEEP; return;
          case 'Q': mode = IDLE; showMenu(); return;

          // convenience: allow quick jumps into other top modes
          case 'W': mode=TEST; transmitPin=TEST_TX_PIN; sensorPin=TEST_RX_PIN; lcd.clear(); lcd.print("Mode: Test (wired)"); Serial.println("-> Test Mode"); return;
          case 'E': case 'M': mode=EDIT_MESSAGE; lcd.clear(); lcd.print("Mode: Edit Msg"); Serial.println("-> Edit Mode"); return;
          case 'T': mode=TRANSMIT; transmitPin=IR_LED_PIN; sensorPin=IR_SENSOR_PIN; lcd.clear(); lcd.print("Mode: Transmit"); Serial.println("-> Transmit Mode"); return;
        }
      }
      // keep looping manual
      return;
    }
  }

  // --------------------- MESSAGE EDIT MODE -------------------
  if (mode == EDIT_MESSAGE) {
    if (Serial.available()) {
      char ch = Serial.read();

      if (ch == '\r' || ch == '\n') {
        lcd.clear();
        lcd.print("Msg saved ("); lcd.print(msgLength); lcd.print(")");
        Serial.print("\nMessage finalized ("); Serial.print(msgLength); Serial.println(" chars).");
        mode = IDLE; delay(800); showMenu(); return;
      }

      if ((ch == 8) || (ch == 127)) { // backspace
        if (msgLength > 0) {
          msgLength--;
          messageBuffer[msgLength] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print(messageBuffer);
          Serial.print("\b \b");
        }
        return;
      }

      if (isprint((unsigned char)ch)) {
        if (msgLength < MAX_MSG_LEN) {
          messageBuffer[msgLength++] = ch;
          messageBuffer[msgLength] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print(messageBuffer);
          Serial.print(ch);
        } else {
          Serial.println("\n[Message length limit reached]");
          lcd.setCursor(0,2); lcd.print("** Msg max length **");
        }
      }
    }
    return;
  }

  // --------------------- TRANSMIT / TEST ---------------------
  if (mode == TRANSMIT || mode == TEST) {
    if (msgLength == 0) {
      Serial.println("No message to send. Enter 'M' to compose a message first.");
      lcd.clear(); lcd.print("No message to send!");
      delay(1500); mode = IDLE; showMenu(); return;
    }

    // Build frame: SOT + payload + EOT
    const int totalLength = msgLength + 2;
    char outChar;
    char expectedChecksum = 0;
    bool success = false;

    for (int attempt = 1; attempt <= 10; ++attempt) {
      lcd.clear(); lcd.print("Sending (Try "); lcd.print(attempt); lcd.print(")...");
      Serial.print("Transmission attempt "); Serial.print(attempt); Serial.println("...");

      // Send SOT
      outChar = SOT;
      lcd.clear(); lcd.print("Transmitting SOT");
      Serial.println("[TX] SOT");
      for (int bit=7; bit>=0; --bit) {
        bool v = (outChar>>bit)&1; digitalWrite(transmitPin, v); delayMicroseconds(dt); expectedChecksum += v;
      }

      // Send payload
      for (int i=0; i<msgLength; ++i) {
        outChar = messageBuffer[i];
        lcd.clear(); lcd.print("Transmitting char "); lcd.print(outChar);
        Serial.print("[TX] '"); Serial.print(outChar); Serial.println("'");
        for (int bit=7; bit>=0; --bit) {
          bool v = (outChar>>bit)&1; digitalWrite(transmitPin, v); delayMicroseconds(dt); expectedChecksum += v;
        }
      }

      // Send EOT
      outChar = EOT;
      lcd.clear(); lcd.print("Transmitting EOT");
      Serial.println("[TX] EOT");
      for (int bit=7; bit>=0; --bit) {
        bool v = (outChar>>bit)&1; digitalWrite(transmitPin, v); delayMicroseconds(dt); expectedChecksum += v;
      }
      digitalWrite(transmitPin, LOW);

      // Wait for checksum reply (or NAK)
      lcd.clear(); lcd.print("Waiting for checksum");
      Serial.println("Waiting for receiver reply...");
      delay(5);
      char reply = recieveChar();

      if (reply == NAK) {
        Serial.println("[RX] NAK received – retrying.");
        lcd.clear(); lcd.print("Received NAK");
      } else if (reply == expectedChecksum) {
        Serial.println("[RX] Checksum matched. Transmission successful!");
        lcd.clear(); lcd.print("Transmission OK!");
        success = true;
        break;
      } else {
        Serial.print("[RX] Unexpected checksum (got ");
        Serial.print((int)reply);
        Serial.print(", expected ");
        Serial.print((int)expectedChecksum);
        Serial.println(") – retrying.");
        lcd.clear(); lcd.print("Checksum mismatch");
      }

      delay(400); // brief pause before retry
    }

    if (!success) {
      Serial.println("ERROR: Transmission failed after 10 attempts.");
      lcd.clear(); lcd.print("Transmission FAILED");
    }

    mode = IDLE;
    delay(1200);
    showMenu();
    return;
  }

  // Fallback: if mode somehow unknown, reset to menu
  mode = IDLE;
  showMenu();
  return;
}