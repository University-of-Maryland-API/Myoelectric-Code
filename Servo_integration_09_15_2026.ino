#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "EMGFilters.h"
#include <Servo.h>
#include <math.h> 

// ==========================================
// USER CALIBRATION - THE TUNING BOX
// ==========================================
unsigned long USER_THRESHOLD = 5;      
float USER_MAX_FLEX = 1000.0;         
float SENSITIVITY_CURVE = 0.25;

// --- Hold-to-Lock Timers ---
// 2000ms (2 seconds) is the sweet spot to prevent accidental triggers 
// while avoiding muscle fatigue during the hold.
const int LOCK_HOLD_TIME = 2000; 
const int UNLOCK_HOLD_TIME = 2000; 
// ==========================================

// --- Servo Configuration & Management ---
// To deactivate a servo, simply remove its pin number from this list.
// To use just one servo, change it to: {2}
const int ACTIVE_SERVO_PINS[] = {2, 3, 4, 5, 6, 7}; 
const int NUM_SERVOS = sizeof(ACTIVE_SERVO_PINS) / sizeof(ACTIVE_SERVO_PINS[0]);

Servo fingers[NUM_SERVOS]; // Dynamically sizes the array based on your list above

const unsigned long SERVO_UPDATE_RATE = 5;  
const unsigned long LOG_RATE = 50;          
int current_angle = 0;
int target_angle = 0;

// --- EMG Sensor Configuration ---
#define SensorInputPin A0
EMGFilters myFilter;
int sampleRate = SAMPLE_FREQ_1000HZ;
int humFreq = NOTCH_FREQ_60HZ;

float smoothedValue = 0;
const float ATTACK_ALPHA = 0.05;  
const float DECAY_ALPHA  = 0.003; 

// --- Grip & Lock Logic Variables ---
int flexPercentage = 0; 
bool is_locked = false;
unsigned long lock_hold_timer = 0;     
const int LOCK_THRESHOLD = 95;         
bool waiting_for_relaxation = false;    

// --- Hysteresis Settings ---
int currentStep = 0;              
const int HYSTERESIS_MARGIN = 8; 

unsigned long next_emg_sample = 0;
unsigned long next_log = 0;

void setup() {
  Serial.begin(115200);
  myFilter.init(sampleRate, humFreq, true, true, true);

  // Automatically attach only the pins you listed in ACTIVE_SERVO_PINS
  for (int i = 0; i < NUM_SERVOS; i++) {
    fingers[i].attach(ACTIVE_SERVO_PINS[i]);
    fingers[i].write(0);
  }
}

void loop() {
  unsigned long current_micros = micros();
  unsigned long current_millis = millis();

  if (current_micros - next_emg_sample >= 1000) {
    next_emg_sample = current_micros;

    int data = analogRead(SensorInputPin);
    int dataAfterFilter = myFilter.update(data);  
    unsigned long envelope = (unsigned long)dataAfterFilter * dataAfterFilter; 
    
    // 1 & 2. Asymmetric Smoothing (Peak Envelope Follower)
    if (envelope > smoothedValue) {
      smoothedValue = (ATTACK_ALPHA * envelope) + ((1.0 - ATTACK_ALPHA) * smoothedValue);
    } else {
      smoothedValue = (DECAY_ALPHA * envelope) + ((1.0 - DECAY_ALPHA) * smoothedValue);
    }

    if (smoothedValue < USER_THRESHOLD) {
      smoothedValue = 0;
    }
    
    // 3. POWER CURVE MAPPING
    if (smoothedValue > 0) {
      float normalized = smoothedValue / USER_MAX_FLEX;
      normalized = constrain(normalized, 0.0, 1.0);
      flexPercentage = (int)(pow(normalized, SENSITIVITY_CURVE) * 100.0);
    } else {
      flexPercentage = 0;
    }
    flexPercentage = constrain(flexPercentage, 0, 100);

    // --- HOLD-TO-LOCK LOGIC ---
    if (flexPercentage >= LOCK_THRESHOLD) {
      if (!waiting_for_relaxation) {
        if (lock_hold_timer == 0) {
          lock_hold_timer = current_millis; 
        } else {
          unsigned long elapsed = current_millis - lock_hold_timer;
          
          if (!is_locked && elapsed >= LOCK_HOLD_TIME) {
            is_locked = true;
            waiting_for_relaxation = true; 
            lock_hold_timer = 0;
          } 
          else if (is_locked && elapsed >= UNLOCK_HOLD_TIME) {
            is_locked = false;
            waiting_for_relaxation = true; 
            lock_hold_timer = 0;
          }
        }
      }
    } else if (flexPercentage < (LOCK_THRESHOLD - HYSTERESIS_MARGIN)) {
      lock_hold_timer = 0;
      waiting_for_relaxation = false;
    }
    
    // --- DISCRETE MAPPING ---
    if (is_locked) {
      target_angle = 120; // Ensure your 3D printed hand can physically reach this angle
    } else {
      if (flexPercentage <= 4) currentStep = 0; 
      else if (currentStep == 0 && flexPercentage > (25 + HYSTERESIS_MARGIN)) currentStep = 1;
      else if (currentStep == 1 && flexPercentage < (25 - HYSTERESIS_MARGIN)) currentStep = 0;
      else if (currentStep == 1 && flexPercentage > (50 + HYSTERESIS_MARGIN)) currentStep = 2;
      else if (currentStep == 2 && flexPercentage < (50 - HYSTERESIS_MARGIN)) currentStep = 1;
      else if (currentStep == 2 && flexPercentage > (75 + HYSTERESIS_MARGIN)) currentStep = 3;
      else if (currentStep == 3 && flexPercentage < (75 - HYSTERESIS_MARGIN)) currentStep = 2;
      else if (currentStep == 3 && flexPercentage > 95) currentStep = 4;
      else if (currentStep == 4 && flexPercentage < (95 - HYSTERESIS_MARGIN)) currentStep = 3;

      target_angle = currentStep * 30;
    }
  }

  update_servos();

  if (current_millis - next_log >= LOG_RATE) {
    next_log = current_millis;
    Serial.print("RawEnv:"); Serial.print(smoothedValue);
    Serial.print(" | Flex%:"); Serial.print(flexPercentage);
    Serial.print(" | Step:"); Serial.print(currentStep);
    Serial.print(" | Lock State:"); Serial.println(is_locked ? "LOCKED" : "UNLOCKED");
  }
}

void update_servos() {
  static unsigned long target_time = 0;
  if (current_angle != target_angle && millis() >= target_time) {
    target_time = millis() + SERVO_UPDATE_RATE;
    
    if (target_angle > current_angle) current_angle++;
    else current_angle--;
    
    // Automatically writes to however many servos are active
    for (int i = 0; i < NUM_SERVOS; i++) {
      fingers[i].write(current_angle);
    }
  }
}