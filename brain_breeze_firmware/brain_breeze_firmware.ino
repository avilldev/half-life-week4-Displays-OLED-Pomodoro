#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>

const int POT_PIN   = A0; // Slide Potentiometer
const int START_BTN = D1; // Start/Pause Button
const int MODE_BTN  = D2; // Mode/Reset Button
const int BUZZ_PIN  = D3; // Buzzer

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_AHTX0 aht;

enum State { IDLE_SELECT, RUNNING, PAUSED };
State currentState = IDLE_SELECT;

bool isBreakMode = false;
int targetMinutes = 25;
long remainingSeconds = 25 * 60;
int completedSessions = 0;
bool sensorConnected = false;

unsigned long lastTick = 0;
unsigned long idleTimer = 0;
int lastStartBtn = HIGH;
int lastModeBtn  = HIGH;
int lastPotMin   = -1;
bool needsRedraw = true;

int shiftX = 0;
int shiftY = 0;
bool isInverted = false;

void beep(int freq, int duration) {
  tone(BUZZ_PIN, freq, duration);
}

void setup() {
  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);

  // Init I2C and OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  // Init AHT20
  sensorConnected = aht.begin();

  display.clearDisplay();
  display.display();
  idleTimer = millis();
}

void loop() {
  unsigned long now = millis();

  // Read S/P Button
  int startBtn = digitalRead(START_BTN);
  if (startBtn == LOW && lastStartBtn == HIGH) {
    beep(1200, 60);
    if (currentState == IDLE_SELECT) {
      remainingSeconds = targetMinutes * 60L;
      currentState = RUNNING;
      lastTick = now;
    } else if (currentState == RUNNING) {
      currentState = PAUSED;
    } else if (currentState == PAUSED) {
      currentState = RUNNING;
      lastTick = now;
    }
    idleTimer = now;
    needsRedraw = true;
  }
  lastStartBtn = startBtn;

  // Read Mode Button
  int modeBtn = digitalRead(MODE_BTN);
  if (modeBtn == LOW && lastModeBtn == HIGH) {
    beep(900, 60);
    if (currentState != IDLE_SELECT) {
      currentState = IDLE_SELECT;
    } else {
      isBreakMode = !isBreakMode;
      targetMinutes = isBreakMode ? 5 : 25;
      remainingSeconds = targetMinutes * 60L;
    }
    idleTimer = now;
    needsRedraw = true;
  }
  lastModeBtn = modeBtn;

  // Read Slider
  if (currentState == IDLE_SELECT) {
    int rawPot = analogRead(POT_PIN);
    int maxMin = isBreakMode ? 15 : 60;
    int minMin = isBreakMode ? 3  : 10;

    int mappedMin = map(rawPot, 0, 1023, minMin, maxMin);
    mappedMin = ((mappedMin + 2) / 5) * 5; 
    if (mappedMin < minMin) mappedMin = minMin;
    if (mappedMin > maxMin) mappedMin = maxMin;

    if (mappedMin != lastPotMin) {
      lastPotMin = mappedMin;
      targetMinutes = mappedMin;
      remainingSeconds = targetMinutes * 60L;
      idleTimer = now;
      needsRedraw = true;
    }
  }

  // Timer Countdown
  if (currentState == RUNNING && (now - lastTick >= 1000)) {
    lastTick += 1000;
    remainingSeconds--;
    needsRedraw = true;

    if (remainingSeconds % 30 == 0) {
      shiftX = random(-2, 3);
      shiftY = random(-2, 3);
    }

    // Timer Complete
    if (remainingSeconds <= 0) {
      if (!isBreakMode) completedSessions++;
      isBreakMode = !isBreakMode;
      targetMinutes = isBreakMode ? 5 : 25;
      remainingSeconds = targetMinutes * 60L;
      currentState = IDLE_SELECT;
      
      beep(987, 100); delay(120);
      beep(1318, 250);
      idleTimer = now;
    }
  }

  if (currentState != RUNNING && (now - idleTimer > 120000)) {
    idleTimer = now;
    isInverted = !isInverted;
    display.invertDisplay(isInverted);
    needsRedraw = true;
  } else if (currentState == RUNNING && isInverted) {
    isInverted = false;
    display.invertDisplay(false);
  }

  if (!needsRedraw) return;
  needsRedraw = false;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(2 + shiftX, 2 + shiftY);
  display.print(isBreakMode ? "BREAK TIME" : "FOCUS TIME");
  display.setCursor(84 + shiftX, 2 + shiftY);
  display.print("XP:");
  display.print(completedSessions);

  // Countdown
  int mins = remainingSeconds / 60;
  int secs = remainingSeconds % 60;
  display.setTextSize(3);
  display.setCursor(16 + shiftX, 16 + shiftY);
  if (mins < 10) display.print("0");
  display.print(mins);
  display.print(":");
  if (secs < 10) display.print("0");
  display.print(secs);

  // AHT20 Info
  display.setTextSize(1);
  display.setCursor(2 + shiftX, 42 + shiftY);
  display.print("Room: ");
  if (sensorConnected) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    display.print(temp.temperature, 1);
    display.print((char)247);
    display.print("C");
  } else {
    display.print("N/A");
  }

  // Progress Bar
  long totalSecs = targetMinutes * 60L;
  int barWidth = map(remainingSeconds, 0, totalSecs, 0, 120);
  display.drawRect(4, 53, 120, 9, SSD1306_WHITE);
  display.fillRect(4, 53, barWidth, 9, SSD1306_WHITE);

  display.display();
}