/*
Project name: Pressure Releave Valve control
Date: 12.03.2025
Author: Marcin Dec
*/

#include <avr/sleep.h>    // Sleep Modes
#include <EEPROM.h>

// pins definition
#define STEP_STICK_N_ENABLE 2
#define STEP_STICK_MS1 3  
#define STEP_STICK_MS2 4  
#define STEP_STICK_MS3 5 
#define STEP_STICK_STEP 0  
#define STEP_STICK_DIR 1  
#define BUZZER 4
 
#define PRESSURE A0
#define LEVEL A1
// constant definitions
#define MOTOR_STEPS_TO_OPEN_CLOSE 255

uint8_t valve_open = 0;
uint8_t liquid_low_level = 0;
uint8_t buzzer_mute = 0;
float level_capacity_nF_to_normal = 400.0;
float level_capacity_nF_to_low = 10.0;
float level_capacity_nF_last = 0;
float pressure_psi_to_close = 10.0;
float pressure_psi_to_open = 15.0;
float pressure_psi_last = 0.0;



void setup() {
  float f_value;
  uint8_t u_value;


  PORTD &= 0b00000000;                                  // set all PORTD pins to 0
  PORTB &= 0b00000000;                                  // set all PORTB pins to 0
  PORTD |= 0b00000001 << STEP_STICK_N_ENABLE;           // disable stepstick output
  PORTD |= 0b00000001 << STEP_STICK_MS1;                // 1/16 microSTEP
  PORTD |= 0b00000001 << STEP_STICK_MS2;                //
  PORTD |= 0b00000001 << STEP_STICK_MS3;                //
  PORTB &= ~(0b00000001 << STEP_STICK_STEP);            // STEP_STICK_STEP Low
  PORTB &= ~(0b00000001 << STEP_STICK_DIR);             // dir to Close
  PORTB &= ~(0b00000001 << BUZZER);                     // Buzzer Off

  pinMode(STEP_STICK_N_ENABLE, OUTPUT);                 // outputs enable
  pinMode(STEP_STICK_MS1, OUTPUT);                      //
  pinMode(STEP_STICK_MS2, OUTPUT);                      //
  pinMode(STEP_STICK_MS3, OUTPUT);                      //
  pinMode(STEP_STICK_STEP+8, OUTPUT);                   //
  pinMode(STEP_STICK_DIR+8, OUTPUT);                    //
  pinMode(BUZZER+8, OUTPUT);                            //
  // Set Pin D6 (OC0A) as output
  pinMode(6, OUTPUT);                                   //

  // Serial init
  Serial.begin(57600);          
  // analog reference for ADC 5.0V                       
  analogReference(DEFAULT);

  // Configure Timer0 in Fast PWM mode with a prescaler of 64
    TCCR0A = (1 << WGM01) | (1 << WGM00);               // Fast PWM mode
    TCCR0A |= (1 << COM0A1);                            // Clear OC0A on Compare Match, set at BOTTOM
    TCCR0B = (1 << CS01) | (1 << CS00);                 // Prescaler 64
    OCR0A = 127;                                        // 50% duty cycle of 255 (8-bit resolution)

  // get nonvolatile values from databse unless database is empty
  EEPROM.get(0, u_value);
  if(u_value != 0xFF) buzzer_mute = u_value;
  EEPROM.get(sizeof(uint8_t), f_value);
  if(!isnan(f_value)) pressure_psi_to_close = f_value;
  EEPROM.get(sizeof(uint8_t)+sizeof(float), f_value);
  if(!isnan(f_value)) pressure_psi_to_open = f_value;  
  EEPROM.get(sizeof(uint8_t)+2*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_normal = f_value;  
  EEPROM.get(sizeof(uint8_t)+3*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_low = f_value; 

  valve_turn(0);                                        // fully close valve upon power-up to calibrate valve position
}



void loop() {
  uint8_t char_command;
  uint16_t pressure_adc_count = 0;
  uint16_t level_adc_count = 0;
  uint16_t level_adc_count_max = 0;
  uint16_t level_adc_count_min = 0;
  uint16_t level_adc_pp = 0;
  float level_capacity_nF = 0;
  float pressure_psi = 0.0;
  float pressure_psi_avg = 0.0;

  static int16_t calval = 520;

  if (Serial.available() > 0) {
      // get incoming byte:
      char_command = Serial.read();
      
      switch (char_command)
        {
          case 'q':
            pressure_psi_to_close += 0.1;
            if(pressure_psi_to_close >= 21.0)
              pressure_psi_to_close = 21.0;
            Serial.print("pressure_psi_to_close: ") ;
            Serial.println(pressure_psi_to_close, 1) ;    // print float with 1 decimal place
            break;
          case 'a':
            pressure_psi_to_close -= 0.1;
            if(pressure_psi_to_close <= -0.1)
              pressure_psi_to_close = 0.0;
            Serial.print("pressure_psi_to_close: ") ;
            Serial.println(pressure_psi_to_close, 1) ; 
            break;
          case 'w':
            pressure_psi_to_open += 0.1;
            if(pressure_psi_to_open >= 21.0)
              pressure_psi_to_open = 21.0;
            Serial.print("pressure_psi_to_open: ") ;
            Serial.println(pressure_psi_to_open, 1) ; 
            break;
          case 's':
            pressure_psi_to_open -= 0.1;
            if(pressure_psi_to_open <= -0.1)
              pressure_psi_to_open = 0.0;
            Serial.print("pressure_psi_to_open: ") ;
            Serial.println(pressure_psi_to_open, 1) ; 
            break;
          case 'e':
            level_capacity_nF_to_normal += 10.0;
            if(level_capacity_nF_to_normal >= 2000.0)
              level_capacity_nF_to_normal = 2000.0;
            Serial.print("level_capacity_nF_to_normal: ") ;
            Serial.println(level_capacity_nF_to_normal, 1) ;    // print float with 1 decimal place
            break;
          case 'd':
            level_capacity_nF_to_normal -= 10.0;
            if(level_capacity_nF_to_normal <= -10.0)
              level_capacity_nF_to_normal = 0.0;
            Serial.print("level_capacity_nF_to_normal: ") ;
            Serial.println(level_capacity_nF_to_normal, 1) ; 
            break;
          case 'r':
            level_capacity_nF_to_low += 10.0;
            if(level_capacity_nF_to_low >= 2000.0)
              level_capacity_nF_to_low = 2000.0;
            Serial.print("level_capacity_nF_to_low: ") ;
            Serial.println(level_capacity_nF_to_low, 1) ; 
            break;
          case 'f':
            level_capacity_nF_to_low -= 10.0;
            if(level_capacity_nF_to_low <= -10.0)
              level_capacity_nF_to_low = 0.0;
            Serial.print("level_capacity_nF_to_low: ") ;
            Serial.println(level_capacity_nF_to_low, 1) ; 
            break;
          case 'm':
            if(buzzer_mute) 
            {
              buzzer_mute = 0;
              Serial.println("buzzer unmuted") ;
            }
            else
            {
              buzzer_mute = 1;
              PORTB &= ~(0b00000001 << BUZZER);
              Serial.println("buzzer muted") ;
            }
            break;
          case 'z':
            database_store();
            Serial.println("data stored") ;
            break;
          case 'o':
            valve_turn(calval);
            valve_turn(-calval);
            calval += 10;
            break;
          case 'c':
            calval -= 10;
            valve_turn(calval);
            valve_turn(-calval);
            break;
          case 't':
            valve_turn(1);
            break;
          case 'g':
            valve_turn(-1);
            break;
          default:
            break;
        }
  }

  // sample and average pressure  
  int i = 100;
  while (i > 0)                                         
  {
    i -= 1;
    pressure_adc_count = analogRead(PRESSURE);
    //pressure_psi = map_float(pressure_adc_count, 478.952, 957.905, 0.0, 12.5); // reference 1.069V, 100psi range
    pressure_psi = map_float((float)pressure_adc_count, 101.0698, 910.67, 0, 30); // reference 5V, 30psi range
    pressure_psi_avg =  (float)(pressure_psi_avg*99 +  pressure_psi)/100;
  }

  // valve control 
  // if(pressure_psi_avg > pressure_psi_to_open && !valve_open )
  // {
  //   valve_turn(1, MOTOR_STEPS_TO_OPEN_CLOSE);  // valve open
  // }
  // else if(pressure_psi_avg < pressure_psi_to_close && valve_open)
  // {
  //   valve_turn(0, MOTOR_STEPS_TO_OPEN_CLOSE);  // valve close
  // }

  // sample and get peak to peak  voltage on "liquid" cappacitor
  i = 100;
  level_adc_count_max = 0;
  level_adc_count_min = 1023;
  while (i > 0)
  {
    i -= 1;
    level_adc_count = analogRead(LEVEL);
    if(level_adc_count > level_adc_count_max) level_adc_count_max = level_adc_count;
    if(level_adc_count < level_adc_count_min) level_adc_count_min = level_adc_count;
  }

  level_adc_pp = level_adc_count_max - level_adc_count_min;
  //     sqrt(sq(Us) - sq(Uc))
  // C = ---------------------
  //       2 * Pi * f * R * Uc
  level_capacity_nF = 2000*(float)(sqrt(1048575 - (float)(level_adc_pp) * (float)(level_adc_pp)))/(double)(62.8*level_adc_pp);

  level_capacity_nF_last -= level_capacity_nF;
  level_capacity_nF_last = abs(level_capacity_nF_last);
  pressure_psi_last -= pressure_psi_avg;
  pressure_psi_last = abs(pressure_psi_last);
  if(level_capacity_nF_last > 1 or pressure_psi_last > 0.1) // print level_capacity_nF and pressure value only when changed 
  {
    Serial.print("level_capacity_nF: ") ;
    Serial.println(level_capacity_nF, 1) ; 
    Serial.print("Pressure: ") ;
    Serial.println(pressure_psi_avg, 1) ;
  }


  if(level_capacity_nF < level_capacity_nF_to_low && !liquid_low_level)
  {
    Serial.println("Fluid Low Level") ; 
    liquid_low_level = 1;
    if(!buzzer_mute)
      PORTB |= 0b00000001 << BUZZER;
  }
  if(level_capacity_nF > level_capacity_nF_to_normal && liquid_low_level)
  {
    liquid_low_level = 0;
    PORTB &= ~(0b00000001 << BUZZER);
  }
  pressure_psi_last = pressure_psi_avg;                   // retain last pressure value for next loop  
  level_capacity_nF_last = level_capacity_nF;
}
void valve_turn(int16_t steps) 
{
  static int16_t position = 0;
  if(steps == 0)
  {
    position = 0;
    steps = - 800;
  }
  else
  {
    position += steps;
  }

  PORTD &= ~(0b00000001 << STEP_STICK_N_ENABLE);
  if(steps > 0) 
  {
    PORTB |= 0b00000001 << STEP_STICK_DIR;
    Serial.print("position: ") ;
    Serial.println(position) ;
    //valve_open = 1;
  }
  else if(steps < 0) 
  {
    PORTB &= ~(0b00000001 << STEP_STICK_DIR);
    Serial.print("position: ") ;
    Serial.println(position) ;
    //valve_open = 0;
  }
  for(uint16_t i=abs(steps); i>0; i--)
  {
    PORTB |= 0b00000001 << STEP_STICK_STEP;
    delay(2);
    PORTB &= ~(0b00000001 << STEP_STICK_STEP);
    delay(2);
  }
  PORTD |= 0b00000001 << STEP_STICK_N_ENABLE;

}
float map_float(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
void database_store(void)
{
  EEPROM.put(0, buzzer_mute); 
  EEPROM.put(sizeof(uint8_t), pressure_psi_to_close);
  EEPROM.put(sizeof(uint8_t)+sizeof(float), pressure_psi_to_open);
  EEPROM.put(sizeof(uint8_t)+2*sizeof(float), level_capacity_nF_to_normal);
  EEPROM.put(sizeof(uint8_t)+3*sizeof(float), level_capacity_nF_to_low);
}
