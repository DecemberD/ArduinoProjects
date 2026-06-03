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
#define STEP_STICK_N_RESET 7  // hw pull-down to keep in reset during arduino power-up sequence
#define STEP_STICK_STEP 8  
#define STEP_STICK_DIR 9  
#define BUZZER 12
 
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
float pressure_psi_setpoint = 10.0;
float pressure_psi_last = 0.0;
int16_t valve_position = 0;
int16_t initial_valve_position = 70;
uint8_t ctrl_override = 0;
// PID Control Parameters
float Kp = 1.0; // Proportional Gain
float Ki = 0.1; // Integral Gain
float Kd = 0.05; // Derivative Gain
int8_t buzzer_enable = 0;



void setup() {
  float f_value;
  uint8_t u_value;
  int16_t int_value;

  digitalWrite(STEP_STICK_N_ENABLE, 1);           // disable stepstick output
  digitalWrite(STEP_STICK_MS1, 1);                // 1/16 microSTEP
  digitalWrite(STEP_STICK_MS2, 1);                //
  digitalWrite(STEP_STICK_MS3, 1);                //
  digitalWrite(STEP_STICK_N_RESET, 0);            // keep stepstick in reset
  digitalWrite(STEP_STICK_STEP, 0);               // STEP_STICK_STEP Low
  digitalWrite(STEP_STICK_DIR, 0);                // dir to Close
  digitalWrite(BUZZER, 0);                        // Buzzer Off

  pinMode(STEP_STICK_N_ENABLE, OUTPUT);                 // outputs enable
  pinMode(STEP_STICK_MS1, OUTPUT);                      //
  pinMode(STEP_STICK_MS2, OUTPUT);                      //
  pinMode(STEP_STICK_MS3, OUTPUT);                      //
  pinMode(STEP_STICK_N_RESET, OUTPUT);                  //
  pinMode(STEP_STICK_STEP, OUTPUT);                     //
  pinMode(STEP_STICK_DIR, OUTPUT);                      //
  pinMode(BUZZER, OUTPUT);                              //
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
  if(!isnan(f_value)) pressure_psi_setpoint = f_value;
  EEPROM.get(sizeof(uint8_t)+2*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_normal = f_value;  
  EEPROM.get(sizeof(uint8_t)+3*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_low = f_value; 
  EEPROM.get(sizeof(uint8_t)+4*sizeof(float), f_value);
  if(!isnan(f_value)) Kp = f_value; 
  EEPROM.get(sizeof(uint8_t)+5*sizeof(float), f_value);
  if(!isnan(f_value)) Ki = f_value; 
  EEPROM.get(sizeof(uint8_t)+6*sizeof(float), int_value);
  if(!isnan(f_value)) initial_valve_position = int_value; 

  valve_turn(0);                                        // calibrate valve position
}



void loop() {
  uint8_t char_command;
  uint16_t pressure_adc_count = 0;
  uint16_t level_adc_count = 0;
  uint16_t level_adc_count_max = 0;
  uint16_t level_adc_count_min = 0;
  uint16_t level_adc_pp = 0;
  float level_capacity_nF = 0;
  float level_capacity_nF_change = 0;
  float pressure_psi = 0.0;
  float pressure_psi_avg = 0.0;
  float pressure_psi_diff = 0.0;
  int16_t pid_output = 0;


  if (Serial.available() > 0) {
      // get incoming byte:
      char_command = Serial.read();
      
      switch (char_command)
        {
          case 'q':
            pressure_psi_setpoint += 0.1;
            if(pressure_psi_setpoint >= 21.0)
              pressure_psi_setpoint = 21.0;
            Serial.print("pressure_psi_setpoint: ") ;
            Serial.println(pressure_psi_setpoint, 1) ;    // print float with 1 decimal place
            break;
          case 'a':
            pressure_psi_setpoint -= 0.1;
            if(pressure_psi_setpoint <= -0.1)
              pressure_psi_setpoint = 0.0;
            Serial.print("pressure_psi_setpoint: ") ;
            Serial.println(pressure_psi_setpoint, 1) ; 
            break;
          case 'e':
            level_capacity_nF_to_normal += 2.0;
            if(level_capacity_nF_to_normal >= 2000.0)
              level_capacity_nF_to_normal = 2000.0;
            Serial.print("level_capacity_nF_to_normal: ") ;
            Serial.println(level_capacity_nF_to_normal, 1) ;    // print float with 1 decimal place
            break;
          case 'd':
            level_capacity_nF_to_normal -= 2.0;
            if(level_capacity_nF_to_normal <= -2.0)
              level_capacity_nF_to_normal = 0.0;
            Serial.print("level_capacity_nF_to_normal: ") ;
            Serial.println(level_capacity_nF_to_normal, 1) ; 
            break;
          case 'r':
            level_capacity_nF_to_low += 2.0;
            if(level_capacity_nF_to_low >= 2000.0)
              level_capacity_nF_to_low = 2000.0;
            Serial.print("level_capacity_nF_to_low: ") ;
            Serial.println(level_capacity_nF_to_low, 1) ; 
            break;
          case 'f':
            level_capacity_nF_to_low -= 2.0;
            if(level_capacity_nF_to_low <= -2.0)
              level_capacity_nF_to_low = 0.0;
            Serial.print("level_capacity_nF_to_low: ") ;
            Serial.println(level_capacity_nF_to_low, 1) ; 
            break;
          case 't':
            Kp += 0.1;
            if(Kp >= 2.0)
              Kp = 2.0;
            Serial.print("Kp: ") ;
            Serial.println(Kp, 1) ; 
            break;
          case 'g':
            Kp -= 0.1;
            if(Kp <= 0.1)
              Kp = 0.1;
            Serial.print("Kp: ") ;
            Serial.println(Kp, 1) ; 
            break;
          case 'y':
            Ki += 0.01;
            if(Ki >= 1.0)
              Ki = 1.0;
            Serial.print("Ki: ") ;
            Serial.println(Ki, 2) ; 
            break;
          case 'h':
            Ki -= 0.01;
            if(Ki <= 0.01)
              Ki = 0.01;
            Serial.print("Ki: ") ;
            Serial.println(Ki, 2) ; 
            break;
          case 'i':
            initial_valve_position += 1;
            if(initial_valve_position >= 400)
              initial_valve_position = 400;
            Serial.print("initial_valve_position: ") ;
            Serial.println(initial_valve_position) ; 
            break;
          case 'k':
            initial_valve_position -= 1;
            if(initial_valve_position <= 0)
              initial_valve_position = 0;
            Serial.print("initial_valve_position: ") ;
            Serial.println(initial_valve_position) ; 
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
              buzzer_enable = 0;
              Serial.println("buzzer muted") ;
            }
            break;
          case 'z':
            database_store();
            Serial.println("data stored") ;
            break;
          case 'c':
            ctrl_override = ~(ctrl_override);
            break;
          case 'u':
            if(ctrl_override) valve_turn(1);
            break;
          case 'j':
            if(ctrl_override) valve_turn(-1);
            break;
          case 'x':
            valve_turn(0);
            break;
          default:
            break;
        }
  }

  // sample and average pressure  
  int i = 100;
  pressure_adc_count = analogRead(PRESSURE);
  //pressure_psi = map_float(pressure_adc_count, 478.952, 957.905, 0.0, 12.5); // reference 1.069V, 100psi range
  pressure_psi_avg = map_float((float)pressure_adc_count, 102.3, 920.7, 0, 30); // reference 5V, 30psi range
  while (i > 0)                                         
  {
    i -= 1;
    pressure_adc_count = analogRead(PRESSURE);
    //pressure_psi = map_float(pressure_adc_count, 478.952, 957.905, 0.0, 12.5); // reference 1.069V, 100psi range
    pressure_psi = map_float((float)pressure_adc_count, 102.3, 920.7, 0, 30); // reference 5V, 30psi range
    pressure_psi_avg =  (float)(pressure_psi_avg*99 +  pressure_psi)/100;
  }

  // valve control 
  pid_output = (int16_t)calculate_pid(pressure_psi_setpoint, pressure_psi_avg);
  if((pid_output - valve_position) != 0 && !ctrl_override)
  {
    valve_turn(pid_output - valve_position);
  }
  
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

  level_capacity_nF_change = level_capacity_nF_last - level_capacity_nF;
  if(level_capacity_nF_change > 0) level_capacity_nF_change = level_capacity_nF/level_capacity_nF_last;
  else level_capacity_nF_change = level_capacity_nF_last/level_capacity_nF;
  pressure_psi_diff = pressure_psi_last - pressure_psi_avg;
  pressure_psi_diff = abs(pressure_psi_diff);
  
  if(level_capacity_nF_change < 0.8 || pressure_psi_diff > 0.1) // print level_capacity_nF and pressure value only when changed 
  {
    Serial.print("level_capacity_nF: ") ;
    Serial.println(level_capacity_nF, 1) ; 
    Serial.print("Pressure: ") ;
    Serial.print(pressure_psi_avg, 2) ;
    Serial.print(",  ") ;
    Serial.println(pressure_adc_count) ;
    pressure_psi_last = pressure_psi_avg;                   // retain last printed pressure value for next loop  
    level_capacity_nF_last = level_capacity_nF;             // and last printed level_capacity_nF
  }

  if(level_capacity_nF < level_capacity_nF_to_low && !liquid_low_level)
  {
    Serial.println("Fluid Low Level") ; 
    liquid_low_level = 1;
    if(!buzzer_mute)
      buzzer_enable = 1;
  }
  if(level_capacity_nF > level_capacity_nF_to_normal && liquid_low_level)
  {
    liquid_low_level = 0;
    buzzer_enable = 0;
  }
  buzzer_modulate(buzzer_enable);
}
void valve_turn(int16_t steps) 
{
  //static int16_t position = 0;
  static uint8_t step_delay_ms = 2;
  if(steps == 0)
  {
    // digitalWrite(STEP_STICK_N_RESET, 0);            // Reset stepstick
    // digitalWrite(STEP_STICK_MS1, 0);                // FullSTEP
    // digitalWrite(STEP_STICK_MS2, 0);                //
    // digitalWrite(STEP_STICK_MS3, 0);                //
    // digitalWrite(STEP_STICK_N_RESET, 1);            // Release Reset stepstick
    // delay(1);                                       //
    // step_delay_ms = 32;                             // set step/ustep delay
    // valve_turn(-32);                                // turn to end position
    digitalWrite(STEP_STICK_N_RESET, 0);            // Reset stepstick                                      //
    digitalWrite(STEP_STICK_MS1, 1);                // 1/16 microSTEP
    digitalWrite(STEP_STICK_MS2, 1);                //
    digitalWrite(STEP_STICK_MS3, 1);                //
    digitalWrite(STEP_STICK_N_RESET, 1);            // Release Reset stepstick
    delay(1);                                       //
    step_delay_ms = 2;                              // set step/ustep delay
    valve_turn(-512);

    valve_turn(initial_valve_position);                                // turn to position 0
    valve_position = 0;
  }
  else
  {
    valve_position += steps;
    digitalWrite(STEP_STICK_N_ENABLE, 0);
    if(steps > 0) 
    {
      digitalWrite(STEP_STICK_DIR, 1);
      Serial.print("valve_position: ") ;
      Serial.println(valve_position) ;
      valve_open = 1;
    }
    else if(steps < 0) 
    {
      digitalWrite(STEP_STICK_DIR, 0);
      Serial.print("valve_position: ") ;
      Serial.println(valve_position) ;
      valve_open = 0;
    }
    for(uint16_t i=abs(steps); i>0; i--)
    {
      digitalWrite(STEP_STICK_STEP, 1);
      delay(step_delay_ms);
      digitalWrite(STEP_STICK_STEP, 0);
      delay(step_delay_ms);
    }
    digitalWrite(STEP_STICK_N_ENABLE, 1);
  }
}
float map_float(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
void database_store(void)
{
  EEPROM.put(0, buzzer_mute); 
  EEPROM.put(sizeof(uint8_t), pressure_psi_setpoint);
  EEPROM.put(sizeof(uint8_t)+2*sizeof(float), level_capacity_nF_to_normal);
  EEPROM.put(sizeof(uint8_t)+3*sizeof(float), level_capacity_nF_to_low);
  EEPROM.put(sizeof(uint8_t)+4*sizeof(float), Kp);
  EEPROM.put(sizeof(uint8_t)+5*sizeof(float), Ki);
  EEPROM.put(sizeof(uint8_t)+6*sizeof(float), initial_valve_position); 
}

// Function to calculate PID output
float calculate_pid(float setpoint, float measured_value) 
{
  static float previous_error = 0.0;
  static float integral = 0.0;
  static uint8_t output_clamped = 0;

  float error = -(setpoint - measured_value);
  if(!output_clamped)
  {
    integral += error; // Accumulate the integral of the error
  }
  float derivative = error - previous_error; // Change in error
  previous_error = error; // Store the current error for next derivative calculation

  // PID output
  float output = Kp * error + Ki * integral + Kd * derivative;

  // Clamp output to 0-150
  if (output <= 0.0)
  {
    output_clamped = 1;
    output = 0.0;
  }
  else if (output >= 150.0) 
  {
    output_clamped = 1;
    output = 150.0;
  }
  else output_clamped = 0;
  
  // Serial.print("error: ") ;
  // Serial.println(error, 1) ;
  // Serial.print("integral: ") ;
  // Serial.println(integral, 1) ;
  // Serial.print("derivative: ") ;
  // Serial.println(derivative, 1) ;
  // Serial.print("output: ") ;
  // Serial.println(output, 1) ;

  return output;
}

// Function for self-tuning gains
void self_tune(float measured_value, float setpoint) {
    float error = setpoint - measured_value;

    // Adjust PID parameters based on the error
    if (error > 1.0) {
        Kp += 0.01; // Increase proportional gain
        Ki += 0.001; // Increase integral gain
    } else if (error < -1.0) {
        Kp -= 0.01; // Decrease proportional gain
        Ki -= 0.001; // Decrease integral gain
    }

    // Clamp Kp and Ki to sensible limits
    if (Kp < 0) Kp = 0;
    if (Ki < 0) Ki = 0;
}
void buzzer_modulate(int8_t enable) {
  static unsigned long myTime = 0;
  static uint8_t state = 0;

  if(enable) 
  {
    if(millis() - myTime > 250)
    {
      state ^= 1;
      digitalWrite(BUZZER, state);
      myTime = millis();
    }
  }
  else
  {
    digitalWrite(BUZZER, 0);
  } 
}

