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

/* Updates by William B Phelps
 *
 * 02sep12 - post to Github
 *
 * 28aug12 - clean up display multiplex code
 *
 * 27aug12 - 10 step brightness for IV-17
 *
 * 26aug12 - display "off" after "alarm" if alarm switch off
 *
 * 25aug12 - change Menu timeout to ~2 seconds
 * show Alarm time when Alarm switch is set to enable the alarm
 * get time from RTC every 200 ms instead of every 150 ms
 * blank hour leading zero if not 24 hr display mode
 * minor typos & cleanup
 *
 */
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include <stdbool.h>
#include <stdlib.h>

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
uint8_t EEMEM b_brightness = 8;
uint8_t EEMEM b_volume = 0;

// Cached settings
uint8_t g_24h_clock = true;
uint8_t g_show_temp = false;
uint8_t g_show_dots = true;
uint8_t g_brightness = 5;
uint8_t g_volume = 0;

// Other globals
uint8_t g_has_dots = false; // can current shield show dot (decimal points)
uint8_t g_alarming = false; // alarm is going off
uint8_t g_alarm_switch;
uint8_t g_show_special_cnt = 0;  // display something special ("time", "alarm", etc)
struct tm* t = NULL; // for holding RTC values

extern enum shield_t shield;

void initialize(void)
{
	// read eeprom
	g_24h_clock  = eeprom_read_byte(&b_24h_clock);
	g_show_temp  = eeprom_read_byte(&b_show_temp);
	g_brightness = eeprom_read_byte(&b_brightness);
	g_volume     = eeprom_read_byte(&b_volume);

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
	rtc_set_ds1307();

	//rtc_set_time_s(16, 59, 50);
	//rtc_set_alarm_s(17,0,0);

	display_init(g_brightness);

	g_alarm_switch = get_alarm_switch();

	// set up interrupt for alarm switch
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18);
}

struct BUTTON_STATE buttons;

// menu states
typedef enum {
	// basic states
	STATE_CLOCK = 0,  // show clock time
	STATE_SET_CLOCK,
	STATE_SET_ALARM,
	// menu
	STATE_MENU_BRIGHTNESS,
	STATE_MENU_24H,
	STATE_MENU_VOL,
	STATE_MENU_TEMP,
	STATE_MENU_DOTS,
	STATE_MENU_LAST,
} menu_state_t;

menu_state_t menu_state = STATE_CLOCK;

// display modes
typedef enum {
	MODE_NORMAL = 0, // normal mode: show time/seconds
	MODE_AMPM, // shows time AM/PM
	MODE_LAST,  // end of display modes for right button pushes
	MODE_ALARM_TEXT,  // show "alarm" (wm)
	MODE_ALARM_TIME,  // show alarm time (wm)
} display_mode_t;

display_mode_t clock_mode = MODE_NORMAL;

// Alarm switch changed interrupt
ISR( PCINT2_vect )
{
	if ( (SWITCH_PIN & _BV(SWITCH_BIT)) == 0)
		g_alarm_switch = false;
	else {
		g_alarm_switch = true;
		}
	g_show_special_cnt = 100;  // show alarm text for 1 second
	clock_mode = MODE_ALARM_TEXT;
}

void read_rtc(display_mode_t mode)  // (wm)  runs approx every 100 ms
{
	static uint16_t counter = 0;
	uint8_t hour = 0, min = 0, sec = 0;
	
	if (mode == MODE_ALARM_TEXT) {
		show_alarm_text();
	}
	else if (mode == MODE_ALARM_TIME) {
		if (g_alarm_switch) {
			rtc_get_alarm_s(&hour, &min, &sec);
			show_time_setting(hour, min, 0);
		}
		else {
			set_string(" off");
		}
	}
	else if(g_show_temp && rtc_is_ds3231() && counter > 125) {
		int8_t t;
		uint8_t f;
		ds3231_get_temp_int(&t, &f);
		show_temp(t, f);
	}
	else {
		t = rtc_get_time();
		if (t == NULL) return;
		show_time(t, g_24h_clock, mode);  // (wm)
	}

	counter++;
	if (counter == 250) counter = 0;
}

#include "piezo.h"

void main(void) __attribute__ ((noreturn));

void main(void)
{
	initialize();

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
	
	set_string("--------");

	piezo_init();
	beep(500, 1);
	beep(1000, 1);
//	beep(500, 1);

	if (shield == SHIELD_NONE) {	
		_delay_ms(200);
		beep(200, 3);
		}
	else
		beep(500, 1);

	while (1) {
		get_button_state(&buttons);
		
		// When alarming:
		// any button press cancels alarm
		if (g_alarming) {
			read_rtc(clock_mode);  // read and display time (??)

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
		else if (menu_state == STATE_CLOCK && buttons.both_held) {
			if (g_alarm_switch) {
				menu_state = STATE_SET_ALARM;
				show_set_alarm();
				rtc_get_alarm_s(&hour, &min, &sec);
				time_to_set = hour*60 + min;
			}
			else {
				menu_state = STATE_SET_CLOCK;
				show_set_time();		
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
		else if (menu_state == STATE_SET_CLOCK || menu_state == STATE_SET_ALARM) {
			// Check if we should exit STATE_SET_CLOCK or STATE_SET_ALARM
			if (buttons.none_held) {
				set_blink(true);
				button_released_timer++;
				button_speed = 50;
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
				if (menu_state == STATE_SET_CLOCK)
					rtc_set_time_s(time_to_set / 60, time_to_set % 60, 0);
				else
					rtc_set_alarm_s(time_to_set / 60, time_to_set % 60, 0);

				menu_state = STATE_CLOCK;
			}
			
			// Increase / Decrease time counter
			if (buttons.b1_repeat) time_to_set+=(button_speed/100);
			if (buttons.b1_keyup)  time_to_set++;
			if (buttons.b2_repeat) time_to_set-=(button_speed/100);
			if (buttons.b2_keyup)  time_to_set--;

			if (time_to_set  >= 1440) time_to_set = 0;
			if (time_to_set  < 0) time_to_set = 1439;

			show_time_setting(time_to_set / 60, time_to_set % 60, 0);
		}
		// Left button enters menu
		else if (menu_state == STATE_CLOCK && buttons.b2_keyup) {
			menu_state = STATE_MENU_BRIGHTNESS;
			show_setting_int("BRIT", "BRITE", g_brightness, false);
			buttons.b2_keyup = 0; // clear state
		}
		// Right button toggles display mode
		else if (menu_state == STATE_CLOCK && buttons.b1_keyup) {
			clock_mode++;
//			if (clock_mode == MODE_ALARM_TEXT)  g_show_special_cnt = 100;  // show alarm text for 1 second
//			if (clock_mode == MODE_ALARM_TIME)  g_show_special_cnt = 100;  // show alarm time for 1 second
			if (clock_mode == MODE_LAST) clock_mode = MODE_NORMAL;
			buttons.b1_keyup = 0; // clear state
		}
		else if (menu_state >= STATE_MENU_BRIGHTNESS) {
			if (buttons.none_held)
				button_released_timer++;
			else
				button_released_timer = 0;
			
//			if (button_released_timer >= 80) {
			if (button_released_timer >= 200) {  // 2 seconds (wm)
				button_released_timer = 0;
				menu_state = STATE_CLOCK;
			}
			
			switch (menu_state) {
				case STATE_MENU_BRIGHTNESS:
					if (buttons.b1_keyup) {
						g_brightness++;
						buttons.b1_keyup = false;
					
						if (g_brightness > 10) g_brightness = 1;
					
						eeprom_update_byte(&b_brightness, g_brightness);
					
//						if (shield == SHIELD_IV17)  // wm ???
//							show_setting_string("BRIT", "BRITE", (g_brightness % 2 == 0) ? "  lo" : "  hi", true);
//						else
						show_setting_int("BRIT", "BRITE", g_brightness, true);
						set_brightness(g_brightness);
					}
					break;
				case STATE_MENU_24H:
					if (buttons.b1_keyup) {
						g_24h_clock = !g_24h_clock;
						eeprom_update_byte(&b_24h_clock, g_24h_clock);
						
						show_setting_string("24H", "24H", g_24h_clock ? " on" : " off", true);
						buttons.b1_keyup = false;
					}
					break;
				case STATE_MENU_VOL:
					if (buttons.b1_keyup) {
						g_volume = !g_volume;
						eeprom_update_byte(&b_volume, g_volume);
						
						show_setting_string("VOL", "VOL", g_volume ? " hi" : " lo", true);
						piezo_init();
						beep(1000, 1);
						buttons.b1_keyup = false;
					}
					break;
				case STATE_MENU_TEMP:
					if (buttons.b1_keyup) {
						g_show_temp = !g_show_temp;
						eeprom_update_byte(&b_show_temp, g_show_temp);
						
						show_setting_string("TEMP", "TEMP", g_show_temp ? " on" : " off", true);
						buttons.b1_keyup = false;
					}
					break;
				case STATE_MENU_DOTS:
					if (buttons.b1_keyup) {
						g_show_dots = !g_show_dots;
						eeprom_update_byte(&b_show_dots, g_show_dots);
						
						show_setting_string("DOTS", "DOTS", g_show_dots ? " on" : " off", true);
						buttons.b1_keyup = false;
					}
					break;
				default:
					break; // do nothing
			}

			if (buttons.b2_keyup) {
				menu_state++;
				
				// show temperature setting only when running on a DS3231
				if (menu_state == STATE_MENU_TEMP && !rtc_is_ds3231()) menu_state++;

				// don't show dots settings for shields that have no dots
				if (menu_state == STATE_MENU_DOTS && !g_has_dots) menu_state++;

				if (menu_state == STATE_MENU_LAST) menu_state = STATE_MENU_BRIGHTNESS;
				
				switch (menu_state) {
					case STATE_MENU_BRIGHTNESS:
						show_setting_int("BRIT", "BRITE", g_brightness, false);
						break;
					case STATE_MENU_VOL:
						show_setting_string("VOL", "VOL", g_volume ? " hi" : " lo", false);
						break;
					case STATE_MENU_24H:
						show_setting_string("24H", "24H", g_24h_clock ? " on" : " off", false);
						break;
					case STATE_MENU_DOTS:
						show_setting_string("DOTS", "DOTS", g_show_dots ? " on" : " off", false);
						break;
					case STATE_MENU_TEMP:
						show_setting_string("TEMP", "TEMP", g_show_temp ? " on" : " off", false);
						break;
					default:
						break; // do nothing
				}
				
				buttons.b2_keyup = 0; // clear state
			}
		}
		else {
			if (g_show_special_cnt>0) {
				g_show_special_cnt--;
				if (g_show_special_cnt == 0)
					switch (clock_mode) {
						case MODE_ALARM_TEXT:
							clock_mode = MODE_ALARM_TIME;
							g_show_special_cnt = 100;
							break;
						case MODE_ALARM_TIME:
							clock_mode = MODE_NORMAL;
							break;
						default:
							clock_mode = MODE_NORMAL;
					}
			}
			// read RTC approx every 200ms  (wm)
			static uint16_t cnt = 0;
			if (cnt++ > 19) {
				read_rtc(clock_mode);  // read RTC and display time
				cnt = 0;
				}
		}
		
		// fixme: alarm should not be checked when setting time or alarm
		if (g_alarm_switch && rtc_check_alarm())
			g_alarming = true;

		_delay_ms(7);  // roughly 10 ms per loop
	}
}
