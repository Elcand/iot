#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

Adafruit_SH1107 display = Adafruit_SH1107(128, 128, &Wire);

const int BUTTON_PIN = 4;
int jam = 12, menit = 0, detik = 0;
bool isPM = false;
unsigned long lastTick = 0;

int currentSession = 0;
unsigned long lastSessionSwitch = 0;
const unsigned long AUTO_SWITCH_INTERVAL = 6000;

unsigned long eyeStateTimer = 0;
int currentEyeMood = 0;
bool lastBtnState = HIGH;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Menggunakan alamat I2C SH1107 umum (0x3C)
  if (!display.begin(0x3C, true)) { 
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  lastSessionSwitch = millis();
  eyeStateTimer = millis();
}

void switchSession() {
  currentSession++;
  if (currentSession > 2) currentSession = 0;
  lastSessionSwitch = millis();
}

void drawOledEyes(int mood) {
  int eyeRadius = 18;
  int eyeY = 64;
  int leftEyeX = 40;
  int rightEyeX = 88;

  switch (mood) {
    case 0:
      display.drawCircle(leftEyeX, eyeY, eyeRadius, SH110X_WHITE);
      display.drawCircle(rightEyeX, eyeY, eyeRadius, SH110X_WHITE);
      display.fillCircle(leftEyeX + 3, eyeY, 6, SH110X_WHITE);
      display.fillCircle(rightEyeX + 3, eyeY, 6, SH110X_WHITE);
      break;
    case 1:
      display.fillRect(leftEyeX - 18, eyeY - 3, 36, 6, SH110X_WHITE);
      display.fillRect(rightEyeX - 18, eyeY - 3, 36, 6, SH110X_WHITE);
      break;
    case 2:
      display.drawCircle(leftEyeX, eyeY, eyeRadius, SH110X_WHITE);
      display.drawCircle(rightEyeX, eyeY, eyeRadius, SH110X_WHITE);
      display.fillRect(leftEyeX - 20, eyeY, 40, 20, SH110X_BLACK);
      display.fillRect(rightEyeX - 20, eyeY, 40, 20, SH110X_BLACK);
      break;
  }
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastTick >= 1000) {
    lastTick = currentMillis;
    detik++;
    if (detik >= 60) { detik = 0; menit++; }
    if (menit >= 60) { menit = 0; jam++; if (jam == 12) isPM = !isPM; if (jam > 12) jam = 1; }
  }

  bool currentBtnState = digitalRead(BUTTON_PIN);
  if (lastBtnState == HIGH && currentBtnState == LOW) {
    switchSession();
    delay(150);
  }
  lastBtnState = currentBtnState;

  if (currentMillis - lastSessionSwitch >= AUTO_SWITCH_INTERVAL) {
    switchSession();
  }

  display.clearDisplay();

  if (currentSession == 0) {
    display.setTextSize(1);
    display.setCursor(44, 20);
    display.print("- TIME -");

    display.setTextSize(2);
    display.setCursor(20, 50);
    if (jam < 10) display.print("0");
    display.print(jam);
    display.print(":");
    if (menit < 10) display.print("0");
    display.print(menit);

    display.setTextSize(1);
    display.setCursor(95, 55);
    display.print(isPM ? "PM" : "AM");

  } else if (currentSession == 1) {
    display.setTextSize(1);
    display.setCursor(12, 40);
    display.print("* HAPPY BIRTHDAY *");
    display.setCursor(28, 68);
    display.print("FOR YOU <3 <3");

  } else {
    if (currentMillis - eyeStateTimer > 2000) {
      eyeStateTimer = currentMillis;
      currentEyeMood = random(0, 3);
    }
    drawOledEyes(currentEyeMood);
  }

  display.display();
  delay(30);
}