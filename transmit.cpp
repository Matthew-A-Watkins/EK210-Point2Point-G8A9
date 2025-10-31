// Sender Arduino
// Digital pin to send the character on
#include <LiquidCrystal_I2C.h>
const int dataPin = 7;
// 9600 bps means 1 / 9600 = ~104 microseconds per bit
const int bitDelay = 104;
enum Mode { IDLE=0, EDIT_MESSAGE, TRANSMIT } mode = IDLE;

const int MAX_MSG_LEN = 80;
char messageBuffer[MAX_MSG_LEN + 1];
int  msgLength = 0;
LiquidCrystal_I2C lcd(0x27, 20, 4);  // 20x4 LCD display at I2C address 0x27


void setup() {
  // Configure the data pin as an output
  pinMode(dataPin, OUTPUT);
  // Keep the signal line high when idle
  digitalWrite(dataPin, HIGH);
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial port */ }

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Transmitter Ready");
  Serial.println("Transmitter ready (wired connection).");
  showMenu();
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Select mode:");
  lcd.setCursor(0,1); lcd.print("[M] Compose message");
  lcd.setCursor(0,2); lcd.print("[S] Send message");
  Serial.println("Enter mode: M=Edit Message, S=Send Message");
}


void loop() {
  if (mode == IDLE) {
    if (Serial.available()) {
      char sel = toupper(Serial.read());
      switch (sel) {
        case 'M':  // Enter message composition mode
          mode = EDIT_MESSAGE;
          msgLength = 0;
          messageBuffer[0] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print("(Enter to finish)");
          Serial.println("** Message Editing Mode **");
          Serial.println("Type your message. Press Enter when done.");
          break;
        case 'S':  // Send the current message
          mode = TRANSMIT;
          // (Using IR_LED_PIN and IR_SENSOR_PIN for wired communication)
          lcd.clear(); lcd.print("Mode: Transmit");
          Serial.println("** Transmission Mode (wired) **");
          break;
        default:
          // ignore other inputs
          break;
      }
    }
    return;  // wait until a mode is chosen
  }

   if (mode == EDIT_MESSAGE) {
    if (Serial.available()) {
      char ch = Serial.read();
      if (ch == '\r' || ch == '\n') {  // Enter key: finish message
        lcd.clear();
        lcd.print("Msg saved ("); lcd.print(msgLength); lcd.print(")");
        Serial.print("\nMessage finalized ("); Serial.print(msgLength);
        Serial.println(" chars).");
        mode = IDLE;
        delay(800);
        showMenu();
        return;
      }
      if (ch == 8 || ch == 127) {  // Backspace: remove last character
        if (msgLength > 0) {
          msgLength--;
          messageBuffer[msgLength] = '\0';
          lcd.clear(); lcd.print("Mode: Edit Msg");
          lcd.setCursor(0,1); lcd.print(messageBuffer);
          Serial.print("\b \b");
        }
        return;
      }
      if (isprint((unsigned char)ch)) {  // Valid printable character
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

  if (mode == TRANSMIT) {
    if (msgLength == 0) {
      Serial.println("No message to send. Enter 'M' to compose a message first.");
      lcd.clear(); lcd.print("No message to send!");
      delay(1500);
      mode = IDLE;
      showMenu();

      
      return;
    }

    for (int i = 0; i < 12; i++) {
      sendChar(messageBuffer[i]);
       }
      delay(1500);
      mode = IDLE;
      showMenu();
    return;
  
}
}
// Function to send a single character
void sendChar(char c) {
  // Send start bit
  digitalWrite(dataPin, LOW);
  delayMicroseconds(bitDelay);

  // Send 8 data bits (least significant bit first)
  for (int i = 0; i < 8; i++) {
    if ((c & 1) == 1) {
      digitalWrite(dataPin, HIGH);
    } else {
      digitalWrite(dataPin, LOW);
    }
    c = c >> 1; // Shift bits to the right
    delayMicroseconds(bitDelay);
  }

  // Send stop bit
  digitalWrite(dataPin, HIGH);
  delayMicroseconds(bitDelay);

  Serial.print("Sent: ");
  Serial.println(c);
  delay(1000); // Wait 1 second before sending again

}