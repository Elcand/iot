#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD I2C pada alamat 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int BUTTON_PIN = 4;

// --- Desain Karakter Piksel (Custom Character 5x8) ---
byte eyeOpen[8] = {
  B00000, B01110, B10101, B11111, B10001, B01110, B00000, B00000
};

byte eyeBlink[8] = {
  B00000, B00000, B00000, B11111, B00000, B00000, B00000, B00000
};

byte eyeHeart[8] = {
  B00000, B01010, B11111, B11111, B01110, B00100, B00000, B00000
};

// Variabel Waktu
int jam = 12;
int menit = 0;
int detik = 0;

unsigned long lastTick = 0;
unsigned long lastBlink = 0;
bool isBlinking = false;

// Mode Surprise / Ultah
bool surpriseMode = false;
unsigned long surpriseTimer = 0;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  // Daftarkan karakter kustom ke memori LCD
  lcd.createChar(0, eyeOpen);
  lcd.createChar(1, eyeBlink);
  lcd.createChar(2, eyeHeart);

  // Pesan Pembuka / Booting
  lcd.setCursor(0, 0);
  lcd.print(" HBD MY FAVORITE ");
  lcd.setCursor(0, 1);
  lcd.print("   PERSON! <3   ");
  delay(2500);
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Pewaktu Sederhana (Jam Digital)
  if (currentMillis - lastTick >= 1000) {
    lastTick = currentMillis;
    detik++;
    if (detik >= 60) { detik = 0; menit++; }
    if (menit >= 60) { menit = 0; jam++; }
    if (jam >= 24) { jam = 0; }
  }

  // 2. Cek Pencetan Tombol Interaksi
  if (digitalRead(BUTTON_PIN) == LOW) {
    surpriseMode = true;
    surpriseTimer = currentMillis;
    lcd.clear();
  }

  // Durasi mode kejutan: 4 detik
  if (surpriseMode && (currentMillis - surpriseTimer > 4000)) {
    surpriseMode = false;
    lcd.clear();
  }

  // 3. Tampilan Layar
  if (surpriseMode) {
    // Mode Kejutan saat tombol dipencet
    lcd.setCursor(0, 0);
    lcd.print(" HAPPY BIRTHDAY!");
    
    lcd.setCursor(6, 1);
    lcd.write(2); // Mata Heart
    lcd.print(" ");
    lcd.write(2);
  } else {
    // Mode Normal: Jam & Animasi Mata Kedip
    lcd.setCursor(0, 0);
    lcd.print("TIME  ");
    if (jam < 10) lcd.print("0");
    lcd.print(jam);
    lcd.print(":");
    if (menit < 10) lcd.print("0");
    lcd.print(menit);
    lcd.print(":");
    if (detik < 10) lcd.print("0");
    lcd.print(detik);

    // Animasi kedip tiap 3 detik
    if (currentMillis - lastBlink > 3000) {
      isBlinking = true;
      if (currentMillis - lastBlink > 3200) {
        lastBlink = currentMillis;
        isBlinking = false;
      }
    }

    lcd.setCursor(6, 1);
    if (isBlinking) {
      lcd.write(1); lcd.print(" "); lcd.write(1); // Mata Merem
    } else {
      lcd.write(0); lcd.print(" "); lcd.write(0); // Mata Terbuka
    }
  }
}