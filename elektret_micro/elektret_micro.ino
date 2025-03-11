

// pins used

#include <PinChangeInterrupt.h>

 
#define micro 12  // D12 logic inputs


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(micro, INPUT_PULLUP);

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) 
  {
    Serial.write(1); 
  }
}
