#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16x2 LCD

// Per-line scroll state
String messages[2] = {"", ""};
int scrollPos[2] = {0, 0};
int scrollOffsetX[2] = {0, 0};
unsigned long lastScroll[2] = {0, 0};
const unsigned long scrollDelay = 350;

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for Serial to become ready
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("
  ");
}

void loop() {
  // Scroll logic for each row
  for (int y = 0; y < 2; y++) {
    if (messages[y].length() > 0 && millis() - lastScroll[y] > scrollDelay) {
      int xOffset = scrollOffsetX[y];
      int visibleChars = 16 - xOffset;

      String fullMsg = messages[y];
      int totalScrollLength = fullMsg.length() + visibleChars;
      int scrollIndex = scrollPos[y] - visibleChars;
      String fragment = "";

      for (int i = 0; i < visibleChars; i++) {
        int charIndex = scrollIndex + i;
        if (charIndex < 0) {
          fragment += " ";
        } else if (charIndex < fullMsg.length()) {
          fragment += fullMsg[charIndex];
        } else {
          fragment += " ";
        }
      }

      lcd.setCursor(0, y);
      lcd.print("                ");  // Clear row
      lcd.setCursor(xOffset, y);
      lcd.print(fragment);

      scrollPos[y]++;
      if (scrollPos[y] >= totalScrollLength) {
        scrollPos[y] = 0;
      }

      lastScroll[y] = millis();
    }
  }

  // Handle incoming serial command
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    String xStr = "", yStr = "", message = "";
    int start = 0;

    while (start < input.length()) {
      int equalPos = input.indexOf('=', start);
      int ampPos = input.indexOf('&', start);
      if (equalPos == -1) break;

      String key = input.substring(start, equalPos);
      String value;
      if (ampPos == -1) {
        value = input.substring(equalPos + 1);
        start = input.length();
      } else {
        value = input.substring(equalPos + 1, ampPos);
        start = ampPos + 1;
      }

      if (key == "x") xStr = value;
      else if (key == "y") yStr = value;
      else if (key == "message") message = value;
    }

    int x = xStr.toInt();
    int y = yStr.toInt();

    if (x >= 0 && x < 16 && y >= 0 && y < 2) {
      int availableSpace = 16 - x;

      if (message.length() <= availableSpace) {
        lcd.setCursor(x, y);
        lcd.print("                ");  // Clear area
        lcd.setCursor(x, y);
        lcd.print(message);
        messages[y] = "";  // No scroll needed
      } else {
        messages[y] = message;
        scrollPos[y] = 0;
        scrollOffsetX[y] = x;
        lastScroll[y] = millis();
      }
    } else {
      Serial.println("Invalid coordinates");
    }
  }
}
