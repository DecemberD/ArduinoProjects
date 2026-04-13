#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 9600
#define UBRR_VALUE ((F_CPU/16/BAUD)-1)

// Pulse timing (microseconds)
#define HIGH_US      10
#define DECAY_US     0

// distance_cm = (time_us * 343) / 20000
static inline uint16_t time_to_cm(uint16_t time_us)
{
    return (uint32_t)time_us * 343UL / 20000UL;
}

volatile uint16_t echo_time_us = 0;
volatile uint8_t measurement_done = 0;
volatile uint8_t timeout_flag = 0;

volatile uint8_t state = 0;
// 0 = idle
// 1 = transmit pulse sequence
// 2 = listen for echo

volatile uint8_t pulse_phase = 0;
// 0 = high
// 1 = decay-low
// 2 = switch to listen

// ---------------- UART ----------------
void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_tx(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void uart_print(const char *s)
{
    while (*s)
        uart_tx(*s++);
}

void uart_print_uint(uint16_t v)
{
    char buf[6];
    uint8_t i = 0;

    if (v == 0)
    {
        uart_tx('0');
        return;
    }

    while (v > 0 && i < sizeof(buf) - 1)
    {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }

    while (i > 0)
        uart_tx(buf[--i]);
}

// ---------------- Timer0 1ms tick ----------------
volatile uint16_t ms_counter = 0;

ISR(TIMER0_COMPA_vect)
{
    ms_counter++;
}

void timer0_init_1ms(void)
{
    // CTC mode
    TCCR0A = (1 << WGM01);

    // 8MHz/64 = 125kHz, 1ms = 125 ticks => OCR0A = 124
    OCR0A = 124;

    // prescaler 64
    TCCR0B = (1 << CS01) | (1 << CS00);

    // enable interrupt
    TIMSK0 = (1 << OCIE0A);
}

// ---------------- Timer1 Compare ISR (pulse sequencing) ----------------
ISR(TIMER1_COMPA_vect)
{
    if (state == 1)
    {
        if (pulse_phase == 0)
        {
            // HIGH phase: PD6=1 PD7=0
            PORTD |=  (1 << PORTD6);
            PORTD &= ~(1 << PORTD7);

            OCR1A = HIGH_US;
            TCNT1 = 0;
            pulse_phase = 1;
        }
        else if (pulse_phase == 1)
        {
            // DECAY LOW: PD6=0 PD7=0
            PORTD &= ~(1 << PORTD6);
            PORTD &= ~(1 << PORTD7);

            OCR1A = DECAY_US;
            TCNT1 = 0;
            pulse_phase = 2;
        }
        else if (pulse_phase == 2)
        {
            // Stop compare interrupt (pulse done)
            TIMSK1 &= ~(1 << OCIE1A);

            // Switch PD6/PD7 to input, no pullups
            DDRD &= ~(1 << DDD6);
            DDRD &= ~(1 << DDD7);
            PORTD &= ~(1 << PORTD6);
            PORTD &= ~(1 << PORTD7);
            uart_print("inp_no_pups");
            uart_print("\n");


            // Disable digital input buffers on AIN0/AIN1
            DIDR1 |= (1 << AIN0D) | (1 << AIN1D);

            // Enable analog comparator interrupt on falling edge
            // ACIS1:0 = 10 => falling edge
            ACSR = (1 << ACIE) | (1 << ACIS1);

            // Start Timer1 free-run at 1MHz (1us per tick)
            TCNT1 = 0;
            TCCR1A = 0;
            TCCR1B = (1 << CS11); // prescaler 8

            // Enable overflow timeout (~65ms)
            TIFR1 |= (1 << TOV1);
            TIMSK1 |= (1 << TOIE1);

            timeout_flag = 0;
            state = 2;

            pulse_phase = 0;
        }
    }
}

// ---------------- Analog Comparator ISR ----------------
ISR(ANALOG_COMP_vect)
{
    if (state == 2)
    {
        echo_time_us = TCNT1;

        // Stop timer
        TCCR1B = 0;

        // Disable overflow interrupt
        TIMSK1 &= ~(1 << TOIE1);

        // Disable comparator
        ACSR &= ~(1 << ACIE);
        ACSR |= (1 << ACD);
        //uart_print_uint(echo_time_us);
        //uart_print("\n");

        timeout_flag = 0;
        measurement_done = 1;
        state = 0;
    }
}

// ---------------- Timer1 Overflow ISR (Timeout) ----------------
ISR(TIMER1_OVF_vect)
{
    if (state == 2)
    {
        // Stop timer
        TCCR1B = 0;

        // Disable overflow interrupt
        TIMSK1 &= ~(1 << TOIE1);

        // Disable comparator
        ACSR &= ~(1 << ACIE);
        ACSR |= (1 << ACD);

        timeout_flag = 1;
        measurement_done = 1;
        state = 0;
    }
}

// ---------------- Start transmit sequence ----------------
void start_pulse_sequence(void)
{
    // PD6/PD7 outputs
    DDRD |= (1 << DDD6) | (1 << DDD7);

    // start low
    PORTD &= ~(1 << PORTD6);
    PORTD &= ~(1 << PORTD7);

    pulse_phase = 0;
    state = 1;

    // Timer1 CTC mode, prescaler 8 => 1MHz => 1us tick
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11);

    OCR1A = 1;   // immediate ISR entry
    TCNT1 = 0;

    TIMSK1 |= (1 << OCIE1A);
}

int main(void)
{
    uart_init();
    timer0_init_1ms();

    // Disable comparator initially
    ACSR |= (1 << ACD);

    sei();

    uint16_t last_trigger = 0;
    ms_counter = 0;

    // first pulse immediately
    start_pulse_sequence();
    last_trigger = 0;

    while (1)
    {
        // Trigger every 1000 ms
        if ((uint16_t)(ms_counter - last_trigger) >= 1000 && state == 0)
        {
            last_trigger = ms_counter;
            start_pulse_sequence();
        }

        if (measurement_done)
        {
            measurement_done = 0;

            if (timeout_flag)
            {
                uart_print("Timeout\r\n");
            }
            else
            {
                uint16_t dist_cm = time_to_cm(echo_time_us);

                //uart_print("Distance: ");
                //uart_print_uint(dist_cm);
                //uart_print(" cm\r\n");
            }

            // Restore pins as outputs in inverted idle state
            DDRD |= (1 << DDD6) | (1 << DDD7);
            PORTD |=  (1 << PORTD6);
            PORTD &= ~(1 << PORTD7);
        }
    }
}
