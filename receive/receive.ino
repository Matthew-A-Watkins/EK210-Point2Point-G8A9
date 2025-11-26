#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>
#include <string.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

int msgLength = 0;
char recMsg[81];
bool currentlyReceiving = false;
int currentLine = 0;

void setup() {
  Serial.begin(9600);
  pinMode(3, OUTPUT);
  //IR
  IrReceiver.begin(4, ENABLE_LED_FEEDBACK, 3);  // RECEIVER PIN IS FIRST ARGUMENT, CHANGE TO ALTER PIN
  Serial.println(F("IR Receiver ready"));

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("WAITING TO RECEIVE");
}

void loop() {
  if (IrReceiver.decode()) {
    // Print protocol + address/command (don’t rely on decodedRawData)
    Serial.print(F(" Addr=0x"));  // SHOULD ALWAYS BE 0x0000
    Serial.print(IrReceiver.decodedIRData.address, HEX);

    Serial.print(F(" Cmd=0x"));  // MESSAGE CONTENT
    Serial.println(IrReceiver.decodedIRData.command, HEX);

    Serial.print(F("Protocol="));  // IF PROTOCOL IS NOT NEC IT WAS HANDLED IMPROPERLY
    Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
    Serial.println();

    if (static_cast<unsigned char>(IrReceiver.decodedIRData.command) == static_cast<unsigned char>(0x0006)) {
      lcd.clear();
      lcd.print("ALIGNMENT RECEIVED");
      Serial.println("ALIGNMENT SIGNAL RECEIVED");
      IrReceiver.resume();
      return;
    }
    else if (static_cast<unsigned char>(IrReceiver.decodedIRData.command) == static_cast<unsigned char>(0x0011)) {
      lcd.clear();
      lcd.print("WAITING TO RECEIVE");
      Serial.println("ESCAPE RECEIVED");
      delay(50);
      IrReceiver.resume();
      return;
    }

    else {
    if (!currentlyReceiving) {
      currentlyReceiving = true;
      lcd.clear();
      currentLine = 0;
    }

    char receivedChar = static_cast<unsigned char>(IrReceiver.decodedIRData.command);

    if (receivedChar == '\0') {
      currentlyReceiving = false;
      currentLine = 0;
      msgLength = 0;
      Serial.print(F("\n****message received: "));
      Serial.println(recMsg);
      IrReceiver.resume();
      return;
    }

    if (isprint(receivedChar)) {
      Serial.print(F(" CHAR: "));
      Serial.print(receivedChar);
      recMsg[msgLength++] = receivedChar;
      lcd.print(receivedChar);

      if (msgLength % 20 == 0) lcd.setCursor(0, ++currentLine);
    } else {
      Serial.print('X');
      recMsg[msgLength++] = 'X';
      lcd.print(F("X"));
      if (msgLength % 20 == 0) {
        lcd.setCursor(0, ++currentLine);
      }
    }

    // For deep debugging, uncomment to see timings:
    // IrReceiver.printIRResultRawFormatted(&Serial, true);

    IrReceiver.resume();
  }
  }
}
