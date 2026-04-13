#include <avr/wdt.h>
const byte LED_PIN = 13;

int cmd;

unsigned long timeNow = 0;

void setup() {
    MCUSR = 0; // clear 
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    noInterrupts();
    wdt_reset();
    /* Start timed equence */
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    /* Set new prescaler(time-out) value = 1024K (1048576) cycles 8.0s */
    WDTCSR = (1<<WDE) | (1<<WDP3) | (1<<WDP0);
    interrupts();
}
 
void loop() {

    if (millis() - timeNow >= 500) {
        Serial.println(timeNow);
        timeNow = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        wdt_reset();
    }
  
    if (Serial.available() > 0) {
        cmd = Serial.read();
        
        if ((cmd == 'q') || (cmd == 'Q')) {
            delay(9000);
        }
    }
}
  