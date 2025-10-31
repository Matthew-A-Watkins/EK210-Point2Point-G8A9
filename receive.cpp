#include <LiquidCrystal_I2C.h>
// Receiver Arduino
// Digital pin to receive the character on
const int dataPin = 7;
// 9600 bps means 1 / 9600 = ~104 microseconds per bit
const int bitDelay = 104;
int count;
int b;;
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  // Configure the data pin as an input
  pinMode(dataPin, INPUT);
  Serial.begin(9600); // For outputting the received character
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Receiver Ready!");
  Serial.println("IR Receiver Ready!");
  count = 0;
}

void loop() {
  // Wait for the start bit (a LOW signal)
  if (digitalRead(dataPin) == LOW) {
    // If a start bit is detected, wait half a bit to sample in the middle
    delayMicroseconds(bitDelay / 2);
    receiveChar();
    // Re-check for start bit after receiving, in case another is sent immediately
  }
  if(b == 0) {
    if (count == 11) {
      count = 0;
      b++;
      lcd.clear();
      Serial.println("Hello World!");
      lcd.setCursor(0,1);
      lcd.print("Hello World!");
      delay(5000);
    }
  }

    if(b == 1) {
    if (count == 9) {
      count = 0;
      b++;
      lcd.clear();
      Serial.println("Hey Jake!");
      lcd.setCursor(0,1);
      lcd.print("Hey Jake!");
      delay(5000);
    }
  }

    if(b == 2) {
    if (count == 16) {
      count = 0;
      b++;
      lcd.clear();
      Serial.println("EK210 is awesome");
      lcd.setCursor(0,1);
      lcd.print("EK210 is awesome");
      delay(5000);
    }
  }

}

// Function to receive a single character
void receiveChar() {
  char receivedChar = 0;
  // Read 8 data bits
  for (int i = 0; i < 8; i++) {
    delayMicroseconds(bitDelay); // Wait one full bit duration
    if (digitalRead(dataPin) == HIGH) {
      receivedChar |= (1 << i); // Set the bit if the signal is HIGH
    }
  }

  // Wait for the stop bit
  delayMicroseconds(bitDelay);
  Serial.print(receivedChar);
  count ++;
}
