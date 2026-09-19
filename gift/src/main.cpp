// Rancangan: Animasi Mata Interaktif & Sistem Display ESP32
// S1 Mata (8s) - S2 Jam (5s) - S3 Teks (4s)
// Wiring MD: OLED 3V3/GND/SDA21/SCL22 | TTP223 SIG->GPIO4 | Button GPIO15->GND
// Lib: U8g2 (grafik) + NTPClient (jam) | Semua non-blocking millis()

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <U8g2lib.h>

// ---------- KONFIG HARDWARE ----------
const int PIN_TOUCH = 4;   // TTP223 SIG (HIGH = disentuh)
const int PIN_BUTTON = 15; // Tombol manual ke GND (INPUT_PULLUP)

// ---------- DISPLAY U8g2 SH1107 128x128 ----------
U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- WIFI + NTP (WIB = UTC+7) ----------
const char* WIFI_SSID = "Wokwi-GUEST"; // di hardware asli ganti SSID rumah
const char* WIFI_PASS = "";
WiFiUDP ntpUDP;
NTPClient jamNTP(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);
bool ntpSiap = false;
unsigned long jadwalNTP = 0;

// Fallback jam bila WiFi/NTP belum connect (tetap jalan)
int fbHari = 0; // 0=Sunday
int fbJam = 9, fbMenit = 30, fbDetik = 0;
bool fbPM = false;
unsigned long tickFallback = 0;
const char* NAMA_HARI[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

// ---------- STATE MACHINE SESI ----------
enum Sesi { S1_MATA = 0, S2_JAM = 1, S3_TEKS = 2 };
int sesi = S1_MATA;
unsigned long sesiMulai = 0;
const unsigned long DURASI_SESI[3] = {8000, 5000, 4000}; // MD: 8s, 5s, 4s

// ---------- STATE MATA ----------
enum Mata { MATA_IDLE, MATA_HAPPY, MATA_WIDE, MATA_SLEEPY };
Mata stateMata = MATA_IDLE;
unsigned long mataSampai = 0;

float lirikX = 0, lirikY = 0, targetLirikX = 0, targetLirikY = 0;
unsigned long jadwalLirik = 0;
float bukaMata = 1.0;
unsigned long jadwalKedip = 0;
unsigned long mulaiKedip = 0;
bool sedangKedip = false;

// ---------- DETEKSI SENTUH TTP223 ----------
bool touchLevel = LOW, touchTerakhir = LOW;
unsigned long touchNaik = 0;
unsigned long rilisTerakhir = 0;
bool tungguDouble = false;

// ---------- TOMBOL MANUAL ----------
bool btnTerakhir = HIGH;
unsigned long btnDebounce = 0;

// ---------- S3 TEKS ----------
String teksJalan = "Happy Birthday!   kamu hebat!   ";
int posScroll = 128;

// helper: tulis tengah
void tulisTengah(const char* s, int y, const uint8_t* font) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(s);
  u8g2.drawStr((128 - w) / 2, y, s);
}

void pindahSesiBerikut() {
  sesi = (sesi + 1) % 3;
  sesiMulai = millis();
}

void sentuhOverride() {
  // MD: sentuh saat S2/S3 -> instan lompat ke S1
  if (sesi != S1_MATA) {
    sesi = S1_MATA;
  }
  sesiMulai = millis();
}

void picuMata(Mata m, unsigned long dur) {
  stateMata = m;
  mataSampai = millis() + dur;
  sedangKedip = false;
  bukaMata = 1.0;
}

// ===== S1: gambar satu mata kotak (seperti foto) =====
void gambarMata(int cx, int cy, int w, int h, bool kiri) {
  unsigned long now = millis();
  int ox = (int)(lirikX * 5);
  int oy = (int)(lirikY * 4);
  int shakeX = 0, shakeY = 0, bounce = 0;

  if (stateMata == MATA_HAPPY) {
    bounce = -abs((int)(sin(now / 150.0) * 6));
  }
  if (stateMata == MATA_WIDE) {
    shakeX = random(-3, 4);
    shakeY = random(-3, 4);
    w += 8; h += 8;
  }

  cx += ox + shakeX;
  cy += oy + shakeY + bounce;

  u8g2.setDrawColor(1);
  if (stateMata == MATA_HAPPY) {
    // kurva senyum ^ ^ : kotak penuh lalu hapus bawah
    u8g2.drawRBox(cx - w / 2, cy - h / 2, w, h, 6);
    u8g2.setDrawColor(0);
    u8g2.drawBox(cx - w / 2 - 2, cy + 2, w + 4, h);
    u8g2.setDrawColor(1);
    u8g2.drawBox(cx - w / 2, cy - h / 2, w, 7); // pertegas lengkung atas
    return;
  }
  if (stateMata == MATA_SLEEPY) {
    // kelopak setengah menutup: garis tidur
    u8g2.drawBox(cx - w / 2, cy - 2, w, 5);
    return;
  }

  int hh = (int)(h * bukaMata);
  if (hh < 4) hh = 4;
  u8g2.drawRBox(cx - w / 2, cy - hh / 2, w, hh, 6);

  // pupil hitam biar bisa melirik (hanya saat terbuka)
  if (bukaMata > 0.6 && stateMata != MATA_WIDE) {
    u8g2.setDrawColor(0);
    int px = cx + (int)(lirikX * 5);
    int py = cy + (int)(lirikY * 4);
    u8g2.drawDisc(px, py, 4);
    u8g2.setDrawColor(1);
  }
}

void animasiMata() {
  unsigned long now = millis();
  if (now > mataSampai && stateMata != MATA_IDLE) stateMata = MATA_IDLE;

  // lirik acak
  if (now > jadwalLirik) {
    jadwalLirik = now + random(1200, 3000);
    targetLirikX = random(-10, 11) / 10.0;
    targetLirikY = random(-7, 8) / 10.0;
    if (random(0, 3) == 0) { targetLirikX = 0; targetLirikY = 0; }
  }
  lirikX += (targetLirikX - lirikX) * 0.12;
  lirikY += (targetLirikY - lirikY) * 0.12;

  // kedip: idle 2-6 dtk (MD), sleepy lebih lambat
  unsigned long jedaKedipMin = (stateMata == MATA_SLEEPY) ? 1500 : 2000;
  unsigned long jedaKedipMax = (stateMata == MATA_SLEEPY) ? 3000 : 6000;
  if (!sedangKedip && stateMata != MATA_HAPPY && stateMata != MATA_WIDE && now > jadwalKedip) {
    sedangKedip = true;
    mulaiKedip = now;
  }
  if (sedangKedip) {
    unsigned long t = now - mulaiKedip;
    if (t < 90) bukaMata = 1.0 - (t / 90.0);
    else if (t < 180) bukaMata = (t - 90) / 90.0;
    else { sedangKedip = false; bukaMata = 1.0; jadwalKedip = now + random(jedaKedipMin, jedaKedipMax); }
  }

  float nafas = sin(now / 650.0);
  int w = 28 + (int)nafas;
  int h = 34 + (int)nafas;

  gambarMata(40, 64, w, h, true);
  gambarMata(88, 64, w, h, false);

  // mulut kecil saat happy / kaget
  if (stateMata == MATA_HAPPY) u8g2.drawBox(58, 100, 12, 3);
  if (stateMata == MATA_WIDE) u8g2.drawDisc(64, 102, 4);
}

// ===== S2: jam "- Sunday -" + HH:MM AM/PM =====
void tampilJam() {
  int hh, mm;
  const char* hari;
  const char* ampm;

  if (ntpSiap) {
    int h24 = jamNTP.getHours();
    mm = jamNTP.getMinutes();
    hari = NAMA_HARI[jamNTP.getDay()];
    ampm = (h24 >= 12) ? "PM" : "AM";
    hh = h24 % 12;
    if (hh == 0) hh = 12;
  } else {
    hh = fbJam; mm = fbMenit;
    hari = NAMA_HARI[fbHari];
    ampm = fbPM ? "PM" : "AM";
  }

  char atas[24], tengah[8];
  snprintf(atas, sizeof(atas), "- %s -", hari);
  snprintf(tengah, sizeof(tengah), "%02d:%02d", hh, mm);

  tulisTengah(atas, 14, u8g2_font_6x10_tf);
  tulisTengah(tengah, 62, u8g2_font_logisoso16_tf);
  tulisTengah(ampm, 80, u8g2_font_6x10_tf);
  if (!ntpSiap) tulisTengah("~no NTP~", 100, u8g2_font_5x7_tf);
}

// ===== S3: scrolling text + 2 emoji =====
void gambarHatiU8(int cx, int cy, int s) {
  u8g2.drawDisc(cx - s / 2, cy - s / 3, s / 2);
  u8g2.drawDisc(cx + s / 2, cy - s / 3, s / 2);
  u8g2.drawTriangle(cx - s, cy, cx + s, cy, cx, cy + s);
}
void gambarSmileU8(int cx, int cy, int r) {
  u8g2.drawCircle(cx, cy, r);
  u8g2.drawDisc(cx - r / 3, cy - r / 4, 2);
  u8g2.drawDisc(cx + r / 3, cy - r / 4, 2);
  for (int a = 30; a <= 150; a++) {
    float rad = a * 3.14159 / 180.0;
    u8g2.drawPixel(cx + cos(rad) * (r / 2), cy + sin(rad) * (r / 2));
  }
}

void tampilTeks() {
  u8g2.setFont(u8g2_font_6x10_tf);
  int tw = u8g2.getStrWidth(teksJalan.c_str());
  posScroll--;
  if (posScroll < -tw) posScroll = 128;
  u8g2.drawStr(posScroll, 24, teksJalan.c_str());
  u8g2.drawStr(10, 48, "for you <3");

  gambarHatiU8(44, 88, 14);
  gambarSmileU8(86, 88, 14);
}

// ===== polling sentuh: tap / double / long (MD tabel) =====
void bacaSentuh() {
  unsigned long now = millis();
  touchLevel = digitalRead(PIN_TOUCH); // HIGH = sentuh (TTP223)

  // tekan mulai
  if (touchTerakhir == LOW && touchLevel == HIGH) {
    touchNaik = now;
    sentuhOverride(); // MD touch-override: batalkan timer, lompat ke S1
  }
  // dilepas -> ukur durasi
  if (touchTerakhir == HIGH && touchLevel == LOW) {
    unsigned long dur = now - touchNaik;
    if (dur >= 800) {
      picuMata(MATA_SLEEPY, 3000);           // long press
    } else if (tungguDouble && (now - rilisTerakhir < 500)) {
      picuMata(MATA_WIDE, 1000);             // double tap
      tungguDouble = false;
    } else {
      picuMata(MATA_HAPPY, 1500);            // tap singkat
      tungguDouble = true;
    }
    rilisTerakhir = now;
  }
  if (tungguDouble && (now - rilisTerakhir > 500)) tungguDouble = false;
  touchTerakhir = touchLevel;
}

void bacaTombol() {
  bool b = digitalRead(PIN_BUTTON);
  if (btnTerakhir == HIGH && b == LOW && millis() - btnDebounce > 200) {
    btnDebounce = millis();
    pindahSesiBerikut(); // manual trigger
  }
  btnTerakhir = b;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT_PULLDOWN); // TTP223 idle LOW, sentuh HIGH
  pinMode(PIN_BUTTON, INPUT_PULLUP);  // tombol ke GND

  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setContrast(255);
  randomSeed(analogRead(34));

  // WiFi + NTP: coba 6 detik, selebihnya fallback millis
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    jamNTP.begin();
    jamNTP.update();
    ntpSiap = true;
    jadwalNTP = millis();
  }

  sesiMulai = millis();
  jadwalKedip = millis() + 2000;
  jadwalLirik = millis() + 1000;
}

void loop() {
  unsigned long now = millis();

  // update NTP berkala + fallback clock tiap 1 dtk
  if (ntpSiap && now - jadwalNTP > 60000) {
    jadwalNTP = now;
    jamNTP.update();
  }
  if (now - tickFallback >= 1000) {
    tickFallback = now;
    fbDetik++;
    if (fbDetik >= 60) {
      fbDetik = 0; fbMenit++;
      if (fbMenit >= 60) {
        fbMenit = 0; fbJam++;
        if (fbJam == 12) fbPM = !fbPM;
        if (fbJam > 12) { fbJam = 1; if (!fbPM) fbHari = (fbHari + 1) % 7; }
      }
    }
  }

  bacaSentuh();
  bacaTombol();

  // MD auto-cycle: S1 8s -> S2 5s -> S3 4s
  if (now - sesiMulai >= DURASI_SESI[sesi]) pindahSesiBerikut();

  u8g2.clearBuffer();
  if (sesi == S1_MATA) animasiMata();
  else if (sesi == S2_JAM) tampilJam();
  else tampilTeks();
  u8g2.sendBuffer();
}
