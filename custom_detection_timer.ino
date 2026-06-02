// ECE342 Project Code
// Jaime Petersen
// Ryan Jimerson
// Justin Sanchez


#include <Wire.h>
#include <DIYables_LCD_I2C.h>
#include <Adafruit_VL53L0X.h>


// Display I2C
#define SDA_LCD 14
#define SCL_LCD 27

// Sensor I2C
#define SDA_TOF 21
#define SCL_TOF 22


// Hardware Pins
const int ALARM_PIN = 19;   
const int BTN1_PIN = 26;    
const int BTN2_PIN = 25;    

// PWM stuff
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 500;
const int PWM_RESOLUTION = 8;
const int BL_PIN = 32;

// Brightness levels
int brightnessLevels[3] = {40, 110, 255};
int currentLevel = 2;


DIYables_LCD_I2C lcd(0x27, 16, 2);
TwoWire I2C_TOF = TwoWire(1);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();


// Timer Settings
const int timerLengths[2] = {300, 600}; // 5 minutes and 10 minutes in seconds
int timerIndex = 0;                     // 0 = 5 min, 1 = 10 min
int remainingSeconds = timerLengths[timerIndex];
unsigned long lastTick = 0;

// State Tracking
bool isRunning = false;
bool isAlarming = false;
int currentDistanceCM = 999;

// Max detection distance
const int START_DISTANCE_CM = 5;


// Button Debounce Tracking
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
const int DEBOUNCE_DELAY = 250;


void setup() {
  Serial.begin(115200);

  // Button setup
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  // Prevent alarm from screeching annoyingly
  pinMode(ALARM_PIN, INPUT);

  // Start display
  Wire.begin(SDA_LCD, SCL_LCD, 100000);
  lcd.init();
  lcd.backlight();

  // PWM setup
  ledcAttachChannel(BL_PIN, PWM_FREQ, PWM_RESOLUTION, PWM_CHANNEL);
  ledcWriteChannel(PWM_CHANNEL, brightnessLevels[currentLevel]);

  
  I2C_TOF.begin(SDA_TOF, SCL_TOF, 100000);
 
  lcd.setCursor(0, 0);
  lcd.print("Init Sensor...");
 
  if (!lox.begin(VL53L0X_I2C_ADDR, false, &I2C_TOF)) {
    lcd.clear();
    lcd.print("Sensor Error!");
    while(1); // Halt if sensor fails
  }

  lcd.clear();
  updateDisplay();
}

// --- Main Loop ---
void loop() {
  unsigned long currentMillis = millis();
 
  // Timer mode button
  if (digitalRead(BTN1_PIN) == LOW) {
    if (currentMillis - lastBtn1Press > DEBOUNCE_DELAY) {
      timerIndex = (timerIndex + 1) % 2;
      remainingSeconds = timerLengths[timerIndex]; 
     
      // make alarm shut up if previous attempts fail
      if (isAlarming) {
        isAlarming = false;
        ledcDetach(ALARM_PIN);
        pinMode(ALARM_PIN, INPUT);
      }
     
      updateDisplay();
      lastBtn1Press = currentMillis;
    }
  }

  // Brightness button
  if (digitalRead(BTN2_PIN) == LOW) {
    if (currentMillis - lastBtn2Press > DEBOUNCE_DELAY) {
      currentLevel = (currentLevel + 1) % 3;
      ledcWriteChannel(PWM_CHANNEL, brightnessLevels[currentLevel]);
      lastBtn2Press = currentMillis;
    }
  }



  // Read the sensor
  if (!isAlarming) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    // only process valid connections
    if (measure.RangeStatus == 0) {
      currentDistanceCM = measure.RangeMilliMeter / 10;
    } else {
      currentDistanceCM = 999;
    }

    // Stop timer if object not close
    if (currentDistanceCM <= START_DISTANCE_CM && remainingSeconds > 0) {
      isRunning = true;
    } else {
      isRunning = false;
    }
   
    // Slow rate at which time shows up on display
    static unsigned long lastDispUpdate = 0;
    if (currentMillis - lastDispUpdate > 200) {
      updateDisplay();
      lastDispUpdate = currentMillis;
    }
  }


  // Timer logic
  if (isRunning && !isAlarming) {
    if (currentMillis - lastTick >= 1000) { // 1 second has passed
      remainingSeconds--;
      lastTick = currentMillis;
      updateDisplay();

      if (remainingSeconds <= 0) {
        // sound the alarm
        isRunning = false;
        isAlarming = true;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("   TIMES UP!   ");
       
        // ensure alarm is 440Hz
        pinMode(ALARM_PIN, OUTPUT);
        ledcAttach(ALARM_PIN, 440, 8);
        ledcWrite(ALARM_PIN, 128);
      }
    }
  }
}

// misc functions used in code
void updateDisplay() {
  if (isAlarming) return;

  int minutes = remainingSeconds / 60;
  int seconds = remainingSeconds % 60;

  // Print distance to object
  lcd.setCursor(0, 0);
  lcd.print("Dist: ");
  if (currentDistanceCM == 999) {
    lcd.print("mucho sadness"); // Show dashes if out of range
  } else {
    lcd.print(currentDistanceCM);
    lcd.print("cm   ");
  }
 
  // Print time left
  lcd.setCursor(0, 1);
  lcd.print("Time:  ");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);
  lcd.print(":");
  if (seconds < 10) lcd.print("0");
  lcd.print(seconds);
  lcd.print("  ");
}