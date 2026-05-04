//  Heat Tunnel Sound Detector Audio Filter
//  M. Dec
//  16.04.2026
//  ESP32-S3-WROOM-1U platform
//  Performs sampling and filtering of audio signal using Groetzel algorithm
//  Input Band: 0-8kHz,  Filter Passband: 3.0-3.5kHz (13 x 40Hz subBands)  

#include <Arduino.h>

#define ADC_PIN 1
#define GATE_PIN 2
#define TP1_PIN 15
#define TP2_PIN 16
#define SAMPLE_RATE 16000
#define N 400
#define NUM_FREQ 13
#define DET_TRESHOLD  10

float targetFreqs[NUM_FREQ] = { 3000, 3040, 3080, 3120, 3160, 3200, 3240, 3280, 3320, 3360, 3400, 3440, 3480 };
float magnitude[NUM_FREQ] = { 0 };
float mag_normalized[NUM_FREQ] = { 0 };
float totalRMS = 0; // Root MEan Square of total signal



// Goertzel coefficients
float coeff[NUM_FREQ];
float sine[NUM_FREQ];
float cosine[NUM_FREQ];


int samples[N];
int sampleIndex = 0;
volatile bool bufferReady = false;

hw_timer_t *timer = NULL;
// 🔧 Timer ISR (FAST, SAFE)
void IRAM_ATTR onTimer() {
  if (bufferReady) return;                      // terminate till buffer empty
  samples[sampleIndex++] = analogRead(ADC_PIN);
  if (sampleIndex >= N) {
    sampleIndex = 0;
    bufferReady = true;
  }
}

// 🔧 Initialize Goertzel
void initGoertzel() {
  for (int i = 0; i < NUM_FREQ; i++) {
    float omega = 2.0 * PI * targetFreqs[i] / SAMPLE_RATE;
    sine[i] = sin(omega);
    cosine[i] = cos(omega);
    coeff[i] = 2.0 * cosine[i];
  }
}

// 🔧 Setup timer (ESP32 core v3.x)
void setupTimer() {
  timer = timerBegin(SAMPLE_RATE);  // 16 kHz base
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1, true, 0);  // interrupt every tick
}

void present(void) {
  Serial.print("Total RMS: ");
  Serial.println(totalRMS);
  for (int f = 0; f < NUM_FREQ; f++) {
    Serial.print(targetFreqs[f]);
    Serial.print(" Hz: abs ");
    Serial.print(magnitude[f]);
    Serial.print(" norm ");
    Serial.println(mag_normalized[f]);
  }
  Serial.print("Alarm detected: ");
  Serial.println(digitalRead(GATE_PIN));
  Serial.println("----");
}

void setup() {
  Serial.begin(115200);
  pinMode(GATE_PIN, OUTPUT);
  //pinMode(TP1_PIN, OUTPUT);
  //pinMode(TP2_PIN, OUTPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  initGoertzel();
  setupTimer();
  Serial.print("Microphone Digital Filter");
}

void loop() {


  // 🧠 Process Goertzel
  if (bufferReady) {
    float q1[NUM_FREQ] = { 0 };
    float q2[NUM_FREQ] = { 0 };
    float totalEnergy = 0;

    for (int n = 0; n < N; n++) {
      float x = samples[n] - 2048;  // remove DC offset
      totalEnergy += x * x;
      for (int f = 0; f < NUM_FREQ; f++) {
        float q0 = coeff[f] * q1[f] - q2[f] + x;
        q2[f] = q1[f];
        q1[f] = q0;
      }
    }
    totalRMS = sqrt(totalEnergy);

    // 📊 Compute magnitude
    for (int f = 0; f < NUM_FREQ; f++) {
      float real = q1[f] - q2[f] * cosine[f];
      float imag = q2[f] * sine[f];
      magnitude[f] = sqrt(real * real + imag * imag);
      mag_normalized[f] = magnitude[f] / totalRMS;
    }

    // 📊 Detect signal within band
    float adj_norm_mag_sum = 0;
    for (int f = 0; f < NUM_FREQ - 1; f++) {
      adj_norm_mag_sum = max(adj_norm_mag_sum, mag_normalized[f] + mag_normalized[f + 1]);
    }
    static long gate_hold_on_time = millis();
    static bool gate_hold_on = false;
    if (adj_norm_mag_sum > DET_TRESHOLD) {      // detected when sum of normalized magnitude of two adjacent sub-bands > treshold
      digitalWrite(GATE_PIN, HIGH);
      gate_hold_on_time = millis();
      gate_hold_on = true;
    } else {
      gate_hold_on = false;
    }
    if ((millis() - gate_hold_on_time > 5000) && gate_hold_on == false) { // hold output for 1 sec
      digitalWrite(GATE_PIN, LOW); 
    }

    bufferReady = false;
  }

  static long ms = millis();
  if (millis() - ms > 400) {
    ms = millis();
    present();
  }
}