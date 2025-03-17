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
#define STEP_STICK_N_RESET 7
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
float pressure_psi_to_close = 10.0;
float pressure_psi_to_open = 15.0;
float pressure_psi_setpoint = 3.0;
float pressure_psi_last = 0.0;
int16_t valve_position = 0;
// PID Control Parameters
float Kp = 1.0; // Proportional Gain
float Ki = 0.1; // Integral Gain
float Kd = 0.05; // Derivative Gain



void setup() {
  float f_value;
  uint8_t u_value;

  digitalWrite(STEP_STICK_N_ENABLE, 1);           // disable stepstick output
  digitalWrite(STEP_STICK_MS1, 0);                // 1/16 microSTEP
  digitalWrite(STEP_STICK_MS2, 0);                //
  digitalWrite(STEP_STICK_MS3, 0);                //
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
  if(!isnan(f_value)) pressure_psi_to_close = f_value;
  EEPROM.get(sizeof(uint8_t)+sizeof(float), f_value);
  if(!isnan(f_value)) pressure_psi_to_open = f_value;  
  EEPROM.get(sizeof(uint8_t)+2*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_normal = f_value;  
  EEPROM.get(sizeof(uint8_t)+3*sizeof(float), f_value);
  if(!isnan(f_value)) level_capacity_nF_to_low = f_value; 

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
  float pressure_psi = 0.0;
  float pressure_psi_avg = 0.0;
  int16_t pid_output = 0;


  static int16_t calval = 26;

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
              digitalWrite(BUZZER, 0);
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
          case 'y':
            valve_turn(0);
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
  pid_output = (int16_t)calculate_pid(pressure_psi_setpoint, pressure_psi_avg);
  if((pid_output - valve_position) != 0)
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
    Serial.println("Fluid m Level") ; 
    liquid_low_level = 1;
    if(!buzzer_mute)
      digitalWrite(BUZZER, 1);
  }
  if(level_capacity_nF > level_capacity_nF_to_normal && liquid_low_level)
  {
    liquid_low_level = 0;
    digitalWrite(BUZZER, 0);
  }
  pressure_psi_last = pressure_psi_avg;                   // retain last pressure value for next loop  
  level_capacity_nF_last = level_capacity_nF;
}
void valve_turn(int16_t steps) 
{
  //static int16_t position = 0;
  static uint8_t step_delay_ms = 2;
  if(steps == 0)
  {
    digitalWrite(STEP_STICK_N_RESET, 0);            // Reset stepstick
    digitalWrite(STEP_STICK_MS1, 0);                // FullSTEP
    digitalWrite(STEP_STICK_MS2, 0);                //
    digitalWrite(STEP_STICK_MS3, 0);                //
    digitalWrite(STEP_STICK_N_RESET, 1);            // Release Reset stepstick
    delay(1);                                       //
    step_delay_ms = 32;                             // set step/ustep delay
    valve_turn(-32);                                // turn to end position
    digitalWrite(STEP_STICK_N_RESET, 0);            // Reset stepstick                                      //
    digitalWrite(STEP_STICK_MS1, 1);                // 1/16 microSTEP
    digitalWrite(STEP_STICK_MS2, 1);                //
    digitalWrite(STEP_STICK_MS3, 1);                //
    digitalWrite(STEP_STICK_N_RESET, 1);            // Release Reset stepstick
    delay(1);                                       //
    step_delay_ms = 2;                              // set step/ustep delay
    valve_turn(120);                                // turn to position 0
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
      //valve_open = 1;
    }
    else if(steps < 0) 
    {
      digitalWrite(STEP_STICK_DIR, 0);
      Serial.print("valve_position: ") ;
      Serial.println(valve_position) ;
      //valve_open = 0;
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
  EEPROM.put(sizeof(uint8_t), pressure_psi_to_close);
  EEPROM.put(sizeof(uint8_t)+sizeof(float), pressure_psi_to_open);
  EEPROM.put(sizeof(uint8_t)+2*sizeof(float), level_capacity_nF_to_normal);
  EEPROM.put(sizeof(uint8_t)+3*sizeof(float), level_capacity_nF_to_low);
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

  // Clamp output to 0-100%
  if (output <= 0.0)
  {
    output_clamped = 1;
    output = 0.0;
  }
  else if (output >= 200.0) 
  {
    output_clamped = 1;
    output = 200.0;
  }
  else output_clamped = 0;
  
  Serial.print("error: ") ;
  Serial.println(error, 1) ;
  Serial.print("integral: ") ;
  Serial.println(integral, 1) ;
  Serial.print("derivative: ") ;
  Serial.println(derivative, 1) ;
  Serial.print("output: ") ;
  Serial.println(output, 1) ;

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

// Main function
// int main() {
//     float setpoint, measured_value, output;

//     // Example setpoint and measured value
//     setpoint = 20.0; // Desired pressure (psi)
    
//     // Simulate measurement (replace this with actual sensor readings)
//     srand(time(0));
//     measured_value = rand() % 31; // Simulated pressure between 0 and 30 psi

//     // Update and calculate PID output
//     output = calculate_pid(setpoint, measured_value);

//     // Self-tune PID parameters
//     self_tune(measured_value, setpoint);

//     // Display results
//     printf("Setpoint: %.1f psi\n", setpoint);
//     printf("Measured Value: %.1f psi\n", measured_value);
//     printf("PID Output: %.1f%%\n", output);
//     printf("Kp: %.2f, Ki: %.4f, Kd: %.2f\n", Kp, Ki, Kd);

//     return 0;
// }
