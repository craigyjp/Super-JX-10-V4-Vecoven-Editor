#define DISPLAYTIMEOUT 1500

#include <LiquidCrystal_PCF8574.h>

#define PULSE 1
#define VAR_TRI 2
#define FILTER_ENV 3
#define AMP_ENV 4

LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display

String currentParameter = "";
String currentValue = "";
float currentFloatValue = 0.0;
String currentPgmNum = "";
String currentPatchName = "";
String newPatchName = "";
const char *currentSettingsOption = "";
const char *currentSettingsValue = "";
int currentSettingsPart = SETTINGS;
int paramType = PARAMETER;

boolean MIDIClkSignal = false;
int Patchnumber = 0;
unsigned long timeout = 0;

void startTimer() {
  if (state == PARAMETER) {
    timeout = millis();
  }
}

void renderBootUpPage() {
  lcd.home();
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("*****  ROLAND  JX-10  *****");
  lcd.setCursor(0, 1);
  lcd.print("VECOVEN ENHANCED MODE   ");
  lcd.print("VERSION ");
  lcd.print(VERSION);
}

void renderCurrentPatchPage() {
    lcd.clear();
    // upper section of screen
    lcd.setCursor(0, 0);
    lcd.print("I:");
    lcd.setCursor(2, 0);
    lcd.print(patches.last().patchNo);
    lcd.setCursor(14, 0);
    lcd.print(patches.first().patchName);
    lcd.setCursor(35, 0);
    lcd.write(byte(0));
    lcd.print(upperData[P_toneNumber]);
    lcd.setCursor(39, 0);
    lcd.write(byte(0));
    // lower section of screen
    lcd.setCursor(0, 1);
    lcd.print("A1");
    lcd.setCursor(36, 1);
    lcd.print(lowerData[P_toneNumber]);
}

void renderCurrentParameterPage() {
  switch (state) {
    case PARAMETER:
      lcd.clear();
      // upper section of screen
      lcd.setCursor(0, 0);
      lcd.print("I:");
      lcd.setCursor(2, 0);
      lcd.print(patches.last().patchNo);
      lcd.setCursor(5, 0);
      if (displayMode == 0) {
      lcd.print("LOWER PARAMETER");
      lcd.setCursor(27, 0);
      lcd.write(byte(0));
      lcd.print("LOWER");
      lcd.write(byte(0));
      lcd.print(" UPPER");

      // lower section of screen
      lcd.setCursor(0, 1);
      lcd.setCursor(15, 1);
      lcd.print(currentParameter);
      lcd.setCursor(27, 1);
      lcd.write(byte(0));
      lcd.setCursor(28, 1);
      lcd.print(currentValue);
      lcd.setCursor(33, 1);
      lcd.write(byte(0));
      }
      if (displayMode == 1) {
        lcd.print("PATCH PARAMETER");
        lcd.setCursor(15, 1);
        lcd.print(currentParameter);
        lcd.setCursor(28, 1);
        lcd.print(currentValue);
      }
      if (displayMode == 2) {
        lcd.print("EDITING PARAMETER");
        lcd.setCursor(15, 1);
        lcd.print(currentParameter);
        lcd.setCursor(28, 1);
        lcd.print(currentValue);
      }
      break;
  }
}

void renderDeletePatchPage() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setCursor(0, 34);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.setTextSize(1);
  // tft.println("Delete?");
  // tft.drawFastHLine(10, 66, tft.width() - 20, ST7735_RED);
  // tft.fillRect(0, 75, tft.width(), 23, ST7735_RED);
  // tft.setCursor(5, 81);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.println(patches.first().patchNo);
  // tft.setCursor(40, 81);
  // tft.setTextColor(ST7735_WHITE);
  // tft.println(patches.first().patchName);
}

void renderDeleteMessagePage() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setFont(&FreeSans12pt7b);
  // tft.setCursor(2, 34);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.setTextSize(1);
  // tft.println("Renumbering");
  // tft.setCursor(10, 84);
  // tft.println("SD Card");
}

void renderSavePage() {
  lcd.clear();
  
  // Line 1: "Save?" prompt with patch number
  lcd.setCursor(0, 0);
  lcd.print("Save? Patch: ");
  lcd.print(patches.last().patchNo);
  
  // Line 2: Patch name (truncate if needed to fit 40 chars)
  lcd.setCursor(0, 1);
  String patchName = patches.last().patchName;
  if (patchName.length() > 40) {
    patchName = patchName.substring(0, 37) + "...";
  }
  lcd.print(patchName);
}

void renderReinitialisePage() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setFont(&FreeSans12pt7b);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.setTextSize(1);
  // tft.setCursor(5, 34);
  // tft.println("Initialise to");
  // tft.setCursor(5, 84);
  // tft.println("panel setting");
}

void renderPatchNamingPage() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setFont(&FreeSans12pt7b);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.setTextSize(1);
  // tft.setCursor(0, 34);
  // tft.println("Rename Patch");
  // tft.drawFastHLine(10, 66, tft.width() - 20, ST7735_RED);
  // tft.setTextColor(ST7735_WHITE);
  // tft.setCursor(5, 84);
  // tft.println(newPatchName);
}

void renderRecallPage() {
  // tft.fillScreen(ST7735_BLACK);
  // tft.setFont(&FreeSans9pt7b);
  // tft.setCursor(5, 34);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.println(patches.last().patchNo);
  // tft.setCursor(40, 34);
  // tft.setTextColor(ST7735_WHITE);
  // tft.println(patches.last().patchName);

  // tft.fillRect(0, 56, tft.width(), 23, 0xA000);
  // tft.setCursor(5, 62);
  // tft.setTextColor(ST7735_YELLOW);
  // tft.println(patches.first().patchNo);
  // tft.setCursor(40, 62);
  // tft.setTextColor(ST7735_WHITE);
  // tft.println(patches.first().patchName);

  // tft.setCursor(5, 89);
  // tft.setTextColor(ST7735_YELLOW);
  // patches.size() > 1 ? tft.println(patches[1].patchNo) : tft.println(patches.last().patchNo);
  // tft.setCursor(40, 89);
  // tft.setTextColor(ST7735_WHITE);
  // patches.size() > 1 ? tft.println(patches[1].patchName) : tft.println(patches.last().patchName);
}

void showRenamingPage(String newName) {
  newPatchName = newName;
}

void renderUpDown(uint16_t x, uint16_t y, uint16_t colour) {
  //Produces up/down indicator glyph at x,y
  // tft.setCursor(x, y);
  // tft.fillTriangle(x, y, x + 8, y - 8, x + 16, y, colour);
  // tft.fillTriangle(x, y + 4, x + 8, y + 12, x + 16, y + 4, colour);
}


void renderSettingsPage() {
  lcd.clear();
  
  // Line 1: Setting name with up/down indicators on the right
  lcd.setCursor(0, 0);
  lcd.print("PARAMETER ");
  lcd.setCursor(10, 0);
  String optionName = currentSettingsOption;
  if (optionName.length() > 27) {
    optionName = optionName.substring(0, 27);
  }
  lcd.print(optionName);
  
  // Up/down indicators for SETTINGS navigation (right side of line 1)
  lcd.setCursor(38, 0);
  if (currentSettingsPart == SETTINGS) {
    lcd.write(byte(1));  // Up triangle
  } else {
    lcd.print("  ");
  }
  
  // Line 2: Setting value with up/down indicators on the right
  lcd.setCursor(0, 1);
  lcd.print("VALUE ");
  lcd.setCursor(10, 1);
  String valueStr = String(currentSettingsValue);
  if (valueStr.length() > 27) {
    valueStr = valueStr.substring(0, 27);
  }
  lcd.print(valueStr);
  
  // Up/down indicators for SETTINGSVALUE navigation (right side of line 2)
  lcd.setCursor(38, 1);
  if (currentSettingsPart == SETTINGSVALUE) {
    lcd.write(byte(2));  // Down triangle
  } else {
    lcd.print("  ");
  }
}

void showCurrentParameterPage(const char *param, float val, int pType) {
  currentParameter = param;
  currentValue = String(val);
  currentFloatValue = val;
  paramType = pType;
  startTimer();
}

void showCurrentParameterPage(const char *param, String val, int pType) {
  if (state == SETTINGS || state == SETTINGSVALUE) state = PARAMETER;  //Exit settings page if showing
  currentParameter = param;
  currentValue = val;
  paramType = pType;
  startTimer();
}

void showCurrentParameterPage(const char *param, String val) {
  showCurrentParameterPage(param, val, PARAMETER);
}


void showPatchPage(String number, String patchName) {
  currentPgmNum = number;
  currentPatchName = patchName;
}

void showSettingsPage(const char *option, const char *value, int settingsPart) {
  currentSettingsOption = option;
  currentSettingsValue = value;
  currentSettingsPart = settingsPart;
}

void updateScreen() {
  switch (state) {
    case PARAMETER:
      if ((millis() - timeout) > DISPLAYTIMEOUT) {
        renderCurrentPatchPage();
      } else {
        renderCurrentParameterPage();
      }
      break;
    case RECALL:
      renderRecallPage();
      break;
    case SAVE:
      renderSavePage();
      break;
    case REINITIALISE:
      renderReinitialisePage();
      //tft.updateScreen();  //update before delay
      state = PARAMETER;
      break;
    case PATCHNAMING:
      renderPatchNamingPage();
      break;
    case PATCH:
      renderCurrentPatchPage();
      break;
    case DELETE:
      renderDeletePatchPage();
      break;
    case DELETEMSG:
      renderDeleteMessagePage();
      break;
    case SETTINGS:
    case SETTINGSVALUE:
      renderSettingsPage();
      break;
  }
  //tft.updateScreen();
}

void setupDisplay() {

  lcd.begin(40, 2, Wire2);  // initialize the lcd
  lcd.createChar(0, midBar2);
  lcd.createChar(1, triUpSolid);
  lcd.createChar(2, triDownSolid);
  renderBootUpPage();
}
