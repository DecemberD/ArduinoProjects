#include <Arduino.h>

#define ADC_PIN 4
#define SAMPLE_RATE 16000
#define N 200

#define NUM_FREQ 3
float targetFreqs[NUM_FREQ] = {1000, 2000, 3000};

// Goertzel coefficients
float coeff[NUM_FREQ];
float sine[NUM_FREQ];
float cosine[NUM_FREQ];

// Sampling
volatile int sampleTicks = 0;

int samples[N];
int sampleIndex = 0;
bool bufferReady = false;

hw_timer_t *timer = NULL;

// 🔧 Timer ISR (FAST, SAFE)
void IRAM_ATTR onTimer() {
  sampleTicks++;
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
  timer = timerBegin(SAMPLE_RATE);     // 16 kHz base
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1, true, 0);       // interrupt every tick
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  initGoertzel();
  setupTimer();
}

void loop() {

  // 🔁 Handle sampling (no data loss)
  while (sampleTicks > 0) {
    sampleTicks--;

    samples[sampleIndex++] = analogRead(ADC_PIN);

    if (sampleIndex >= N) {
      sampleIndex = 0;
      bufferReady = true;
    }
  }

  // 🧠 Process Goertzel
  if (bufferReady) {
    bufferReady = false;

    float q1[NUM_FREQ] = {0};
    float q2[NUM_FREQ] = {0};

    for (int n = 0; n < N; n++) {
      float x = samples[n] - 2048; // remove DC offset

      for (int f = 0; f < NUM_FREQ; f++) {
        float q0 = coeff[f] * q1[f] - q2[f] + x;
        q2[f] = q1[f];
        q1[f] = q0;
      }
    }

    // 📊 Compute magnitude
    for (int f = 0; f < NUM_FREQ; f++) {
      float real = q1[f] - q2[f] * cosine[f];
      float imag = q2[f] * sine[f];
      float magnitude = sqrt(real * real + imag * imag);

      Serial.print(targetFreqs[f]);
      Serial.print(" Hz: ");
      Serial.println(magnitude);

      if (magnitude > 10000) {   // tune this!
        Serial.print(">>> DETECTED: ");
        Serial.println(targetFreqs[f]);
      }
    }

    Serial.println("----");
  }
}