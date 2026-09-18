#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int BUTTON_PIN = 4;

// Custom Character (5x8)
byte eyeOpen[8] = { B00000, B01110, B10101, B11111, B10001, B01110, B00000, B00000 };
byte eyeBlink[8] = { B00000, B00000, B00000, B11111, B00000, B00000, B00000, B00000 };
byte eyeHeart[8] = { B00000, B01010, B11111, B11111, B01110, B00100, B00000, B00000 };

int jam = 12, menit = 0, detik = 0;
unsigned long lastTick = 0, lastBlink = 0, surpriseTimer = 0;
bool isBlinking = false, surpriseMode = false;
bool isBooting = true;
unsigned long bootTimer = 0;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, eyeOpen);
  lcd.createChar(1, eyeBlink);
  lcd.createChar(2, eyeHeart);

  lcd.setCursor(0, 0);
  lcd.print(" HBD MY FAVORITE ");
  lcd.setCursor(0, 1);
  lcd.print("   PERSON! <3   ");

  bootTimer = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  if (isBooting) {
    if (currentMillis - bootTimer >= 2500) {
      isBooting = false;
      lcd.clear();
    }
    delay(10);
    return;
  }

  if (currentMillis - lastTick >= 1000) {
    lastTick = currentMillis;
    detik++;
    if (detik >= 60) { detik = 0; menit++; }
    if (menit >= 60) { menit = 0; jam++; }
    if (jam >= 24) { jam = 0; }
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!surpriseMode) {
      surpriseMode = true;
      lcd.clear();
    }
    surpriseTimer = currentMillis;
  }

  if (surpriseMode && (currentMillis - surpriseTimer > 4000)) {
    surpriseMode = false;
    lcd.clear();
  }

  if (surpriseMode) {
    lcd.setCursor(0, 0);
    lcd.print(" HAPPY BIRTHDAY!");
    lcd.setCursor(6, 1);
    lcd.write(2); lcd.print(" "); lcd.write(2);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("TIME  ");
    if (jam < 10) lcd.print("0"); lcd.print(jam); lcd.print(":");
    if (menit < 10) lcd.print("0"); lcd.print(menit); lcd.print(":");
    if (detik < 10) lcd.print("0"); lcd.print(detik);

    if (currentMillis - lastBlink > 3000) {
      isBlinking = true;
      if (currentMillis - lastBlink > 3200) {
        lastBlink = currentMillis;
        isBlinking = false;
      }
    }

    lcd.setCursor(6, 1);
    if (isBlinking) {
      lcd.write(1); lcd.print(" "); lcd.write(1);
    } else {
      lcd.write(0); lcd.print(" "); lcd.write(0);
    }
  }

  delay(10);
}