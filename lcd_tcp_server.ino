#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LinkedList.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

String messages[2] = {"", ""};
int scrollPos[2] = {0, 0};
int scrollOffsetX[2] = {0, 0};
unsigned long lastScroll[2] = {0, 0};
const unsigned long scrollDelay = 300;

LinkedList<String> messageQueue[2] = { LinkedList<String>(), LinkedList<String>() };
LinkedList<int> queueX[2] = { LinkedList<int>(), LinkedList<int>() };

int lastUsedRow = 1;

bool rowBusy[2] = {false, false};
unsigned long startDisplayTime[2] = {0, 0};
const unsigned long displayHoldTime = 1800000;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  Serial.println("Ready...");
}

void loop() {
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

      String fullLine = "";
      for (int i = 0; i < xOffset; i++) fullLine += " ";
      fullLine += fragment;
      while (fullLine.length() < 16) fullLine += " ";

      lcd.setCursor(0, y);
      lcd.print(fullLine);

      scrollPos[y]++;
      if (scrollPos[y] >= totalScrollLength) {
        if (millis() - startDisplayTime[y] >= displayHoldTime) {
          scrollPos[y] = 0;
          messages[y] = "";
          rowBusy[y] = false;
          lcd.setCursor(0, y);
          lcd.print("                ");
          Serial.print("[INFO] Row ");
          Serial.print(y);
          Serial.println(" cleared after scroll timeout.");

          if (messageQueue[y].size() > 0) {
            String next = messageQueue[y].remove(0);
            int x = queueX[y].remove(0);
            messages[y] = next;
            scrollOffsetX[y] = x;
            scrollPos[y] = 0;
            lastScroll[y] = millis();
            startDisplayTime[y] = millis();
            rowBusy[y] = true;
            Serial.print("[INFO] Displaying next queued message on row ");
            Serial.print(y);
            Serial.print(": \"");
            Serial.print(next);
            Serial.println("\"");
          }
        } else {
          scrollPos[y] = 0;
        }
      }

      lastScroll[y] = millis();
    }
  }

  for (int y = 0; y < 2; y++) {
    if (rowBusy[y] && messages[y] == "") {
      if (messageQueue[y].size() == 0 && millis() - startDisplayTime[y] >= displayHoldTime) {
        rowBusy[y] = false;
        lcd.setCursor(0, y);
        lcd.print("                ");
        Serial.print("[INFO] Row ");
        Serial.print(y);
        Serial.println(" cleared after fixed message timeout.");
      }
    }
  }

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
      if (rowBusy[y]) {
        int alt = 1 - y;
        if (!rowBusy[alt]) {
          Serial.print("[INFO] Row ");
          Serial.print(y);
          Serial.print(" busy, using row ");
          Serial.print(alt);
          Serial.println(" instead.");
          y = alt;
        } else {
          // Ambas filas ocupadas: usar la de menor cola o alternar
          int q0 = messageQueue[0].size();
          int q1 = messageQueue[1].size();
          int target = (q0 < q1) ? 0 : (q1 < q0) ? 1 : (1 - lastUsedRow);

          messageQueue[target].add(message);
          queueX[target].add(x);
          Serial.print("[INFO] Both rows busy. Queued message on row ");
          Serial.print(target);
          Serial.print(": \"");
          Serial.print(message);
          Serial.println("\"");

          int freeRam = freeMemory();
          if (freeRam < 500) {
            Serial.println("[WARN] Low memory! Clearing all queues.");
            for (int i = 0; i < 2; i++) {
              messageQueue[i].clear();
              queueX[i].clear();
            }
          }

          return;
        }
      }

      int availableSpace = 16 - x;

      if (message.length() <= availableSpace) {
        lcd.setCursor(x, y);
        lcd.print("                ");
        lcd.setCursor(x, y);
        lcd.print(message);
        messages[y] = "";
        rowBusy[y] = true;
        startDisplayTime[y] = millis();
        Serial.print("[INFO] Displaying short message on row ");
        Serial.print(y);
        Serial.print(" at x=");
        Serial.print(x);
        Serial.print(": \"");
        Serial.print(message);
        Serial.println("\"");
      } else {
        messages[y] = message;
        scrollPos[y] = 0;
        scrollOffsetX[y] = x;
        lastScroll[y] = millis();
        startDisplayTime[y] = millis();
        rowBusy[y] = true;
        Serial.print("[INFO] Scheduled scrolling message on row ");
        Serial.print(y);
        Serial.print(" at x=");
        Serial.print(x);
        Serial.print(": \"");
        Serial.print(message);

        Serial.println("\"");
      }

      lastUsedRow = y;
    }
  }
}

// ✅ Utilidad para chequear memoria libre
extern unsigned int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
