

uint8_t useconds = 0;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  #define INA 2
  #define ENBA 3  

  PORTD &= 0b00000000;  // set all PORTD pins to 0
  PORTD |= 0b00000001 << INA; // set INA High
  PORTD |= 0b00000001 << ENBA; // enable ENBA
  pinMode(ENBA, OUTPUT);   // PORTD PIN2 set as output
  pinMode(INA, OUTPUT);   // PORTD PIN3 set as output

  Serial.print("pulse time =  ") ;
  Serial.print(useconds) ;
  Serial.print("\n") ;
  Serial.print("Hit u to increase or d to decrease\n") ;

}



void loop() {

// put your main code here, to run repeatedly:
  uint8_t inByte;
  

  if (Serial.available() > 0) {
      // get incoming byte:
      inByte = Serial.read();
      
      switch (inByte)
        {
          case 'u':
            useconds += 1;
            if(useconds == 255)
              useconds = 255;
            break;
          case 'd':
            useconds -= 1;
            if(useconds == 255)
              useconds = 0;
            break;
          
          default:
            break;
        }
  
  Serial.print("pulse time =  ") ;
  Serial.print(useconds) ;
  Serial.print("\n") ;
  Serial.print("Hit u to increase or d to decrease\n") ;
  }
  if (useconds == 0)
  {
    PORTD &= ~(0b00000001 << INA); // set INA Low
    PORTD |= 0b00000001 << INA;// set INA High
  
  }
  else
  {
    PORTD &= ~(0b00000001 << INA); // set INA Low
    delayMicroseconds(useconds);    
    PORTD |= 0b00000001 << INA; // set INA High
  }
      
  delay(1000);
}
