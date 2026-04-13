#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <ctype.h>
#include <Preferences.h>

/* ================= MOTOR SPEED CONTROL =================
motorSpeed - keep it above 1k; this is in micro seconds (1000000 = 1 sec)
stopTime - this can be 0; its unit is milli seconds (1000 = 1 sec)
*/

// ================= DISPLAY & TOUCH =================
#define TOUCH_CS   5
#define TOUCH_IRQ  27

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Preferences preferences;

// ================= STEPPER (A4988) =================
#define STEP_PIN   26
#define DIR_PIN    33
#define ENABLE_PIN 25

const int STEPS_PER_REV = 200;

// ================= UI STATE =================
enum Page {
  PAGE_HOME,
  PAGE_ANGLE_SETUP,
  PAGE_SPEED_SETUP,
  PAGE_TEST,
  PAGE_KEYPAD
};

Page currentPage = PAGE_HOME;

// ================= VARIABLES =================
int openAngle;
int closeAngle;
unsigned long turnCount;
unsigned long motorSpeed, stopTime;
bool testRunning = false;
bool directionOpening = true;

// keypad
String keypadBuffer = "";
int* keypadTarget = nullptr;

// ================= NON-BLOCKING MOTOR STATE =================
enum TestState {
  TEST_IDLE,
  TEST_STEPPING,
  TEST_WAITING
};

TestState currentTestState = TEST_IDLE;
int currentStepsTotal = 0;
int currentStepsDone = 0;
bool stepPinState = false;
unsigned long lastMotorTime = 0;

// ================= HELPERS =================
bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return (x > rx && x < rx + rw && y > ry && y < ry + rh);
}

void drawButton(int x, int y, int w, int h, const char* txt) {
  tft.fillRoundRect(x, y, w, h, 8, TFT_BLUE);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString(txt, x + w / 2, y + h / 2);
}

void drawBackArrow() {
  tft.fillTriangle(10, 20, 30, 10, 30, 30, TFT_WHITE);
}

int degreesToSteps(int deg) {
  return (deg * STEPS_PER_REV) / 360;
}

// ================= PAGE DRAW =================
void drawHome() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Lock Tester", 120, 40);

  drawButton(20, 70, 200, 60, "Angle Setup");
  drawButton(20, 140, 200, 60, "Speed Setup");
  drawButton(20, 210, 200, 60, "Test Start");
}

void drawAngleSetup() {
  tft.fillScreen(TFT_BLACK);
  drawBackArrow();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Angle Setup", 120, 30);

  tft.setTextFont(2);             
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Open Angle", 30, 95);
  drawButton(140, 80, 70, 40, String(openAngle).c_str());

  tft.setTextFont(2);      
  tft.setTextDatum(TL_DATUM);       
  tft.drawString("Close Angle", 30, 155);
  drawButton(140, 140, 70, 40, String(closeAngle).c_str());
}

void drawSpeedSetup() {
  tft.fillScreen(TFT_BLACK);
  drawBackArrow();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Speed Setup", 120, 30);

  tft.setTextFont(2);             
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Speed (us)", 30, 95);
  drawButton(140, 80, 70, 40, String((int)motorSpeed).c_str());

  tft.setTextFont(2);      
  tft.setTextDatum(TL_DATUM);       
  tft.drawString("Stop (ms)", 30, 155);
  drawButton(140, 140, 70, 40, String((int)stopTime).c_str());
}

void drawTestPage() {
  tft.fillScreen(TFT_BLACK);
  drawBackArrow();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Test Running", 120, 30);

  tft.setTextFont(8);
  tft.drawString(String(turnCount), 120, 110);

  tft.setTextFont(2);
  tft.drawString("turns completed", 120, 155);
  if(directionOpening)
    tft.drawString("Opening", 120, 180);
  else
    tft.drawString("Closing", 120, 180);

  drawButton(10, 220, 70, 50, "START");
  drawButton(85, 220, 70, 50, "STOP");
  drawButton(160, 220, 70, 50, "RESET");
}

void drawKeypad() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(20, 40, 200, 40, TFT_WHITE);

  tft.setTextFont(4);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(keypadBuffer, 30, 48);

  const char* keys[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"DEL","0","OK"}
  };

  int x0 = 20, y0 = 100, w = 60, h = 45;

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      drawButton(x0 + c * 70, y0 + r * 55, w, h, keys[r][c]);
    }
  }
}

// ================= PAGE SWITCH =================
void redraw() {
  if (currentPage == PAGE_HOME) drawHome();
  else if (currentPage == PAGE_ANGLE_SETUP) drawAngleSetup();
  else if (currentPage == PAGE_SPEED_SETUP) drawSpeedSetup();
  else if (currentPage == PAGE_TEST) drawTestPage();
  else if (currentPage == PAGE_KEYPAD) drawKeypad();
}

void switchPage(Page p) {
  if (currentPage == PAGE_TEST) {
    testRunning = false;
    currentTestState = TEST_IDLE; // Reset motor state when leaving test page
  }
  currentPage = p;
  redraw();
}

// ================= TOUCH HANDLING =================
void openKeypad(int* target) {
  keypadTarget = target;
  keypadBuffer = String(*target);
  switchPage(PAGE_KEYPAD);
}

void handleTouch(int x, int y) {

  if (currentPage == PAGE_HOME) {
    if (inRect(x,y,20,70,200,60)) switchPage(PAGE_ANGLE_SETUP);
    if (inRect(x,y,20,140,200,60)) switchPage(PAGE_SPEED_SETUP);
    if (inRect(x,y,20,210,200,60)) switchPage(PAGE_TEST);
  }

  else if (currentPage == PAGE_ANGLE_SETUP) {
    if (inRect(x,y,0,0,40,40)) switchPage(PAGE_HOME);
    if (inRect(x,y,140,80,70,40)) openKeypad(&openAngle);
    if (inRect(x,y,140,140,70,40)) openKeypad(&closeAngle);
  }

  else if (currentPage == PAGE_SPEED_SETUP) {
    if (inRect(x,y,0,0,40,40)) switchPage(PAGE_HOME);
    if (inRect(x,y,140,80,70,40)) openKeypad((int*)&motorSpeed);
    if (inRect(x,y,140,140,70,40)) openKeypad((int*)&stopTime);
  }

  else if (currentPage == PAGE_TEST) {
    if (inRect(x,y,0,0,40,40)) switchPage(PAGE_HOME);
    if (inRect(x,y,10,220,70,50)) testRunning = true;
    if (inRect(x,y,85,220,70,50)) {
      testRunning = false;
      currentTestState = TEST_IDLE; // Safely stop the motor logic
    }
    if (inRect(x,y,160,220,70,50)) {
      testRunning = false;
      currentTestState = TEST_IDLE;
      turnCount = 0;
      directionOpening = true; // Reset direction
      drawTestPage();
    }
  }

  else if (currentPage == PAGE_KEYPAD) {
    int x0 = 20, y0 = 100, w = 60, h = 45;

    const char* keyMap[] = {
      "1","2","3","4","5","6","7","8","9","DEL","0","OK"
    };

    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 3; c++) {
        if (inRect(x,y,x0+c*70,y0+r*55,w,h)) {
          const char* key = keyMap[r*3+c];

          if (!strcmp(key,"DEL") && keypadBuffer.length())
            keypadBuffer.remove(keypadBuffer.length()-1);

          else if (!strcmp(key,"OK")) {
            *keypadTarget = keypadBuffer.toInt();
            keypadBuffer = "";

            if((int*)&openAngle == keypadTarget){
              preferences.putInt("openAngle", openAngle);
              switchPage(PAGE_ANGLE_SETUP);
            }
            else if((int*)&closeAngle == keypadTarget){
              preferences.putInt("closeAngle", closeAngle);
              switchPage(PAGE_ANGLE_SETUP);
            }
            else if((int*)&motorSpeed == keypadTarget){
              preferences.putULong("motorSpeed", motorSpeed);
              switchPage(PAGE_SPEED_SETUP);
            }
            else if((int*)&stopTime == keypadTarget){
              preferences.putULong("stopTime", stopTime);
              switchPage(PAGE_SPEED_SETUP);
            }
            
            return;
          }
          else if (isdigit(key[0]) && keypadBuffer.length() < 6)
            keypadBuffer += key;

          drawKeypad();
          return;
        }
      }
    }
  }
}

// ================= NON-BLOCKING MOTOR LOGIC =================
void handleTest() {
  if (!testRunning) return;

  // 1. Setup the next movement if we are idle
  if (currentTestState == TEST_IDLE) {
    digitalWrite(ENABLE_PIN, LOW);
    if (directionOpening) {
      digitalWrite(DIR_PIN, LOW);
      currentStepsTotal = degreesToSteps(openAngle);
    } else {
      digitalWrite(DIR_PIN, HIGH);
      currentStepsTotal = degreesToSteps(closeAngle);
    }
    currentStepsDone = 0;
    stepPinState = false;
    lastMotorTime = micros();
    currentTestState = TEST_STEPPING;
  }

  // 2. Handle stepping based on microseconds
  if (currentTestState == TEST_STEPPING) {
    if (micros() - lastMotorTime >= motorSpeed) {
      lastMotorTime = micros();
      
      stepPinState = !stepPinState; // Toggle the pin state
      digitalWrite(STEP_PIN, stepPinState ? HIGH : LOW);

      // We complete one full step when the pin goes back LOW
      if (!stepPinState) {
        currentStepsDone++;
        
        // Are we done moving?
        if (currentStepsDone >= currentStepsTotal) {
          currentTestState = TEST_WAITING;
          lastMotorTime = millis(); // Swap to tracking milliseconds for the delay
          drawTestPage(); // Update UI
        }
      }
    }
  }

  // 3. Handle the delay between cycles based on milliseconds
  if (currentTestState == TEST_WAITING) {
    if (millis() - lastMotorTime >= stopTime) {
      // Delay is over, update direction and count
      if (directionOpening) {
        directionOpening = false;
      } else {
        directionOpening = true;
        turnCount++;
        if (turnCount % 100 == 0) {
          preferences.putULong("turnCount", turnCount);
        }
      }
      
      drawTestPage(); // Refresh text on screen
      currentTestState = TEST_IDLE; // Go back to IDLE to immediately start the next cycle
    }
  }
}

// ================= SETUP & LOOP =================
void setup() {
  preferences.begin("settings", false);
  openAngle  = preferences.getInt("openAngle", 180);
  closeAngle = preferences.getInt("closeAngle", 180);
  turnCount = preferences.getULong("turnCount", 0);
  motorSpeed = preferences.getULong("motorSpeed", 3000);
  stopTime = preferences.getULong("stopTime", 1000);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH); // Disable motor initially

  tft.init();
  tft.setRotation(0);
  ts.begin();
  ts.setRotation(0);

  redraw();
}

void loop() {
  // Use a smaller, non-blocking delay for touch polling
  static unsigned long lastTouchTime = 0;
  if (millis() - lastTouchTime > 100) { 
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      int x = map(p.x, 200, 3800, 0, 240);
      int y = map(p.y, 3800, 200, 0, 320);
      handleTouch(x, y);
      lastTouchTime = millis();
    }
  }

  handleTest();
}