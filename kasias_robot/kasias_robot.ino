#include <Servo.h>

// pins used

int ur_pwm_mosfet = 6;
int lr_mosfet = 7;

int ul_pwm_mosfet = 5;
int ll_mosfet = 8;


int steer_out = 9;

Servo steering;
int speed_val = 0, direction_val = 0, steering_val = 0, relays_val = 0, connection = 0;



void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  steering.attach(steer_out);
  
  pinMode(ul_pwm_mosfet, OUTPUT);
  digitalWrite(ul_pwm_mosfet, LOW);
  pinMode(ur_pwm_mosfet, OUTPUT);
  digitalWrite(ur_pwm_mosfet, LOW);
  pinMode(ll_mosfet, OUTPUT);
  digitalWrite(ll_mosfet, LOW);
  pinMode(lr_mosfet, OUTPUT);
  digitalWrite(lr_mosfet, LOW);
  

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 3) 
  {
    int inByte = Serial.read();
    if (0xF0 < inByte < 0xF6)
    {
      switch (inByte)
      {
        case 0xF1:
          direction_val = 1;
          break;
        case 0xF2:
          direction_val = 0;
          break;
        case 0xF3:
          speed_val = 0;
          break;
        default:
          connection = 1;
      }
      speed_val = Serial.read();
      steering_val = Serial.read();
      relays_val = Serial.read();  
    }
    else
    {
        Serial.read();
    }
    steering_val = map(steering_val, 0x41, 0x6E, 0, 180);     // scale it for use with the servo (value between 0 and 180)
    steering.write(steering_val);
    if (speed_val == 0)
    {
      digitalWrite(ul_pwm_mosfet, LOW);
      digitalWrite(lr_mosfet, LOW);
      digitalWrite(ur_pwm_mosfet, LOW);
      digitalWrite(ll_mosfet, LOW);
      
    }
    else if (speed_val > 0 && direction_val == 0)
    {
      digitalWrite(ul_pwm_mosfet, LOW);
      digitalWrite(lr_mosfet, LOW);
      digitalWrite(ll_mosfet, HIGH);
      analogWrite(ur_pwm_mosfet, speed_val); 
    }
    else if (speed_val > 0 && direction_val == 1)
    {
      digitalWrite(ur_pwm_mosfet, LOW);
      digitalWrite(ll_mosfet, LOW);
      digitalWrite(lr_mosfet, HIGH);
      analogWrite(ul_pwm_mosfet, speed_val); 
    }
    //Serial.write(speed_val);
    //Serial.write(direction_val);
    //Serial.write(steering_val);
    //Serial.write(relays_val);
    //Serial.write(connection);
    
  }
}
