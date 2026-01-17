#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <ctype.h>
// ================= MOTOR SPEED CONTROL =================
#define MOTOR_SPEED 3000        // keep it above 1k; this is in micro seconds (1000000 = 1 sec)
#define STOP_TIME   1000        // this can be 0; it's unit is milli seconds (1000 = 1 sec)

// ================= DISPLAY & TOUCH =================
#define TOUCH_CS   5
#define TOUCH_IRQ  27

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ================= STEPPER (A4988) =================
#define STEP_PIN   26
#define DIR_PIN    25
#define ENABLE_PIN 33

const int STEPS_PER_REV = 200;

// ================= UI STATE =================
enum Page {
  PAGE_HOME,
  PAGE_ANGLE_SETUP,
  PAGE_TEST,
  PAGE_KEYPAD
};

Page currentPage = PAGE_HOME;

// ================= VARIABLES =================
int openAngle  = 180;
int closeAngle = 180;

bool testRunning = false;
bool directionOpening = true;
unsigned long turnCount = 0;

// keypad
String keypadBuffer = "";
int* keypadTarget = nullptr;

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

// ================= MOTOR =================
int degreesToSteps(int deg) {
  return (deg * STEPS_PER_REV) / 360;
}

void stepMotor(bool ccw, int steps) {
  digitalWrite(ENABLE_PIN, LOW);
  digitalWrite(DIR_PIN, ccw ? LOW : HIGH);

  for (int i = 0; i < steps && testRunning; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(MOTOR_SPEED);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(MOTOR_SPEED);
  }
  drawTestPage();
  delay(STOP_TIME);
}

void handleTest() {
  if (!testRunning) return;

  if (directionOpening) {
    stepMotor(true, degreesToSteps(openAngle));
    directionOpening = false;
  } else {
    stepMotor(false, degreesToSteps(closeAngle));
    directionOpening = true;
    turnCount++;
    drawTestPage();   // refresh counter
  }
}

// ================= PAGE DRAW =================
void drawHome() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString("Lock Tester", 120, 40);

  drawButton(20, 90, 200, 70, "Angle Setup");
  drawButton(20, 190, 200, 70, "Test Start");
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
  else if (currentPage == PAGE_TEST) drawTestPage();
  else if (currentPage == PAGE_KEYPAD) drawKeypad();
}

void switchPage(Page p) {
  if (currentPage == PAGE_TEST) testRunning = false;
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
    if (inRect(x,y,20,90,200,70)) switchPage(PAGE_ANGLE_SETUP);
    if (inRect(x,y,20,190,200,70)) switchPage(PAGE_TEST);
  }

  else if (currentPage == PAGE_ANGLE_SETUP) {
    if (inRect(x,y,0,0,40,40)) switchPage(PAGE_HOME);
    if (inRect(x,y,140,80,70,40)) openKeypad(&openAngle);
    if (inRect(x,y,140,140,70,40)) openKeypad(&closeAngle);
  }

  else if (currentPage == PAGE_TEST) {
    if (inRect(x,y,0,0,40,40)) switchPage(PAGE_HOME);
    if (inRect(x,y,10,220,70,50)) testRunning = true;
    if (inRect(x,y,85,220,70,50)) testRunning = false;
    if (inRect(x,y,160,220,70,50)) {
      testRunning = false;
      turnCount = 0;
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
            switchPage(PAGE_ANGLE_SETUP);
            return;
          }
          else if (isdigit(key[0]) && keypadBuffer.length() < 3)
            keypadBuffer += key;

          drawKeypad();
          return;
        }
      }
    }
  }
}

// ================= SETUP & LOOP =================
void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);

  tft.init();
  tft.setRotation(0);
  ts.begin();
  ts.setRotation(0);

  redraw();
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, 0, 240);
    int y = map(p.y, 3800, 200, 0, 320);
    handleTouch(x, y);
    delay(200);
  }

  handleTest();
}
