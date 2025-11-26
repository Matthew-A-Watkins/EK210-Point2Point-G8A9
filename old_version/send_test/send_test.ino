#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include 
// -----GLOBAL DEFINITIONS------
enum Mode { IDLE = 0,
            EDIT,
            TRANSMIT } mode = IDLE;

LiquidCrystal_I2C lcd(0x27, 20, 4);

char msg[81]; // edit number to change max message length WARNING: WILL NEED TO UPDATE
int msgLength;


void setup() {
  IrSender.begin(3);  // initialize sender on default pin

  lcd.init();
  lcd.backlight();
  Serial.begin(9600);

  while (!Serial) { /* wait for Serial port */
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Transmitter Ready");
  Serial.println("Transmitter ready");
  showMenu();
  msgLength = 0;
}

// SHOWS DEFAULT OPTIONS ON LCD
void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select mode:");
  lcd.setCursor(0, 1);
  lcd.print("[M] Compose message");
  lcd.setCursor(0, 2);
  lcd.print("[S] Send message");
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
    if (Serial.available()) {
      char sel = toupper(Serial.read());


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
          lcd.setCursor(0, 0);
          lcd.print("Mode: Transmit");
          Serial.println("** Transmission Mode **");
          break;

          default: break;
      }
    }
    return;
  }  // END MENU HANDLER

  if (mode == EDIT) {  // --------MESSAGE EDITOR SCREEN--------
    if (Serial.available()) {
      char ch = Serial.read();
      if (ch == '\r' || ch == '\n' || ch == '\0') {  // Enter key: finish message
        lcd.clear();
        lcd.print("Msg saved ({}");
        lcd.print(msgLength);
        lcd.print(")");
        Serial.print("\nMessage finalized (");
        Serial.print(msgLength);
        Serial.println(" chars).");
        mode = IDLE;
        delay(800);
        showMenu();
        return;
      }
      else if (ch == 8 || ch == 127) {  // Backspace: remove last character
        if (msgLength > 0) {
          msgLength--;
          msg[msgLength] = '\0';
          lcd.clear(); lcd.setCursor(0,0); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0, 1);
          lcd.print(msg);
          Serial.print("\b \b");
        }
        return;
      }
      else if (isprint((unsigned char) ch)) {  // Valid printable character
        if (msgLength < 81) {
          msg[msgLength++] = ch;
          msg[msgLength] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print(msg);
          Serial.print(ch);
        } else {
          Serial.println("\n[Message length limit reached]");
          lcd.setCursor(0,2); lcd.print("** Msg max length **");
        }
      }
    }
  }

  if(mode == TRANSMIT) { // ------TRANSMIT SCREEN--------
    sendMessage(msg);
    mode = IDLE; 

    showMenu();
  }
}