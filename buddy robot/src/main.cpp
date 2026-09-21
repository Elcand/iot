#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_MPU6050 mpu;

#define BUTTON_PIN 15

enum EmotionState
{
  HAPPY,
  DIZZY,
  ANGRY,
  CLOCK_MODE
};
EmotionState currentState = HAPPY;

unsigned long lastBlinkTime = 0;
unsigned long stateTimer = 0;

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Failed to init OLED SSD1306!"));
    for (;;)
      ;
  }

  // Init MPU6050
  if (!mpu.begin())
  {
    Serial.println("Failed found sensor MPU6050!");
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(30, 28);
  display.print("Hallooo!");
  display.display();
  delay(1500);
}

void drawEyes(int eyeWidth, int eyeHeight, int xOffset = 0, int yOffset = 0)
{
  display.clearDisplay();

  // Mata Kiri
  display.fillRoundRect(32 - eyeWidth / 2 + xOffset, 32 - eyeHeight / 2 + yOffset, eyeWidth, eyeHeight, 8, SSD1306_WHITE);
  // Mata Kanan
  display.fillRoundRect(96 - eyeWidth / 2 + xOffset, 32 - eyeHeight / 2 + yOffset, eyeWidth, eyeHeight, 8, SSD1306_WHITE);

  display.display();
}

void drawDizzyEyes()
{
  display.clearDisplay();
  // Mata Kiri 'X'
  display.drawLine(20, 20, 44, 44, SSD1306_WHITE);
  display.drawLine(44, 20, 20, 44, SSD1306_WHITE);

  // Mata Kanan 'X'
  display.drawLine(84, 20, 108, 44, SSD1306_WHITE);
  display.drawLine(108, 20, 84, 44, SSD1306_WHITE);

  display.setCursor(45, 50);
  display.print("Dizzyyyy😵‍💫!");
  display.display();
}

void drawClockMode()
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(30, 20);
  display.print("10:45");
  display.setTextSize(1);
  display.setCursor(35, 45);
  display.print("WIB - Jakarta");
  display.display();
}

void loop()
{
  // 1. Cek Tombol Navigasi
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    delay(200); // Debounce
    if (currentState == CLOCK_MODE)
    {
      currentState = HAPPY;
    }
    else
    {
      currentState = CLOCK_MODE;
    }
  }

  // 2. Cek Sensor Goyangan (MPU6050)
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Hitung total percepatan (accel magnitude)
  float totalAccel = sqrt(a.acceleration.x * a.acceleration.x +
                          a.acceleration.y * a.acceleration.y +
                          a.acceleration.z * a.acceleration.z);

  // Jika digoyang kencang (Threshold > 15 m/s^2)
  if (totalAccel > 15.0 && currentState != CLOCK_MODE)
  {
    currentState = DIZZY;
    stateTimer = millis();
  }

  // Kembalikan ke mode HAPPY setelah 3 detik jika pusing
  if (currentState == DIZZY && (millis() - stateTimer > 3000))
  {
    currentState = HAPPY;
  }

  // 3. Render Tampilan Sesuai Mode
  switch (currentState)
  {
  case HAPPY:
    // Animasi kedip periodik
    if (millis() - lastBlinkTime > 3000)
    {
      drawEyes(30, 4); // Kedip (Mata sipit)
      delay(150);
      lastBlinkTime = millis();
    }
    else
    {
      drawEyes(30, 36); // Normal
    }
    break;

  case DIZZY:
    drawDizzyEyes();
    break;

  case CLOCK_MODE:
    drawClockMode();
    break;
  }

  delay(50);
}