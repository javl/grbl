/*
This file is not part of the original Grbl
Based on this article on Instructables:
https://www.instructables.com/Bitbanging-step-by-step-Arduino-control-of-WS2811-/
*/

#ifndef rgb_led_h
#define rgb_led_h

void rgb_led_init(void);
void rgb_led_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
void rgb_led_render(void);

#endif
