/*
 * VFD Modular Clock
 * (C) 2011 Akafugu Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include <stdbool.h>

#include "display.h"
#include "button.h"

#include "twi.h"
#include "rtc.h"

// Second indicator LED (optional second indicator on shield)
#define LED_BIT PD7
#define LED_DDR DDRD
#define LED_PORT PORTD
#define LED_HIGH LED_PORT |= _BV(LED_BIT)
#define LED_HIGH LED_PORT |= _BV(LED_BIT)
#define LED_LOW  LED_PORT &= ~(_BV(LED_BIT))

// Piezo
#define PIEZO_PORT PORTB
#define PIEZO_DDR  DDRB
#define PIEZO_LOW_BIT  PB1
#define PIEZO_HIGH_BIT PB2
#define PIEZO_LOW_OFF  PIEZO_PORT &= ~(_BV(PIEZO_LOW_BIT))
#define PIEZO_HIGH_OFF PIEZO_PORT &= ~(_BV(PIEZO_HIGH_BIT))
#define PIEZO_LOW_ON  PIEZO_PORT |= _BV(PIEZO_LOW_BIT)
#define PIEZO_HIGH_ON PIEZO_PORT |= _BV(PIEZO_HIGH_BIT)

// Settings saved to eeprom
uint8_t EEMEM b_24h_clock = true;
uint8_t EEMEM b_show_temp = false;
uint8_t EEMEM b_show_dots = true;
uint8_t EEMEM b_brightness = 80;

// Cached settings
uint8_t g_24h_clock = true;
uint8_t g_show_temp = false;
uint8_t g_show_dots = true;
uint8_t g_brightness = 80;

// Other globals
uint8_t g_has_dots = false; // can current shield show dot (decimal points)
uint8_t g_alarming = false; // alarm is going off
struct tm* t = NULL; // for holding RTC values

void initialize(void)
{
	// read eeprom
	g_24h_clock  = eeprom_read_byte(&b_24h_clock);
	g_show_temp  = eeprom_read_byte(&b_show_temp);
	g_brightness = eeprom_read_byte(&b_brightness);

	PIEZO_DDR |= _BV(PIEZO_LOW_BIT);
	PIEZO_DDR |= _BV(PIEZO_HIGH_BIT);
	
	PIEZO_LOW_OFF;
	PIEZO_HIGH_OFF;

	// Set buttons as inputs
	BUTTON_DDR &= ~(_BV(BUTTON1_BIT));
	BUTTON_DDR &= ~(_BV(BUTTON2_BIT));
	SWITCH_DDR  &= ~(_BV(SWITCH_BIT));
	
	// Enable pullups for buttons
	BUTTON_PORT |= _BV(BUTTON1_BIT);
	BUTTON_PORT |= _BV(BUTTON2_BIT);
	SWITCH_PORT |= _BV(SWITCH_BIT);

	LED_DDR  |= _BV(LED_BIT); // indicator led

	for (int i = 0; i < 5; i++) {
		LED_HIGH;
		_delay_ms(100);
		LED_LOW;
		_delay_ms(100);
	}

	sei();
	twi_init_master();
	
	rtc_init();
	//rtc_set_time_s(17, 6, 0);

	display_init(g_brightness);
}

void read_rtc(bool show_extra_info)
{
	static uint16_t counter = 0;
	
	if(g_show_temp && rtc_is_ds3231() && counter > 125) {
		int8_t t;
		uint8_t f;
		ds3231_get_temp_int(&t, &f);
		set_temp(t, f);
	}
	else {
		t = rtc_get_time();

		if (show_extra_info /*&& get_digits() == 4*/) {
			if (t->am) set_string("AM");
			else set_string("PM");
		}
		else if (g_24h_clock)
			set_time(t->hour, t->min, t->sec);
		else
			set_time(t->twelveHour, t->min, t->sec);
	}

	counter++;
	if (counter == 250) counter = 0;
}

struct BUTTON_STATE buttons;

// menu states
typedef enum {
	// basic states
	STATE_CLOCK = 0,
	STATE_SET_CLOCK,
	STATE_SET_ALARM,
	// menu
	STATE_MENU_BRIGHTNESS,
	STATE_MENU_24H,
	STATE_MENU_TEMP,
	STATE_MENU_DOTS,
	STATE_MENU_LAST,
} state_t;

state_t clock_state = STATE_CLOCK;

// display modes
typedef enum {
	MODE_NORMAL = 0, // normal mode: show time/seconds
	MODE_AMPM, // shows time AM/PM
	MODE_LAST,
} display_mode_t;

display_mode_t clock_mode = MODE_NORMAL;

#include "piezo.h"

void main(void) __attribute__ ((noreturn));

void main(void)
{
	initialize();
	detect_shield();

	/*
	// test: write alphabet
	while (1) {
		for (int i = 'A'; i <= 'Z'+1; i++) {
			set_char_at(i, 0);
			set_char_at(i+1, 1);
			set_char_at(i+2, 2);
			set_char_at(i+3, 3);

			if (get_digits() == 6) {
				set_char_at(i+4, 4);
				set_char_at(i+5, 5);
			}

			_delay_ms(250);
		}
	}
	*/
	
	uint8_t hour = 0, min = 0, sec = 0;

	// Counters used when setting time
	int16_t time_to_set = 0;
	uint16_t button_released_timer = 0;
	uint16_t button_speed = 25;
	
	if (get_digits() == 6)
		set_string("------");
	else
		set_string("----");

	piezo_init();
	beep(500, 1);
	beep(1000, 1);
	beep(500, 1);

	while (1) {
		get_button_state(&buttons);
		
		// When alarming:
		// any button press cancels alarm
		if (g_alarming) {
			read_rtc(clock_mode == MODE_AMPM ? true : false);

			// fixme: if keydown is detected here, wait for keyup and clear state
			// this prevents going into the menu when disabling the alarm
			if (buttons.b1_keydown || buttons.b1_keyup || buttons.b2_keydown || buttons.b2_keyup) {
				buttons.b1_keyup = 0; // clear state
				buttons.b2_keyup = 0; // clear state
				g_alarming = false;
			}
			else {
				alarm();	
			}
		}
		// If both buttons are held:
		//  * If the ALARM BUTTON SWITCH is on the LEFT, go into set time mode
		//  * If the ALARM BUTTON SWITCH is on the RIGHT, go into set alarm mode
		else if (clock_state == STATE_CLOCK && buttons.both_held) {
			if (get_alarm_switch()) {
				clock_state = STATE_SET_ALARM;
			
				if (get_digits() == 6)
					set_string("Alarm");
				else
					set_string("Alrm");

				rtc_get_alarm_s(&hour, &min, &sec);
				time_to_set = hour*60 + min;
			}
			else {
				clock_state = STATE_SET_CLOCK;
			
				if (get_digits() == 6)
					set_string(" time ");
				else
					set_string("Time");
				
				rtc_get_time_s(&hour, &min, &sec);
				time_to_set = hour*60 + min;
			}

			set_blink(true);
			
			// wait until both buttons are released
			while (1) {
				_delay_ms(50);
				get_button_state(&buttons);
				if (buttons.none_held)
					break;
			}
		}
		// Set time or alarm
		else if (clock_state == STATE_SET_CLOCK || clock_state == STATE_SET_ALARM) {
			// Check if we should exit STATE_SET_CLOCK or STATE_SET_ALARM
			if (buttons.none_held) {
				set_blink(true);
				button_released_timer++;
				button_speed = 25;
			}
			else {
				set_blink(false);
				button_released_timer = 0;
				button_speed++;
			}

			// exit mode after no button has been touched for a while
			if (button_released_timer >= 160) {
				set_blink(false);
				button_released_timer = 0;
				button_speed = 1;
				
				// fixme: should be different in 12h mode
				if (clock_state == STATE_SET_CLOCK)
					rtc_set_time_s(time_to_set / 60, time_to_set % 60, 0);
				else
					rtc_set_alarm_s(time_to_set / 60, time_to_set % 60, 0);

				clock_state = STATE_CLOCK;
			}
			
			// Increase / Decrease time counter
			if (buttons.b1_repeat) time_to_set+=(button_speed/25);
			if (buttons.b1_keyup)  time_to_set++;
			if (buttons.b2_repeat) time_to_set-=(button_speed/25);
			if (buttons.b2_keyup)  time_to_set--;

			if (time_to_set  >= 1440) time_to_set = 0;
			if (time_to_set  < 0) time_to_set = 1439;

			set_time(time_to_set / 60, time_to_set % 60, 0);
		}
		// Left button enters menu
		else if (clock_state == STATE_CLOCK && buttons.b2_keyup) {
			clock_state = STATE_MENU_BRIGHTNESS;
			if (get_digits() == 4)
				set_string("BRIT");
			else
				set_string("BRITE");
			buttons.b2_keyup = 0; // clear state
		}
		// Right button toggles display mode
		else if (clock_state == STATE_CLOCK && buttons.b1_keyup) {
			clock_mode++;
			if (clock_mode == MODE_LAST) clock_mode = MODE_NORMAL;
			buttons.b2_keyup = 0; // clear state
		}
		else if (clock_state >= STATE_MENU_BRIGHTNESS) {
			if (buttons.none_held)
				button_released_timer++;
			else
				button_released_timer = 0;
			
			if (button_released_timer >= 80) {
				button_released_timer = 0;
				clock_state = STATE_CLOCK;
			}
			
			switch (clock_state) {
				case STATE_MENU_BRIGHTNESS:
					if (buttons.b1_keyup) {
						g_brightness += 10;
						buttons.b1_keyup = false;
					
						if (g_brightness > 250) g_brightness = 0;
					
						eeprom_update_byte(&b_brightness, g_brightness);
					
						set_number(g_brightness); // make scale appear as 0-100
						set_brightness(g_brightness);
					}
					break;
				case STATE_MENU_24H:
					if (buttons.b1_keyup) {
						g_24h_clock = !g_24h_clock;
						eeprom_update_byte(&b_24h_clock, g_24h_clock);
						
						set_string(g_24h_clock ? " on" : " off");
						buttons.b1_keyup = false;
					}
					break;
				case STATE_MENU_TEMP:
					if (buttons.b1_keyup) {
						g_show_temp = !g_show_temp;
						eeprom_update_byte(&b_show_temp, g_show_temp);
						
						set_string(g_show_temp ? " on" : " off");
						buttons.b1_keyup = false;
					}
					break;
				case STATE_MENU_DOTS:
					if (buttons.b1_keyup) {
						g_show_dots = !g_show_dots;
						eeprom_update_byte(&b_show_dots, g_show_dots);
						
						set_string(g_show_dots ? " on" : " off");
						buttons.b1_keyup = false;
					}
					break;
				default:
					break; // do nothing
			}

			if (buttons.b2_keyup) {
				clock_state++;
				
				// show temperature setting only when running on a DS3231
				if (clock_state == STATE_MENU_TEMP && !rtc_is_ds3231()) clock_state++;

				// don't show dots settings for shields that have no dots
				if (clock_state == STATE_MENU_DOTS && !g_has_dots) clock_state++;

				if (clock_state == STATE_MENU_LAST) clock_state = STATE_MENU_BRIGHTNESS;
				
				switch (clock_state) {
					case STATE_MENU_BRIGHTNESS:
						if (get_digits() == 4)
							set_string("BRIT");
						else
							set_string("BRITE");
						break;
					case STATE_MENU_24H:
						set_string("24H");
						break;
					case STATE_MENU_DOTS:
						set_string("DOTS");
						break;
					case STATE_MENU_TEMP:
						set_string("TEMP");
						break;
					default:
						break; // do nothing
				}
				
				buttons.b2_keyup = 0; // clear state
			}
		}
		else {
			read_rtc(clock_mode == MODE_AMPM ? true : false);
		}
		
		// fixme: alarm should not be checked when setting time or alarm
		if (rtc_check_alarm() && get_alarm_switch())
			g_alarming = true;

		_delay_ms(10);
	}
}
