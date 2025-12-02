#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>
#include <PS2Keyboard.h>
#include <Servo.h>

// -----GLOBAL DEFINITIONS------
enum Mode { IDLE = 0,
            EDIT,
            TRANSMIT,
            ALIGN } mode = IDLE;

Servo myservo;
int pos = 90;

LiquidCrystal_I2C lcd(0x27, 20, 4);

char msg[81];  // edit number to change max message length WARNING: WILL NEED TO UPDATE
int msgLength = 0;
int currentLine = 0;  // for QOL when printing
PS2Keyboard keyboard;

void setup() {
  // SERVO
  myservo.attach(9);
  myservo.write(pos);  // set to 0 degrees
  // IR
  IrSender.begin(3);  // initialize sender on default pin

  // keyboard
  keyboard.begin(8, 2);

  Serial.begin(9600);
  while (!Serial) { /* wait for Serial port */
  }

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.print("Transmitter Ready");
  Serial.println("Transmitter ready");
  showMenu();
  msgLength = 0;
}

// SHOWS DEFAULT OPTIONS ON LCD
void showMenu() {
  lcd.clear();
  lcd.print("Select mode:");
  lcd.setCursor(0, 1);
  lcd.print("[M] Compose message");
  lcd.setCursor(0, 2);
  lcd.print("[S] Send message");
  lcd.setCursor(0, 3);
  lcd.print("[A] Alignment");
  Serial.println("Enter mode: M=Edit Message, S=Send Message");
}

void sendMessage(char* message) {
  for (int i = 0; i <= msgLength; i++) {
    uint16_t charToSend = static_cast<uint16_t>(msg[i]);

    IrSender.sendNEC(0x0000, charToSend, 0);  // address, command, # of repeats
    delay(25);
  }
}

void loop() {

  // -------MENU HANDLER-------
  if (mode == IDLE) {
    if (keyboard.available()) {
      char sel = toupper(keyboard.read());
      switch (sel) {
        // Select message edit mode
        case 'M':
          mode = EDIT;
          lcd.setCursor(0, 0);
          lcd.clear();
          lcd.print("Mode: Edit Msg");
          lcd.setCursor(0, 1);
          lcd.print("(Enter to finish)");
          Serial.println("** Message Editing Mode **");
          Serial.println("Type your message. Press Enter when done.");
          msgLength = 0;
          break;

        // Select message send mode
        case 'S':
          mode = TRANSMIT;
          lcd.clear();
          lcd.print("Mode: Transmit");
          Serial.println("** Transmission Mode **");
          break;

        case 'A':
          mode = ALIGN;
          lcd.clear();
          lcd.print("Mode: Alignment");
          lcd.setCursor(0, 1);
          lcd.print("Please use left and ");
          lcd.setCursor(0, 2);
          lcd.print("right arrow keys.");
        default: break;
      }
    }
    return;
  }  // END MENU HANDLER

  if (mode == EDIT) {  // --------MESSAGE EDITOR SCREEN--------
    if (keyboard.available()) {
      char ch = keyboard.read();
      if (ch == '\r' || ch == '\n' || ch == '\0') {  // Enter key: finish message
        msg[msgLength] = '\0'; // to ensure that when msg is empty, transmit just null character
        lcd.clear();
        lcd.print("Msg saved {");
        lcd.print(msgLength);
        lcd.print("}");
        Serial.print("\nMessage finalized (");
        Serial.print(msgLength);
        Serial.println(" chars).");
        mode = IDLE;
        delay(800);
        showMenu();
        return;
      } else if (ch == 8 || ch == 127) {  // Backspace: remove last character
        if (msgLength > 0) {
          msg[--msgLength] = '\0';
          lcd.clear();
          for (int i = 0; i < msgLength; i++) {
            lcd.setCursor(i % 20, i / 20);
            lcd.print(msg[i]);
          }
          currentLine = 0;
          Serial.print("\b \b");
        }
        return;
      } else if (isprint((unsigned char)ch)) {  // Valid printable character
        if (msgLength < 81) {
          msg[msgLength++] = ch;
          msg[msgLength] = '\0';
          lcd.clear();
          for (int i = 0; i < msgLength; i++) {
            lcd.setCursor(i % 20, i / 20);
            lcd.print(msg[i]);
          }
          Serial.print(ch);
        } else {
          lcd.clear();
          Serial.println("\n[Message length limit reached]");
          lcd.setCursor(0, 1);
          lcd.print("** Msg max length **");
        }
      }
    }
  }

  if (mode == TRANSMIT) {  // ------TRANSMIT SCREEN--------
    sendMessage(msg);
    mode = IDLE;
    showMenu();
  }

  if (mode == ALIGN) {
    if (keyboard.available()) {
      char arrow = keyboard.read();

      if (arrow == PS2_ESC) {
        IrSender.sendNEC(0x0000, 0x0011, 5);  // send end of alignment to receiver
        mode = IDLE;
        showMenu();
        return;
      }

      if (arrow == PS2_LEFTARROW) {
        pos += 5;
        if (pos > 180) pos = 180;
      }

      else if (arrow == PS2_RIGHTARROW) {
        pos -= 5;
        if (pos < 0) pos = 0;
      }
      myservo.write(pos);
      IrSender.sendNEC(0x0000, 0x0006, 1);  // send ACK message
      delay(5);                             //delay to prevent turning too fast
      return;
    }
  }
}