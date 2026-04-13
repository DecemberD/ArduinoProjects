#include <avr/wdt.h>//włączamy bibliotekę do programu

const int nr_pinu = 13;  //definicja numer pinu do którego podłączona jest dioda
int stan_diody = LOW;  //definicja zmiennej stanu diody (HIGH-świeci, LOW-nie świeci)
unsigned long ostatni_czas = 0;  //definicja zmiennej pomocniczej, do której przypisana będzie ostatnia wartość czasu zmiany staniu diody
const long okres_czasu = 1000;  //definicja okresu migania diody (500 to znaczy 0,5s)

void setup() {
  uint8_t mcusr = MCUSR;
  MCUSR = 0;
  pinMode(nr_pinu, OUTPUT);
  digitalWrite(nr_pinu, LOW);
  ostatni_czas  = millis(); // przypisanie aktualnej wartości czasu do zmiennej
  // Serial init
  Serial.begin(115200); 
  Serial.print("INIT ");
  Serial.println(mcusr, HEX);
  wdt_enable(WDTO_250MS);//od tego miejsca watchdog sprawdza co 0,25 sekundy czy arduino nie zawiesiło się
}

void loop() {
  uint8_t char_command;
  if (Serial.available() > 0) {
      // get incoming byte:
      char_command = Serial.read();
      
      switch (char_command)
        {
          case 'q':
            delay(1000);
            break;
          default:
            break;
        }
  }
  if (millis() - ostatni_czas  >= okres_czasu) 
  {
    Serial.print("LED toggle\n\n");
    stan_diody =!stan_diody ;//zmiana stanu zmiennej diody na przeciwny
    digitalWrite(nr_pinu, stan_diody);
    ostatni_czas  = millis(); // przypisanie aktualnej wartości czasu do zmiennej
  }
  wdt_reset(); //reset zegara watchdog
}