#include "HX711.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ESP32Servo.h> 
#include <ESP32Encoder.h> // --- ADDED ENCODER LIBRARY ---

const int LOADCELL_DOUT_PIN = 4; 
const int LOADCELL_SCK_PIN = 1;  // *Reminder: Consider moving this from Pin 1 later!

const int servoPin = 5; // *Note: If you actually hook up the servo later, you will need to change this pin since the encoder is using 5 now.

Servo honeyValve;
TFT_eSPI tft = TFT_eSPI(); 

// --- ENCODER SETUP ---
ESP32Encoder encoder;
const int ENCODER_CLK = 5;
const int ENCODER_DT = 6;
const int ENCODER_SW = 7;

long startup_baseline = 0;
long tare_offset = 0;
float calibration_factor = 1035.0; // <--------------------------------------------------------------------------------------------------------------------------------------------------------------

int previous_fill_y = 198; 
float previous_needle_weight = -999.0; 

// --- STATE MACHINE ---
enum ScaleState { STATE_EMPTY, STATE_TARING, STATE_POURING };
ScaleState current_state = STATE_EMPTY;

// --- CUSTOM 16-BIT COLORS ---
#define HONEY_DARK  tft.color565(180, 100, 0)   
#define HONEY_MID   tft.color565(240, 160, 0)   
#define HONEY_LIGHT tft.color565(255, 210, 50)  
#define GOLD_LID    tft.color565(210, 170, 50)
#define GLASS_EDGE  tft.color565(150, 150, 170) 
#define NEEDLE_RED  tft.color565(255, 0, 0)

long read_hx711_software_timed() {
  long count = 0;
  pinMode(LOADCELL_DOUT_PIN, INPUT);
  pinMode(LOADCELL_SCK_PIN, OUTPUT);
  digitalWrite(LOADCELL_SCK_PIN, LOW);

  unsigned long timeout = millis();
  while (digitalRead(LOADCELL_DOUT_PIN) == HIGH) {
    if (millis() - timeout > 150) return 0; 
  }

  for (int i = 0; i < 24; i++) {
    digitalWrite(LOADCELL_SCK_PIN, HIGH);
    delayMicroseconds(1); 
    count = count << 1;
    digitalWrite(LOADCELL_SCK_PIN, LOW);
    delayMicroseconds(1); 
    if (digitalRead(LOADCELL_DOUT_PIN)) count++;
  }

  digitalWrite(LOADCELL_SCK_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(LOADCELL_SCK_PIN, LOW);
  delayMicroseconds(1);

  if (count & 0x800000) count |= 0xFF000000;
  return count;
}

void drawScaleGraphic() {
  tft.fillRect(0, 0, 130, 204, TFT_BLACK); 
  
  // Top Weighing Pan
  tft.fillRect(30, 20, 70, 8, TFT_LIGHTGREY);
  tft.fillRect(55, 28, 20, 10, TFT_DARKGREY);
  
  // The Main Stand 
  tft.fillRect(45, 120, 40, 50, TFT_LIGHTGREY);
  
  // Base Platform
  tft.fillRect(15, 170, 100, 12, TFT_DARKGREY);
  
  // Little Rubber Feet 
  tft.fillRect(25, 182, 12, 4, TFT_LIGHTGREY); 
  tft.fillRect(93, 182, 12, 4, TFT_LIGHTGREY); 
  
  // Round Gauge Bezel
  tft.fillCircle(65, 80, 45, TFT_WHITE);
  tft.drawCircle(65, 80, 45, TFT_DARKGREY);
  tft.drawCircle(65, 80, 43, TFT_DARKGREY);
  
  updateScaleNeedle(0.0); 
}

void updateScaleNeedle(float weight) {
  tft.fillCircle(65, 80, 41, TFT_WHITE);

  tft.drawLine(65, 39, 65, 43, TFT_BLACK);   // 12 o'clock
  tft.drawLine(106, 80, 102, 80, TFT_BLACK); // 3 o'clock
  tft.drawLine(24, 80, 28, 80, TFT_BLACK);   // 9 o'clock
  tft.drawLine(86, 44, 83, 49, TFT_BLACK);   // Approx 1:30
  tft.drawLine(44, 44, 47, 49, TFT_BLACK);   // Approx 10:30

  float clamped_weight = weight;
  if (clamped_weight < 0.0) clamped_weight = 0.0;
  if (clamped_weight > 200.0) clamped_weight = 200.0;

  float angle_deg = 225.0 + (clamped_weight / 200.0) * 90.0;
  float angle_rad = angle_deg * 0.0174533; 

  int needle_length = 35;
  int needle_x = 65 + (int)(cos(angle_rad) * needle_length);
  int needle_y = 80 + (int)(sin(angle_rad) * needle_length);

  tft.drawLine(65, 80, needle_x, needle_y, NEEDLE_RED);
  tft.fillCircle(65, 80, 4, NEEDLE_RED); 
}

void drawJarOutline() {
  tft.fillRect(10, 5, 115, 195, TFT_BLACK); 
  
  // Metallic Lid
  tft.fillRect(45, 6, 50, 10, GOLD_LID); 
  tft.drawRect(45, 6, 50, 10, HONEY_DARK); 
  tft.drawFastHLine(45, 8, 50, TFT_WHITE); 

  // Glass Frame Profiles
  tft.drawLine(49, 16, 49, 35, GLASS_EDGE); 
  tft.drawLine(91, 16, 91, 35, GLASS_EDGE); 
  tft.drawLine(49, 35, 19, 60, GLASS_EDGE); 
  tft.drawLine(91, 35, 121, 60, GLASS_EDGE);
  tft.drawLine(19, 60, 19, 180, GLASS_EDGE); 
  tft.drawLine(121, 60, 121, 180, GLASS_EDGE);
  tft.drawLine(19, 180, 34, 200, GLASS_EDGE); 
  tft.drawLine(121, 180, 106, 200, GLASS_EDGE);

  tft.drawLine(50, 16, 50, 35, TFT_WHITE); 
  tft.drawLine(90, 16, 90, 35, TFT_WHITE); 
  tft.drawLine(50, 35, 20, 60, TFT_WHITE); 
  tft.drawLine(90, 35, 120, 60, TFT_WHITE);
  tft.drawLine(20, 60, 20, 180, TFT_WHITE); 
  tft.drawLine(120, 60, 120, 180, TFT_WHITE);
  tft.drawLine(20, 180, 35, 199, TFT_WHITE); 
  tft.drawLine(120, 180, 105, 199, TFT_WHITE);
  tft.drawLine(35, 199, 105, 199, TFT_WHITE); 
}

void drawJarLine(int y, bool erase) {
  int x_start, width;
  if (y >= 180 && y <= 198) {
    float frac = (198 - y) / 18.0;
    x_start = 35 - (int)(15 * frac); width = 140 - x_start * 2;
  } else if (y >= 60 && y < 180) {
    x_start = 20; width = 100;
  } else if (y >= 35 && y < 60) {
    float frac = (60 - y) / 25.0;
    x_start = 20 + (int)(30 * frac); width = 140 - x_start * 2;
  } else if (y >= 16 && y < 35) {
    x_start = 50; width = 40;
  } else {
    return;
  }
  
  if (erase) {
    tft.drawFastHLine(x_start + 1, y, width - 2, TFT_BLACK);
  } else {
    tft.drawFastHLine(x_start + 1, y, width - 2, HONEY_DARK);
    int mid_w = width * 0.8; int mid_x = x_start + (width * 0.1);
    tft.drawFastHLine(mid_x, y, mid_w, HONEY_MID);
    int light_w = width * 0.4; int light_x = x_start + (width * 0.3);
    tft.drawFastHLine(light_x, y, light_w, HONEY_LIGHT);
    int reflection_w = max(2, (int)(width * 0.05)); int reflection_x = x_start + (width * 0.15);
    tft.drawFastHLine(reflection_x, y, reflection_w, TFT_WHITE);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100); 
  
  // --- ENCODER HARDWARE INITIALIZATION ---
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENCODER_DT, ENCODER_CLK);
  encoder.setCount(0);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  tft.init(); 
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);

  drawScaleGraphic();
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Current Weight:", 140, 30, 2);
  tft.drawString("Target: 1000.0 g", 140, 130, 2);

  tft.drawFastHLine(0, 205, 320, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("t=TARE | u=UNTARE | w/s=CALIB", 160, 215, 2);

  long sample_sum = 0; int valid_samples = 0;
  for(int i = 0; i < 8; i++) {
    long reading = read_hx711_software_timed();
    if(reading != 0) { sample_sum += reading; valid_samples++; }
    delay(20);
  }
  if(valid_samples > 0) {
    startup_baseline = sample_sum / valid_samples; 
    tare_offset = startup_baseline;
  }

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  honeyValve.setPeriodHertz(50); 
  
  // Note: Since servoPin is 5 and the encoder is using 5, 
  // the servo attach is commented out so they don't fight during this test.
  // honeyValve.attach(servoPin, 500, 2400); 
  // honeyValve.write(0);
}

void loop() {
  long raw_reading = read_hx711_software_timed();
  
  if (raw_reading != 0) {
    // Standard Load Cell Calculations
    float current_weight = (float)(raw_reading - tare_offset) / calibration_factor;
    if (abs(current_weight) < 0.2) current_weight = 0.0;
    
    float raw_total_weight = (float)(raw_reading - startup_baseline) / calibration_factor;

    // --- ENCODER TEST INJECTION ---
    // Multiply each click by 20.0 grams to make testing faster
    float fake_weight_offset = encoder.getCount() * 20.0; 
    
    // Check if the button is pressed (LOW = pressed) to clear the fake weight
    if (digitalRead(ENCODER_SW) == LOW) {
      encoder.setCount(0); // Reset dial to 0
      fake_weight_offset = 0.0;
    }

    // Add the fake weight into the real variables before the screen processes them
    current_weight += fake_weight_offset;
    raw_total_weight += fake_weight_offset;
    // ------------------------------

    Serial.print("Weight: "); Serial.print(current_weight, 1);
    Serial.print(" g   ... Total: "); Serial.print(raw_total_weight, 1);
    Serial.print(" g   ... State: "); Serial.print(current_state);
    Serial.print("  ... Calibration factor: "); Serial.println(calibration_factor);

    // --- STATE MACHINE MANAGEMENT ---
    
    if (current_state == STATE_EMPTY) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      char weightBuffer[15];
      dtostrf(current_weight, 5, 1, weightBuffer);
      strcat(weightBuffer, " g   ");
      tft.drawString(weightBuffer, 140, 60, 6);

      if (abs(current_weight - previous_needle_weight) >= 1.5) {
        updateScaleNeedle(current_weight);
        previous_needle_weight = current_weight;
      }

      if (current_weight > 200.0) {
        current_state = STATE_TARING;
      }
    }
    
    else if (current_state == STATE_TARING) {
      tft.fillRect(130, 50, 190, 60, TFT_BLACK); 

      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString("TARING... ", 140, 60, 4);
      
      long sample_sum = 0; int valid_samples = 0; int attempts = 0;
      while (valid_samples < 8 && attempts < 20) {
        long reading = read_hx711_software_timed();
        if(reading != 0) { sample_sum += reading; valid_samples++; }
        attempts++; delay(10);
      }
      if(valid_samples > 0) tare_offset = sample_sum / valid_samples;
      
      tft.fillRect(130, 50, 190, 60, TFT_BLACK); 
      
      drawJarOutline();
      previous_fill_y = 198;
      
      // Auto-reset the fake weight here so the pouring starts at 0g inside the jar!
      encoder.setCount(0); 
      
      current_state = STATE_POURING;
    }
    
    else if (current_state == STATE_POURING) {
      char weightBuffer[15];
      dtostrf(current_weight, 5, 1, weightBuffer);
      strcat(weightBuffer, " g   "); 

      if (current_weight >= 1000.0) tft.setTextColor(tft.color565(0, 255, 0), TFT_BLACK);
      else tft.setTextColor(TFT_WHITE, TFT_BLACK);
      
      tft.drawString(weightBuffer, 140, 60, 6); 

      float fill_pct = current_weight / 1000.0;
      if (fill_pct < 0.0) fill_pct = 0.0;
      if (fill_pct > 1.0) fill_pct = 1.0; 

      int max_y = 198; int min_y = 17;  
      int target_y = max_y - (int)(fill_pct * (max_y - min_y));

      if (target_y != previous_fill_y) {
        if (target_y < previous_fill_y) {
          for (int y = previous_fill_y; y >= target_y; y--) drawJarLine(y, false);
        } else {
          for (int y = previous_fill_y; y <= target_y; y++) drawJarLine(y, true);
        }
        previous_fill_y = target_y;
      }

      if (raw_total_weight < 15.0) {
        tare_offset = startup_baseline;
        previous_needle_weight = -999.0;
        
        tft.fillRect(130, 50, 190, 40, TFT_BLACK); 
        drawScaleGraphic();
        current_state = STATE_EMPTY;
      }
    }
  }

  // --- MANUAL COMMAND INTERRUPTS ---
  if (Serial.available()) {
    char temp = Serial.read();
    if (temp == 'w') calibration_factor += 1.0;
    else if (temp == 's') calibration_factor -= 1.0;
    else if (temp == 't') { 
      current_state = STATE_TARING; 
    }
    else if (temp == 'u') { 
      tare_offset = startup_baseline; 
      previous_needle_weight = -999.0;
      
      tft.fillRect(130, 50, 190, 40, TFT_BLACK); 
      drawScaleGraphic();
      current_state = STATE_EMPTY;
    }
  }
}