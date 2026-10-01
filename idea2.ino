/*
 ============================================================================
  EMG-CONTROLLED HAND - REVISED FIRMWARE (v2)
 ============================================================================

  WHAT THIS DOES
  --------------
  Reads a muscle (EMG) sensor at 1 kHz, filters it, and turns your forearm
  muscle effort into a proportional servo angle (0..MAX_ANGLE) that drives
  all finger servos together. On power-up it calibrates itself in two steps:

    1. BASELINE  (~4 s)   Relax your arm completely.
    2. MVC       (3 reps) Flex as hard as you can when told (LED cues below).

  Then it enters ACTIVE mode with slow background drift correction.
  To recalibrate without unplugging, send the character 'r' over the Serial
  Monitor (115200 baud).

  ----------------------------------------------------------------------------
  PHYSICAL WIRING SETUP
  ----------------------------------------------------------------------------
  Arduino Uno / Nano (5 V, 16 MHz). Other boards work if they
  support the Servo and EMGFilters libraries, but check the ADC voltage.

  PARTS
    - Arduino Uno/Nano
    - DFRobot Gravity: Analog EMG Sensor (SEN0240, by DFRobot + OYMotion).
      Signal conditioner board with built-in filtering and amplification.
      Supply 3.3-5.5 V, analog output 0-3.0 V centered on 1.5 V.
    - EMG dry electrode board (PJ-342 3.5 mm plug, 50 cm cable). This is a
      single rigid 22x35mm board with all 3 metal contacts fixed in place -
      see ELECTRODE PLACEMENT below, it is NOT like a 3-lead setup with
      independently positioned electrodes.
    - Wrist belt (included) - straps the electrode board snugly to the arm.
    - 3-wire PH2.0 Gravity cable (sensor board to Arduino)
    - Hobby servos (one per finger)
    - Separate servo power supply, 5-6 V (see sizing note below)
    - 470-1000 uF electrolytic capacitor (across the servo supply)
    - 1 LED + 220 ohm resistor (optional status light)
    - Jumper wires / breadboard

  PIN MAP
    +---------------------------+---------------------------------------+
    | Arduino pin               | Connects to                           |
    +---------------------------+---------------------------------------+
    | A0                        | EMG sensor SIGNAL / OUTPUT            |
    | 5V                        | EMG sensor VCC (check your sensor's   |
    |                           | rated voltage - some are 3.3 V only)  |
    | GND                       | EMG sensor GND                        |
    | D2, D3, D4, D5, D6, D7    | Servo signal wires (orange/yellow),   |
    |                           | one servo per pin                     |
    | D12                       | LED anode (+) through 220 ohm resistor|
    | GND                       | LED cathode (-)                       |
    +---------------------------+---------------------------------------+

  SERVO POWER (IMPORTANT)
    - Do NOT power the servos from the Arduino 5V pin. Six servos can draw
      several amps when moving or stalled; the Arduino's regulator/USB port
      cannot supply that, and voltage dips will reset the board or add noise
      to the EMG readings.
    - Use a separate 5-6 V supply (battery pack or DC adapter). Size it for
      the combined STALL current of all servos (check your servo datasheet;
      micro servos are roughly 0.5-0.7 A each, full-size ones can be 2 A+).
    - Servo red wires (V+)   -> external supply +
    - Servo brown/black (GND) -> external supply -
    - Connect the external supply's GND to an Arduino GND pin (common
      ground). Without a common ground the signal wires will not work.
    - Place the 470-1000 uF capacitor across the supply + and - close to
      the servos to smooth current spikes.

  WIRING DIAGRAM (simplified)

       EMG sensor                Arduino                    Servo supply
      +----------+           +-------------+                +-----------+
      | VCC  ----+---------->| 5V          |                |  +     -  |
      | GND  ----+---------->| GND  <------+----------------+---------+ |
      | SIG  ----+---------->| A0          |                |  |        |
      +----------+           |             |                +--+--------+
                             | D2..D7 -----+--> servo signal   |   |
                             | D12 --[220]-+--> LED --> GND    |   |
                             +-------------+                   |   |
                                                 servo V+ <----+   |
                                                 servo GND <-------+
                               (470-1000 uF cap across + and - of supply)

  ELECTRODE PLACEMENT (matches the actual DFRobot dry electrode board)
    - The electrode board is a single rigid 22x35mm piece with all 3 metal
      contacts fixed in place at a set spacing. You cannot separate or
      reposition the contacts - they move as one unit. This is different
      from traditional 3-lead EMG, where you place two sensing electrodes
      and a separate reference electrode independently.
    - Per DFRobot: you do NOT need to worry about the reference contact's
      position relative to the other two. Just place the whole board on the
      belly of the target muscle (for gripping: the flexor muscles on the
      palm-side of the forearm), oriented so the board's long axis runs
      ALONG the direction of the muscle fibers (roughly elbow-to-wrist),
      not across them.
    - Find the belly of the muscle by making a fist and feeling where it
      bulges most - that's usually the strongest signal location.
    - Clean and dry the skin first (an alcohol wipe helps); shave the area
      if hair is preventing good metal-to-skin contact.
    - Use the included wrist belt/strap to hold the board firmly and
      consistently against the skin. Inconsistent pressure = inconsistent
      contact impedance = a baseline/MVC that drifts between sessions.
    - Keep the sensor cable short and secured so it doesn't tug on the
      board - cable movement shows up as false "flexes."

  NOISE TIPS
    - Running from a battery (instead of USB connected to a plugged-in
      laptop) usually gives a much cleaner EMG signal. Use the Serial Monitor
      for debugging, then run on battery for real use.
    - Keep servo power wires away from the EMG sensor and its cable.

  NOTE ON PINS: the Servo library uses Timer1, which disables analogWrite()
  (PWM) on pins 9 and 10 on Uno/Nano. Pins 2-7 are unaffected.

  ----------------------------------------------------------------------------
  LED STATUS GUIDE
  ----------------------------------------------------------------------------
    Fast blink    -> Baseline calibration (relax your arm)
    Double blink  -> Get ready to flex
    Solid on      -> FLEX NOW (recording a rep)
    Off           -> Rest between reps
    Slow blink    -> Active: hand is under your control

  SERIAL LOG FORMAT (115200 baud)
    S = state number, E = smoothed EMG envelope, F = flex %, B = runtime
    baseline, M = runtime max, A = target servo angle.
    State numbers: 0 baseline, 1 MVC ready, 2 MVC record, 3 MVC rest, 4 active

  ----------------------------------------------------------------------------
  CHANGES FROM v1 (bug fixes)
  ----------------------------------------------------------------------------
    - Baseline sampling now counts real 1 kHz samples, not loop iterations
      (the 100-point reservoir previously filled in well under a second).
    - Control and drift adaptation now run once per EMG sample, so their
      time constants no longer depend on how fast loop() spins.
    - Calibrated baseline threshold is now used as a deadband, so resting
      noise doesn't get amplified by the power curve into servo twitching.
    - Idle detection for baseline drift uses the linear signal instead of
      the post-power-curve percentage (which almost never went low enough).
    - Divide-by-zero protection on the normalization span.
    - Runtime max is capped so one cable tug can't inflate it forever.
    - Bad calibration now retries instead of continuing with a nonsense range.
    - Sample timing no longer drifts below 1 kHz (notch filter stays tuned).
    - Serial log shortened to fit the 64-byte TX buffer and avoid blocking.
    - millis() rollover-safe servo timing; per-servo inversion option added.
 ============================================================================
*/

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "EMGFilters.h"
#include <Servo.h>
#include <math.h>

// ============================================================
// PIN CONFIGURATION
// ============================================================
#define SensorInputPin A0

const int ACTIVE_SERVO_PINS[] = {2, 3, 4, 5, 6, 7};
const int NUM_SERVOS = sizeof(ACTIVE_SERVO_PINS) / sizeof(ACTIVE_SERVO_PINS[0]);
Servo fingers[NUM_SERVOS];

// Set an entry to true if that finger's servo is mounted mirrored (it would
// otherwise close when the others open). Must have NUM_SERVOS entries.
const bool SERVO_INVERTED[] = {false, false, false, false, false, false};

const int LED_PIN = 12; // optional status LED. Remove if unused.

// ============================================================
// EMG ACQUISITION CONFIG
// ============================================================
EMGFilters myFilter;
int sampleRate = SAMPLE_FREQ_1000HZ;
int humFreq = NOTCH_FREQ_60HZ; // use NOTCH_FREQ_50HZ if your mains is 50 Hz

const unsigned long SAMPLE_PERIOD_US = 1000; // 1 kHz

float smoothedValue = 0;           // EMG "envelope" (squared, asymmetrically smoothed)
const float ATTACK_ALPHA = 0.05;   // rises quickly when muscle activates
const float DECAY_ALPHA  = 0.003;  // falls slowly when muscle relaxes

unsigned long next_emg_sample = 0;
bool newSample = false; // true only on loop passes where a fresh EMG sample was taken.
                        // Everything time-constant-based keys off this, so behavior
                        // doesn't change with how fast loop() happens to run.

// ============================================================
// SERVO MOTION CONFIG
// ============================================================
const unsigned long SERVO_UPDATE_RATE = 5; // ms per 1-degree step
int current_angle = 0;
int target_angle = 0;
const int MAX_ANGLE = 120; // match your 3D printed hand's full-close angle

// ============================================================
// CALIBRATION CONFIG
// ============================================================
// Settle is 1 s because the filters start cold and the envelope decays with a
// ~330 ms time constant; 0.5 s could leak start-up transients into the baseline.
const unsigned long SETTLE_MS          = 1000;  // discard samples right after entering a phase
const unsigned long BASELINE_DURATION  = 4000;  // total ms in baseline phase (incl. settle)
const int  RESERVOIR_SIZE              = 100;   // downsampled points used for outlier rejection
const int  MIN_RESERVOIR_POINTS        = 20;    // fewer than this -> redo baseline
const float BASELINE_PERCENTILE_CUT    = 0.90;  // discard top 10% as motion/noise artifacts
const float BASELINE_THRESHOLD_STDDEVS = 3.0;   // deadband = N * stddev
const float MIN_DEADBAND_RATIO         = 0.10;  // deadband is at least 10% of baseline mean...
const float MIN_DEADBAND_ABS           = 1.0;   // ...and at least this (in envelope units)

const int  MVC_REPS            = 3;
const unsigned long MVC_GETREADY_MS = 1000;
const unsigned long MVC_REP_MS      = 1500;
const unsigned long MVC_REST_MS     = 2000;

// If the MVC median isn't at least this many times the baseline threshold, the
// signal is probably bad (electrode contact, placement) and calibration restarts.
// Lower it if you legitimately have a weak signal.
const float MVC_MIN_RATIO = 3.0;

// ============================================================
// RUNTIME (POST-CALIBRATION) STATE
// ============================================================
float calBaselineMean = 0;
float calBaselineThreshold = 0;
float calDeadband = 0;      // threshold - mean; signal must exceed baseline + this to move the hand
float calMaxFlex = 1.0;

float runtimeBaseline = 0;   // slow-adapting floor
float runtimeMaxFlex  = 1.0; // slow-adapting ceiling

// Slow adaptation is applied once every ADAPT_DECIMATION samples instead of every single sample.
// This avoids a real 32-bit float precision issue: at typical envelope magnitudes, a per-sample
// step on the order of 1e-5 * delta can fall below the float's local precision and get silently
// dropped by the subtraction, stalling the decay. Note that "double" does NOT fix this on AVR
// (Uno/Nano) - double is just an alias for the same 32-bit float there, unlike on a true 32-bit
// MCU (e.g. Due). Taking fewer, proportionally larger steps - same overall time constant, bigger
// individual increments - keeps every step safely above the precision floor instead.
const int ADAPT_DECIMATION = 100; // apply slow adaptation every 100 samples (~100 ms @ 1 kHz)
int adaptSampleCounter = 0;

// Time constant = 1 / (alpha_per_sample * 1000) seconds, where alpha_per_sample = alpha / ADAPT_DECIMATION.
const float BASELINE_ADAPT_ALPHA = 0.00002 * ADAPT_DECIMATION; // ~50 s time constant
const float MAXFLEX_DECAY_ALPHA  = 0.00001 * ADAPT_DECIMATION; // ~100 s time constant
const float IDLE_FRACTION        = 0.05; // adapt baseline only while activation is below 5% of range

const float MVC_FLOOR_RATIO   = 0.4; // runtime max never decays below this fraction of calibrated MVC
const float MVC_CEILING_RATIO = 1.5; // ...and never jumps above this (so one cable tug can't wreck it)
const float MIN_SPAN          = 1.0; // smallest usable (max - lower edge); below this, output is 0

const float SENSITIVITY_CURVE = 0.35; // power curve exponent (lower = more low-end sensitivity)

int flexPercentage = 0;

// ============================================================
// STATE MACHINE
// ============================================================
enum SystemState {
  STATE_CALIB_BASELINE,   // 0
  STATE_CALIB_MVC_READY,  // 1
  STATE_CALIB_MVC_RECORD, // 2
  STATE_CALIB_MVC_REST,   // 3
  STATE_ACTIVE            // 4
};
SystemState state = STATE_CALIB_BASELINE;

unsigned long phase_start_ms = 0;

// baseline calibration working vars
float reservoir[RESERVOIR_SIZE];
int reservoirIndex = 0;
unsigned long baselineSampleCounter = 0;
unsigned long baselineSampleStride = 1;

// MVC calibration working vars
float mvcPeaks[MVC_REPS];
int mvcRepIndex = 0;
float mvcCurrentRepPeak = 0;

// Forward declarations (so this also compiles outside the Arduino IDE)
void enterState(SystemState newState);
void finishMvcCalibration();
void backgroundRecalibrate(float normalized);

// ============================================================
// LED STATUS PATTERNS (non-blocking)
// ============================================================
enum LedPattern { LED_OFF, LED_ON, LED_BLINK_FAST, LED_BLINK_SLOW, LED_BLINK_DOUBLE };
LedPattern ledPattern = LED_OFF;
unsigned long ledTimer = 0;
bool ledState = false;

//sets the LED state by case
void updateLed() {
  unsigned long now = millis();
  switch (ledPattern) {
    case LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
    case LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
    case LED_BLINK_FAST:
      if (now - ledTimer >= 150) { ledTimer = now; ledState = !ledState; digitalWrite(LED_PIN, ledState); }
      break;
    case LED_BLINK_SLOW:
      if (now - ledTimer >= 700) { ledTimer = now; ledState = !ledState; digitalWrite(LED_PIN, ledState); }
      break;
    case LED_BLINK_DOUBLE: {
      unsigned long t = now % 1200;
      bool on = (t < 120) || (t > 240 && t < 360);
      digitalWrite(LED_PIN, on ? HIGH : LOW);
      break;
    }
  }
}

// initializes servo locations and flips inverted servos
void writeAllServos(int angle) {
  for (int i = 0; i < NUM_SERVOS; i++) {
    int target = SERVO_INVERTED[i] ? (MAX_ANGLE - angle) : angle;
    target = constrain(target, 0, 180);
    fingers[i].write(target);
  }
}

// initializes the filter, LED pin, emg sampler, calibration state, and servo angles
void setup() {
  Serial.begin(115200);
  myFilter.init(sampleRate, humFreq, true, true, true);
  for (int i = 0; i < NUM_SERVOS; i++) {
    fingers[i].attach(ACTIVE_SERVO_PINS[i]);
  }
  writeAllServos(0);
  current_angle = 0;
  target_angle = 0;

  pinMode(LED_PIN, OUTPUT); // the LED is a plain digital output, so it does need this

  next_emg_sample = micros();
  enterState(STATE_CALIB_BASELINE);
}

// ============================================================
// EMG SAMPLING
// Called every loop; only actually samples once per SAMPLE_PERIOD_US.
// Pipeline: read -> EMGFilters (notch/high-pass/low-pass) -> square ->
// asymmetric smoothing (fast attack, slow decay) = "envelope".
// Sets newSample = true when a sample was taken this pass.
// ============================================================
void sampleEMG() {
  unsigned long now = micros();
  if ((long)(now - next_emg_sample) < 0) return; // not time yet (rollover-safe)

  // Advance the schedule by a fixed period instead of resetting it to "now".
  // This keeps the average rate at exactly 1 kHz, which the digital notch and
  // band filters in EMGFilters assume.
  next_emg_sample += SAMPLE_PERIOD_US;
  // If we fell badly behind (e.g. a long blocking call), resync instead of
  // bursting many catch-up samples.
  if ((long)(now - next_emg_sample) > 5000) next_emg_sample = now + SAMPLE_PERIOD_US;

  int data = analogRead(SensorInputPin);
  int dataAfterFilter = myFilter.update(data);
  // filtered value can be negative; squaring as signed long is explicit and safe
  float envelope = (float)((long)dataAfterFilter * (long)dataAfterFilter);

  if (envelope > smoothedValue) {
    smoothedValue = (ATTACK_ALPHA * envelope) + ((1.0 - ATTACK_ALPHA) * smoothedValue);
  } else {
    smoothedValue = (DECAY_ALPHA * envelope) + ((1.0 - DECAY_ALPHA) * smoothedValue);
  }
  newSample = true;
}

// ============================================================
// STATE TRANSITIONS
// Called any time the state changes. Centralizes what happens ONCE on entry
// (LED pattern, resetting working variables, status print, safe hand
// position) so it isn't re-triggered every loop.
// ============================================================
void enterState(SystemState newState) {
  state = newState;
  phase_start_ms = millis();

  switch (state) {
    case STATE_CALIB_BASELINE:
      ledPattern = LED_BLINK_FAST;
      reservoirIndex = 0;
      baselineSampleCounter = 0;
      // Real EMG samples (1 per ms) available after the settle period, spread
      // across the reservoir: (4000 - 1000) / 100 = 30 samples between slots.
      baselineSampleStride = (BASELINE_DURATION - SETTLE_MS) / RESERVOIR_SIZE;
      if (baselineSampleStride < 1) baselineSampleStride = 1;
      target_angle = 0; // ensure hand is/returns to safe open position during calibration
      flexPercentage = 0;
      Serial.println(F("=== CALIBRATION: relax your arm ==="));
      break;
    case STATE_CALIB_MVC_READY:
      ledPattern = LED_BLINK_DOUBLE;
      Serial.print(F("Get ready to flex hard - rep "));
      Serial.println(mvcRepIndex + 1);
      break;
    case STATE_CALIB_MVC_RECORD:
      ledPattern = LED_ON;
      mvcCurrentRepPeak = 0;
      Serial.println(F("GO - flex as hard as you can"));
      break;
    case STATE_CALIB_MVC_REST:
      ledPattern = LED_OFF;
      Serial.println(F("Rest..."));
      break;
    case STATE_ACTIVE:
      ledPattern = LED_BLINK_SLOW;
      Serial.println(F("=== ACTIVE ==="));
      break;
  }
}

// ============================================================
// CALIBRATION: BASELINE (outlier-rejected)
// 1) Waits out SETTLE_MS so the filters aren't still converging.
// 2) Every baselineSampleStride-th REAL EMG sample, stores smoothedValue in a
//    100-slot reservoir.
// 3) When the window closes: sorts the reservoir, discards the top 10%
//    (motion/twitch artifacts), computes mean and stddev on the rest, and
//    derives a deadband from them.
// ============================================================
void runBaselineCalibration() {
  unsigned long elapsed = millis() - phase_start_ms;
  if (elapsed < SETTLE_MS) return; // let filters settle before sampling

  bool reservoirFull = (reservoirIndex >= RESERVOIR_SIZE);
  if (elapsed < BASELINE_DURATION && !reservoirFull) {
    if (newSample) { // count actual samples, not loop passes
      baselineSampleCounter++;
      if (baselineSampleCounter % baselineSampleStride == 0 && reservoirIndex < RESERVOIR_SIZE) {
        reservoir[reservoirIndex++] = smoothedValue;
      }
    }
    return;
  }

  // Window complete. Guard against a too-empty reservoir (would divide by zero).
  if (reservoirIndex < MIN_RESERVOIR_POINTS) {
    Serial.println(F("Baseline failed (too few samples) - retrying"));
    enterState(STATE_CALIB_BASELINE);
    return;
  }

  // insertion sort (small array)
  for (int i = 1; i < reservoirIndex; i++) {
    float key = reservoir[i];
    int j = i - 1;
    while (j >= 0 && reservoir[j] > key) {
      reservoir[j + 1] = reservoir[j];
      j--;
    }
    reservoir[j + 1] = key;
  }

  int cutIndex = (int)(reservoirIndex * BASELINE_PERCENTILE_CUT);
  if (cutIndex < 1) cutIndex = reservoirIndex;

  float sum = 0, sumSq = 0;
  for (int i = 0; i < cutIndex; i++) sum += reservoir[i];
  float mean = sum / cutIndex;
  for (int i = 0; i < cutIndex; i++) {
    float d = reservoir[i] - mean;
    sumSq += d * d;
  }
  float stddev = sqrt(sumSq / cutIndex);

  // Deadband: how far above baseline the signal must rise before the hand moves.
  // Floors stop a perfectly flat baseline from producing a zero deadband.
  float deadband = BASELINE_THRESHOLD_STDDEVS * stddev;
  if (deadband < mean * MIN_DEADBAND_RATIO) deadband = mean * MIN_DEADBAND_RATIO;
  if (deadband < MIN_DEADBAND_ABS) deadband = MIN_DEADBAND_ABS;

  calBaselineMean = mean;
  calDeadband = deadband;
  calBaselineThreshold = mean + deadband;
  runtimeBaseline = calBaselineMean;

  Serial.print(F("Baseline mean: ")); Serial.print(calBaselineMean);
  Serial.print(F(" | threshold: ")); Serial.println(calBaselineThreshold);

  mvcRepIndex = 0;
  enterState(STATE_CALIB_MVC_READY);
}

// ============================================================
// CALIBRATION: MVC (multi-rep median)
// Three-way switch on the sub-state (READY / RECORD / REST) driven by time
// since phase_start_ms. During RECORD it tracks the highest smoothedValue of
// the rep. After each rest it starts the next rep, or after MVC_REPS calls
// finishMvcCalibration().
// ============================================================
void runMvcCalibration() {
  unsigned long elapsed = millis() - phase_start_ms;

  if (state == STATE_CALIB_MVC_READY) {
    if (elapsed >= MVC_GETREADY_MS) {
      enterState(STATE_CALIB_MVC_RECORD);
    }
  } else if (state == STATE_CALIB_MVC_RECORD) {
    if (smoothedValue > mvcCurrentRepPeak) mvcCurrentRepPeak = smoothedValue;
    if (elapsed >= MVC_REP_MS) {
      mvcPeaks[mvcRepIndex] = mvcCurrentRepPeak;
      Serial.print(F("Rep ")); Serial.print(mvcRepIndex + 1);
      Serial.print(F(" peak: ")); Serial.println(mvcCurrentRepPeak);
      mvcRepIndex++;
      enterState(STATE_CALIB_MVC_REST);
    }
  } else if (state == STATE_CALIB_MVC_REST) {
    if (elapsed >= MVC_REST_MS) {
      if (mvcRepIndex >= MVC_REPS) {
        finishMvcCalibration();
      } else {
        enterState(STATE_CALIB_MVC_READY);
      }
    }
  }
}

// Sorts the rep peaks and takes the MEDIAN (not max or mean) as calMaxFlex.
// This resists fatigue-skewed reps and one-off spikes. Then sanity-checks the
// result against the baseline: if it's too weak, calibration restarts (with the
// hand held open) instead of running with a nonsense range.
void finishMvcCalibration() {
  float sorted[MVC_REPS];
  memcpy(sorted, mvcPeaks, sizeof(mvcPeaks));
  for (int i = 1; i < MVC_REPS; i++) {
    float key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  float median = sorted[MVC_REPS / 2];

  Serial.print(F("Calibrated MVC (median): ")); Serial.println(median);

  if (median < calBaselineThreshold * MVC_MIN_RATIO) {
    Serial.println(F("MVC too weak vs baseline. Check electrode contact/placement."));
    Serial.println(F("Restarting calibration..."));
    enterState(STATE_CALIB_BASELINE);
    return;
  }

  calMaxFlex = median;
  runtimeMaxFlex = calMaxFlex;
  enterState(STATE_ACTIVE);
}

// ============================================================
// ACTIVE MODE: continuous proportional control (runs once per EMG sample)
// 1) Lower edge = runtime baseline + deadband, so resting noise = 0 motion.
// 2) Normalize smoothedValue between lower edge and runtime max (0..1).
// 3) Apply the power curve (pow(x, 0.35)) for extra low-end sensitivity.
// 4) Map straight to target_angle - continuous, not stepped.
// 5) Run background drift correction.
// ============================================================
void runActiveControl() {
  float lowerEdge = runtimeBaseline + calDeadband;
  float span = runtimeMaxFlex - lowerEdge;

  float normalized = 0.0;
  if (span > MIN_SPAN) {                       // avoids divide-by-zero / negative span
    normalized = (smoothedValue - lowerEdge) / span;
  }
  normalized = constrain(normalized, 0.0, 1.0);

  flexPercentage = (int)(pow(normalized, SENSITIVITY_CURVE) * 100.0);
  flexPercentage = constrain(flexPercentage, 0, 100);

  target_angle = (int)((flexPercentage / 100.0) * MAX_ANGLE);

  backgroundRecalibrate(normalized);
}

// Drift correction. `normalized` is the LINEAR activation (before the power
// curve), because the post-curve percentage barely gets near zero and made the
// old idle check almost never true.
//  - Baseline: nudged toward the signal only while confirmed idle, and clamped
//    to 0.3x..3x of the calibrated baseline.
//  - Max: jumps up immediately on a new peak (capped at MVC_CEILING_RATIO x the
//    calibrated MVC) and otherwise decays slowly, floored at MVC_FLOOR_RATIO x
//    the calibrated MVC so noise can't collapse the range.
void backgroundRecalibrate(float normalized) {
  // Peak capture is immediate every sample - it's a plain reassignment, not a small incremental
  // step, so it isn't subject to the precision issue the decay/drift steps below are.
  if (smoothedValue > runtimeMaxFlex) {
    float ceilingClamp = calMaxFlex * MVC_CEILING_RATIO;
    runtimeMaxFlex = (smoothedValue < ceilingClamp) ? smoothedValue : ceilingClamp;
    adaptSampleCounter = 0; // resync so decay doesn't immediately fight a fresh peak
    return;
  }

  adaptSampleCounter++;
  if (adaptSampleCounter < ADAPT_DECIMATION) return;
  adaptSampleCounter = 0;

  if (normalized < IDLE_FRACTION) {
    runtimeBaseline += BASELINE_ADAPT_ALPHA * (smoothedValue - runtimeBaseline);
    float lowClamp = calBaselineMean * 0.3;
    float highClamp = calBaselineMean * 3.0;
    runtimeBaseline = constrain(runtimeBaseline, lowClamp, highClamp);
  }

  runtimeMaxFlex -= MAXFLEX_DECAY_ALPHA * (runtimeMaxFlex - smoothedValue);
  float floorClamp = calMaxFlex * MVC_FLOOR_RATIO;
  if (runtimeMaxFlex < floorClamp) runtimeMaxFlex = floorClamp;
}

// ============================================================
// SERVO MOTION (gradual, shared by all states)
// Steps current_angle toward target_angle by 1 degree every
// SERVO_UPDATE_RATE ms, so calibration, active control and the return to the
// safe open position all move smoothly. Uses rollover-safe time comparison.
// ============================================================
void updateServos() {
  static unsigned long target_time = 0;
  if (current_angle != target_angle && (long)(millis() - target_time) >= 0) {
    target_time = millis() + SERVO_UPDATE_RATE;
    if (target_angle > current_angle) current_angle++;
    else current_angle--;
    writeAllServos(current_angle);
  }
}

// ============================================================
// MAIN LOOP
// sample EMG -> handle serial command -> run current state's logic ->
// step servos -> update LED -> periodic short log line.
// ============================================================
unsigned long next_log = 0;
const unsigned long LOG_RATE = 100; // ms between log lines
const bool ENABLE_LOG = true;       // set false for quietest/fastest operation

void loop() {
  newSample = false;
  sampleEMG();

  // Optional: send 'r' in the Serial Monitor to recalibrate without a power cycle
  if (Serial.available() && Serial.read() == 'r') {
    enterState(STATE_CALIB_BASELINE);
  }

  switch (state) {
    case STATE_CALIB_BASELINE:
      runBaselineCalibration();
      break;
    case STATE_CALIB_MVC_READY:
    case STATE_CALIB_MVC_RECORD:
    case STATE_CALIB_MVC_REST:
      runMvcCalibration();
      break;
    case STATE_ACTIVE:
      if (newSample) runActiveControl(); // pow() and adaptation only once per sample
      break;
  }

  updateServos();
  updateLed();

  // Compact log line (~45 chars, fits in the 64-byte Serial TX buffer so
  // print() doesn't block and cause dropped EMG samples).
  if (ENABLE_LOG && millis() - next_log >= LOG_RATE) {
    next_log = millis();
    Serial.print(F("S:")); Serial.print((int)state);
    Serial.print(F(" E:")); Serial.print((long)smoothedValue);
    Serial.print(F(" F:")); Serial.print(flexPercentage);
    Serial.print(F(" B:")); Serial.print((long)runtimeBaseline);
    Serial.print(F(" M:")); Serial.print((long)runtimeMaxFlex);
    Serial.print(F(" A:")); Serial.println(target_angle);
  }
}
