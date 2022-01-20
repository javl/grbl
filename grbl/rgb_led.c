/*
This file is not part of the original Grbl
Based on this article on Instructables:
https://www.instructables.com/Bitbanging-step-by-step-Arduino-control-of-WS2811-/
*/

#include "grbl.h"

#define RGB_LED_NUM_RGB 1
#define RGB_LED_NUM_BYTES (RGB_LED_NUM_RGB*3)  // Number of LEDs (3 per each WS281X)
#define RGB_LED_NUM_BITS  8  // bits per byte

uint8_t* rgb_led_rgb_arr = NULL;
uint32_t rgb_led_t_f;

void rgb_led_init(void) 
{

  RGB_LED_DDR |= (1 << RGB_LED_BIT); // Configure as output pin
  RGB_LED_PORT &= ~(1<<RGB_LED_BIT); // Set pin to low
  // RGB_LED_PORT |= (1<<RGB_LED_BIT);  // Set pin to high

  if((rgb_led_rgb_arr = (uint8_t *)malloc(RGB_LED_NUM_BYTES))) 
  {
    memset(rgb_led_rgb_arr, 0, RGB_LED_NUM_BYTES);
  }
  rgb_led_render();
}

void rgb_led_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) 
{
  // printPgmString(PSTR("Set RGB to "));
  // print_uint8_base10(r); 
  // printPgmString(PSTR(", "));
  // print_uint8_base10(g); 
  // printPgmString(PSTR(", "));
  // print_uint8_base10(b); 
  // printPgmString(PSTR("\n"));

  if(idx < RGB_LED_NUM_RGB) {
    uint8_t *p = &rgb_led_rgb_arr[idx*3];
    *p++ = g;
    *p++ = r;
    *p = b;
  }
  rgb_led_render();
}

// unsigned long rgb_led_micros() 
// {
// 	unsigned long m;
// 	uint8_t oldSREG = SREG, t;
// 	cli();
// 	return ((m << 8) + t) * (64 / ( F_CPU / 1000000L ));
// }

void rgb_led_render(void) 
{
  if(!rgb_led_rgb_arr) { return; }

  // while((rgb_led_micros() - rgb_led_t_f) < 50L);  // wait for 50us (data latch)
  // while((micros() - rgb_led_t_f) < 50L);  // wait for 50us (data latch)
  // printPgmString(PSTR("done waiting\r\n"));

  cli(); // Disable interrupts so that timing is as precise as possible

  volatile uint8_t
   *p    = rgb_led_rgb_arr,   // Copy the start address of our data array
    val  = *p++,      // Get the current byte value & point to next byte
    high = RGB_LED_PORT |  _BV(RGB_LED_BIT), // Bitmask for sending HIGH to pin
    low  = RGB_LED_PORT & ~_BV(RGB_LED_BIT), // Bitmask for sending LOW to pin
    tmp  = low,       // Swap variable to adjust duty cycle
    nbits= RGB_LED_NUM_BITS;  // Bit counter for inner loop
  volatile uint16_t nbytes = RGB_LED_NUM_BYTES; // Byte counter for outer loop
  asm volatile(
  // The volatile attribute is used to tell the compiler not to optimize
  // this section.  We want every instruction to be left as is.
  //
  // Generating an 800KHz signal (1.25us period) implies that we have
  // exactly 20 instructions clocked at 16MHz (0.0625us duration) to
  // generate either a 1 or a 0---we need to do it within a single
  // period.
  //
  // By choosing 1 clock cycle as our time unit we can keep track of
  // the signal's phase (T) after each instruction is executed.
  //
  // To generate a value of 1, we need to hold the signal HIGH (maximum)
  // for 0.8us, and then LOW (minimum) for 0.45us.  Since our timing has a
  // resolution of 0.0625us we can only approximate these values. Luckily,
  // the WS281X chips were designed to accept a +/- 300ns variance in the
  // duration of the signal.  Thus, if we hold the signal HIGH for 13
  // cycles (0.8125us), and LOW for 7 cycles (0.4375us), then the variance
  // is well within the tolerated range.
  //
  // To generate a value of 0, we need to hold the signal HIGH (maximum)
  // for 0.4us, and then LOW (minimum) for 0.85us.  Thus, holding the
  // signal HIGH for 6 cycles (0.375us), and LOW for 14 cycles (0.875us)
  // will maintain the variance within the tolerated range.
  //
  // For a full description of each assembly instruction consult the AVR
  // manual here: http://www.atmel.com/images/doc0856.pdf
    // Instruction        CLK     Description                 Phase
   "nextbit:\n\t"         // -    label                       (T =  0)
    "sbi  %0, %1\n\t"     // 2    signal HIGH                 (T =  2)
    "sbrc %4, 7\n\t"      // 1-2  if MSB set                  (T =  ?)
     "mov  %6, %3\n\t"    // 0-1   tmp'll set signal high     (T =  4)
    "dec  %5\n\t"         // 1    decrease bitcount           (T =  5)
    "nop\n\t"             // 1    nop (idle 1 clock cycle)    (T =  6)
    "st   %a2, %6\n\t"    // 2    set PORT to tmp             (T =  8)
    "mov  %6, %7\n\t"     // 1    reset tmp to low (default)  (T =  9)
    "breq nextbyte\n\t"   // 1-2  if bitcount ==0 -> nextbyte (T =  ?)
    "rol  %4\n\t"         // 1    shift MSB leftwards         (T = 11)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "nop\n\t"             // 1    nop                         (T = 18)
    "rjmp nextbit\n\t"    // 2    bitcount !=0 -> nextbit     (T = 20)
   "nextbyte:\n\t"        // -    label                       -
    "ldi  %5, 8\n\t"      // 1    reset bitcount              (T = 11)
    "ld   %4, %a8+\n\t"   // 2    val = *p++                  (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "nop\n\t"             // 1    nop                         (T = 18)
    "dec %9\n\t"          // 1    decrease bytecount          (T = 19)
    "brne nextbit\n\t"    // 2    if bytecount !=0 -> nextbit (T = 20)
    ::
    // Input operands         Operand Id (w/ constraint)
    "I" (_SFR_IO_ADDR(RGB_LED_PORT)), // %0
    "I" (RGB_LED_BIT),           // %1
    "e" (&RGB_LED_PORT),              // %a2
    "r" (high),               // %3
    "r" (val),                // %4
    "r" (nbits),              // %5
    "r" (tmp),                // %6
    "r" (low),                // %7
    "e" (p),                  // %a8
    "w" (nbytes)              // %9
  );
  sei();                            // Enable interrupts
  // rgb_led_t_f = rgb_led_micros();   // t_f will be used to measure the 50us
  // rgb_led_t_f = micros();   // t_f will be used to measure the 50us
                                    // latching period in the next call of the
                                    // function.
}
