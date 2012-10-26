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
 * 25oct12 implement Auto DST
 *
 * 24oct12 fix date display for larger displays
 *
 * 22oct12 add DATE to menu, add FEATURE_SET_DATE and FEATURE_AUTO_DATE
 * added to menu: YEAR, MONTH, DAY
 *
 * 12oct12 fix blank display when setting time (blink)
 * return to AMPM after AutoDate display
 *
 * 05oct12 clean up GPS mods for commit
 * add get/set time in time_t to RTC
 * slight blink when RTC updated from GPS to show signal reception
 *
 * 26sap12
 * fix EUR date display
 * add GPS 48, 96 (support 9600 baud GPS) 
 *
 * 25sep12
 * improve date scrolling
 * add Time.h/Time.c, use tmElements_t structure instead of tm
 * add '/' to font-16seg.c
 * 
 * 24sep12 - add time zone & DST menu items
 * show current value without change on 1st press of b1
 * scroll the date for smaller displays
 *
 * 21sep12 - GPS support
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

// defines moved to makefile
//#define FEATURE_SDATE  // add (set) DATE to menu
//#define FEATURE_ADATE  // automatic date display 
//#define FEATURE_WmGPS  // GPS support
//#define FEATURE_WmDST  // AUto DST support
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include <stdbool.h>
#include <stdlib.h>

#include "Time.h"
#include "display.h"
#include "button.h"
#include "twi.h"
#include "rtc.h"
#include "piezo.h"

#ifdef FEATURE_AUTO_DST
#include "adst.h"
#endif
#ifdef FEATURE_WmGPS
#include "gps.h"
#endif

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
#ifdef FEATURE_WmGPS
uint8_t EEMEM b_gps_enabled = 0;  // 0, 48, or 96 - default no gps
uint8_t EEMEM b_TZ_hour = -8 + 12;
uint8_t EEMEM b_TZ_minutes = 0;
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
uint8_t EEMEM b_DST_mode = 0;  // 0: off, 1: on, 2: Auto
uint8_t EEMEM b_DST_offset = 0;
#endif
#ifdef FEATURE_AUTO_DATE
uint8_t EEMEM b_Region = 0;  // default European date format yyyy/mm/dd
uint8_t EEMEM b_AutoDate = false;
#endif
// Cached settings
uint8_t g_24h_clock = true;
uint8_t g_show_temp = false;
uint8_t g_show_dots = true;
uint8_t g_brightness = 5;
uint8_t g_volume = 0;
#ifdef FEATURE_SET_DATE
uint8_t g_dateyear = 12;
uint8_t g_datemonth = 1;
uint8_t g_dateday = 1;
#endif
#ifdef FEATURE_WmGPS 
uint8_t g_gps_enabled = 0;
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
uint8_t g_DST_mode;  // DST off, on, auto?
uint8_t g_DST_offset;  // DST offset in Hours
uint8_t g_DST_update = 0;  // DST update flag
#endif
#ifdef FEATURE_AUTO_DST
DST_Rules dst_rules = {{3,1,2,2},{11,1,1,2},1};   // initial values from US DST rules as of 2011
// DST Rules: Start(month, dotw, week, hour), End(month, dotw, week, hour), Offset
// DOTW is Day of the Week, 1=Sunday, 7=Saturday
// N is which occurrence of DOTW
// Current US Rules:  March, Sunday, 2nd, 2am, November, Sunday, 1st, 2 am, 1 hour
const DST_Rules dst_rules_lo = {{1,1,1,0},{1,1,1,0},0};  // low limit
const DST_Rules dst_rules_hi = {{12,7,5,23},{12,7,5,23},1};  // high limit
#endif
#ifdef FEATURE_AUTO_DATE
uint8_t g_region = 0;
uint8_t g_autodate = false;
uint8_t g_autotime = 54;  // when to display date
uint16_t g_autodisp = 550;  // how long to display date
#endif

// Other globals
uint8_t g_has_dots = false; // can current shield show dot (decimal points)
uint8_t g_alarming = false; // alarm is going off
uint8_t g_alarm_switch;
uint16_t g_show_special_cnt = 0;  // display something special ("time", "alarm", etc)
//time_t g_tNow = 0;  // current local date and time as time_t, with DST offset
tmElements_t* te; // current local date and time as TimeElements

extern enum shield_t shield;

void initialize(void)
{
	// read eeprom
	g_24h_clock  = eeprom_read_byte(&b_24h_clock);
	g_show_temp  = eeprom_read_byte(&b_show_temp);
	g_brightness = eeprom_read_byte(&b_brightness);
	g_volume     = eeprom_read_byte(&b_volume);
#ifdef FEATURE_WmGPS
	g_gps_enabled = eeprom_read_byte(&b_gps_enabled);
	g_TZ_hour = eeprom_read_byte(&b_TZ_hour) - 12;
	g_TZ_minutes = eeprom_read_byte(&b_TZ_minutes);
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
	g_DST_mode = eeprom_read_byte(&b_DST_mode);
	g_DST_offset = eeprom_read_byte(&b_DST_offset);
#endif
#ifdef FEATURE_AUTO_DATE
	g_region = eeprom_read_byte(&b_Region);
	g_autodate = eeprom_read_byte(&b_AutoDate);
#endif

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
	
#ifdef FEATURE_WmGPS
  // setup uart for GPS
	gps_init(g_gps_enabled);
#endif
	
//#ifdef FEATURE_AUTO_DATE
//	if (get_digits() > 4) {
//		g_autotime = 54;  // display date at secs = 58;
//		g_autodisp = 550;  // display for 1.5 secs;
//	}
//#endif
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
#ifdef FEATURE_SET_DATE
	STATE_MENU_SETYEAR,
	STATE_MENU_SETMONTH,
	STATE_MENU_SETDAY,
#endif
#ifdef FEATURE_AUTO_DATE
	STATE_MENU_AUTODATE,
#endif
#ifdef FEATURE_WmGPS
	STATE_MENU_DST,
	STATE_MENU_GPS,
	STATE_MENU_REGION,
	STATE_MENU_ZONEH,
	STATE_MENU_ZONEM,
#endif
	STATE_MENU_TEMP,
	STATE_MENU_DOTS,
	STATE_MENU_LAST,
} menu_state_t;

menu_state_t menu_state = STATE_CLOCK;
bool menu_b1_first = false;

// display modes
typedef enum {
	MODE_NORMAL = 0, // normal mode: show time/seconds
	MODE_AMPM, // shows time AM/PM
#ifdef FEATURE_AUTO_DATE
	MODE_DATE, // shows date
#endif
	MODE_LAST,  // end of display modes for right button pushes
	MODE_ALARM_TEXT,  // show "alarm" (wm)
	MODE_ALARM_TIME,  // show alarm time (wm)
} display_mode_t;

display_mode_t clock_mode = MODE_NORMAL;
display_mode_t save_mode = MODE_NORMAL;  // for restoring mode after autodate display

// Alarm switch changed interrupt
ISR( PCINT2_vect )
{
	if ( (SWITCH_PIN & _BV(SWITCH_BIT)) == 0)
		g_alarm_switch = false;
	else {
		g_alarm_switch = true;
		}
	g_show_special_cnt = 100;  // show alarm text for 1 second
	if (get_digits() == 8)
		clock_mode = MODE_ALARM_TIME;
	else
		clock_mode = MODE_ALARM_TEXT;
}

#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
void setDSToffset(int8_t newOffset) {
	int8_t adjOffset;
	if (newOffset == 2) {  // Auto DST
		newOffset = getDSToffset(te, &dst_rules);  // get current DST offset based on DST Rules
	}
	adjOffset = newOffset - g_DST_offset;  // offset delta
	if (adjOffset == 0)  return;  // nothing to do
	time_t tNow = rtc_get_time_t();  // fetch current time from RTC as time_t
	tNow += adjOffset * SECS_PER_HOUR;  // add or subtract new DST offset
	rtc_set_time_t(tNow);  // adjust RTC
	g_DST_offset = newOffset;
	eeprom_update_byte(&b_DST_offset, g_DST_offset);
}
#endif

void display_time(display_mode_t mode)  // (wm)  runs approx every 200 ms
{
	static uint16_t counter = 0;
	uint8_t hour = 0, min = 0, sec = 0;
	
#ifdef FEATURE_AUTO_DATE
	if (mode == MODE_DATE) {
		show_date(te, g_region);  // show date from last rtc_get_time() call
	}
	else
#endif	
	if (mode == MODE_ALARM_TEXT) {
		show_alarm_text();
	}
	else if (mode == MODE_ALARM_TIME) {
		if (g_alarm_switch) {
			rtc_get_alarm_s(&hour, &min, &sec);
			show_alarm_time(hour, min, 0);
		}
		else {
			show_alarm_off();
		}
	}
	else if (g_show_temp && rtc_is_ds3231() && counter > 125) {
		int8_t t;
		uint8_t f;
		ds3231_get_temp_int(&t, &f);
		show_temp(t, f);
	}
	else {
		te = rtc_get_time();
		if (te == NULL) return;
#ifdef FEATURE_SET_DATE
		g_dateyear = te->Year;  // save year for Menu
		g_datemonth = te->Month;  // save month for Menu
		g_dateday = te->Day;  // save day for Menu
#endif
#ifdef FEATURE_AUTO_DST
		if (te->Second%10 == 0) {  // check DST Offset every 10 seconds
			setDSToffset(g_DST_mode); 
		}
#endif
#ifdef FEATURE_AUTO_DATE
		if (g_autodate && (te->Second == g_autotime) ) { 
			save_mode = clock_mode;  // save current mode
			clock_mode = MODE_DATE;  // display date now
			g_show_special_cnt = g_autodisp;  // show date for g_autodisp ms
			scroll_ctr = 0;  // reset scroll position
			}
		else
#endif		
		show_time(te, g_24h_clock, mode);  // (wm)
	}
	counter++;
	if (counter == 250) counter = 0;
}

#ifdef FEATURE_SET_DATE
void set_date(uint8_t yy, uint8_t mm, uint8_t dd) {
	te->Year = yy;
	te->Month = mm;
	te->Day = dd;
	rtc_set_time(te);
}
#endif

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
	
	switch (shield) {
		case(SHIELD_IV6):
			set_string("IV-6");
			break;
		case(SHIELD_IV17):
			set_string("IV17");
			break;
		case(SHIELD_IV18):
			set_string("IV18");
			break;
		case(SHIELD_IV22):
			set_string("IV22");
			break;
		default:
			break;
	}

	piezo_init();
	beep(500, 1);
	beep(1000, 1);
	beep(500, 1);

	_delay_ms(500);
	set_string("--------");

	while (1) {  // <<< ===================== MAIN LOOP ===================== >>>
		get_button_state(&buttons);
		
		// When alarming:
		// any button press cancels alarm
		if (g_alarming) {
			display_time(clock_mode);  // read and display time (??)

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
				button_speed = 1;
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
			if (get_digits() == 4)  // only set first time flag for 4 digit displays
				menu_b1_first = true;  // set first time flag
			show_setting_int("BRIT", "BRITE", g_brightness, false);
			buttons.b2_keyup = 0; // clear state
		}
		// Right button toggles display mode
		else if (menu_state == STATE_CLOCK && buttons.b1_keyup) {
			clock_mode++;
#ifdef FEATURE_AUTO_DATE
			if (clock_mode == MODE_DATE) {
				g_show_special_cnt = g_autodisp;  // show date for g_autodisp ms
				scroll_ctr = 0;  // reset scroll position
			}
#endif			
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
//				flash_display(100);  // flash briefly
			}
			
			if (buttons.b1_keyup) {
				switch (menu_state) {
					case STATE_MENU_BRIGHTNESS:
						if (!menu_b1_first)	
							g_brightness++;
						if (g_brightness > 10) g_brightness = 1;
						eeprom_update_byte(&b_brightness, g_brightness);
//						if (shield == SHIELD_IV17)  // wm ???
//							show_setting_string("BRIT", "BRITE", (g_brightness % 2 == 0) ? "  lo" : "  hi", true);
//						else
						show_setting_int("BRIT", "BRITE", g_brightness, true);
						set_brightness(g_brightness);
						break;
					case STATE_MENU_24H:
						if (!menu_b1_first)	
							g_24h_clock = !g_24h_clock;
						eeprom_update_byte(&b_24h_clock, g_24h_clock);
						show_setting_string("24H", "24H", g_24h_clock ? " on" : " off", true);
						break;
					case STATE_MENU_VOL:
						if (!menu_b1_first)	
							g_volume = !g_volume;
						eeprom_update_byte(&b_volume, g_volume);
						show_setting_string("VOL", "VOL", g_volume ? " hi" : " lo", true);
						piezo_init();
						beep(1000, 1);
						break;
#ifdef FEATURE_SET_DATE						
					case STATE_MENU_SETYEAR:
						if (!menu_b1_first)
							g_dateyear++;
						if (g_dateyear > 29) g_dateyear = 10;  // 20 years...
						show_setting_int("YEAR", "YEAR ", g_dateyear, true);
						set_date(g_dateyear, g_datemonth, g_dateday);
						break;
					case STATE_MENU_SETMONTH:
						if (!menu_b1_first)
							g_datemonth++;
						if (g_datemonth > 12) g_datemonth = 1;  
						show_setting_int("MNTH", "MONTH", g_datemonth, true);
						set_date(g_dateyear, g_datemonth, g_dateday);
						break;
					case STATE_MENU_SETDAY:
						if (!menu_b1_first)
							g_dateday++;
						if (g_dateday > 31) g_dateday = 1;  
						show_setting_int("DAY", "DAY  ", g_dateday, true);
						set_date(g_dateyear, g_datemonth, g_dateday);
						break;
#endif						
#ifdef FEATURE_AUTO_DATE						
					case STATE_MENU_AUTODATE:
						if (!menu_b1_first)	
							g_autodate = !g_autodate;
						eeprom_update_byte(&b_AutoDate, g_autodate);
						show_setting_string("ADTE", "ADATE", g_autodate ? " on " : " off", true);
						break;
					case STATE_MENU_REGION:
						if (!menu_b1_first)	
							g_region = (g_region+1)%2;  // 0 = EUR, 1 = USA
						eeprom_update_byte(&b_Region, g_region);
						show_setting_string("REGN", "REGION", g_region ? " USA" : " EUR", true);
						break;
#endif						
#ifdef FEATURE_WmGPS
					case STATE_MENU_GPS:
						if (!menu_b1_first)	
							g_gps_enabled = (g_gps_enabled+48)%144;  // 0, 48, 96
						eeprom_update_byte(&b_gps_enabled, g_gps_enabled);
						gps_init(g_gps_enabled);  // change baud rate
						show_setting_string("GPS", "GPS", gps_setting(g_gps_enabled), true);
						break;
#endif
#if defined FEATURE_AUTO_DST
					case STATE_MENU_DST:
						if (!menu_b1_first) {	
							g_DST_mode = (g_DST_mode+1)%3;  //  0: off, 1: on, 2: auto
							eeprom_update_byte(&b_DST_mode, g_DST_mode);
							setDSToffset(g_DST_mode);
						}
						show_setting_string("DST", "DST", dst_setting(g_DST_mode), true);
						break;
#elif defined FEATURE_WmGPS
					case STATE_MENU_DST:
						if (!menu_b1_first) {	
							g_DST_mode = (g_DST_mode+1)%2;  //  0: off, 1: on
							eeprom_update_byte(&b_DST_mode, g_DST_mode);
							setDSToffset(g_DST_mode);
						}
						show_setting_string("DST", "DST", g_DST_mode ? " on" : " off", true);
						break;
#endif
#ifdef FEATURE_WmGPS
					case STATE_MENU_ZONEH:
						if (!menu_b1_first)	
							g_TZ_hour++;
						if (g_TZ_hour > 12) g_TZ_hour = -12;
						eeprom_update_byte(&b_TZ_hour, g_TZ_hour + 12);
						show_setting_int("TZ-H", "TZ-H", g_TZ_hour, true);
						break;
					case STATE_MENU_ZONEM:
						if (!menu_b1_first)	
							g_TZ_minutes = (g_TZ_minutes + 15) % 60;;
						eeprom_update_byte(&b_TZ_minutes, g_TZ_minutes);
						show_setting_int("TZ-M", "TZ-M", g_TZ_minutes, true);
						break;
#endif
					case STATE_MENU_TEMP:
						if (!menu_b1_first)	
							g_show_temp = !g_show_temp;
						eeprom_update_byte(&b_show_temp, g_show_temp);
						show_setting_string("TEMP", "TEMP", g_show_temp ? " on" : " off", true);
						break;
					case STATE_MENU_DOTS:
						if (!menu_b1_first)	
							g_show_dots = !g_show_dots;
						eeprom_update_byte(&b_show_dots, g_show_dots);
						show_setting_string("DOTS", "DOTS", g_show_dots ? " on" : " off", true);
						break;
					default:
						break; // do nothing
				}  // switch (menu_state)
			buttons.b1_keyup = false;
			menu_b1_first = false;  // b1 not first time now
			}  // if (buttons.b1_keyup) 

			if (buttons.b2_keyup) {
				menu_state++;
				if (get_digits() == 4)  // only set first time flag for 4 digit displays
					menu_b1_first = true;  // reset b1 first time flag
				
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
#ifdef FEATURE_SET_DATE						
					case STATE_MENU_SETYEAR:
						show_setting_int("YEAR", "YEAR ", g_dateyear, false);
						break;
					case STATE_MENU_SETMONTH:
						show_setting_int("MNTH", "MONTH", g_datemonth, false);
						break;
					case STATE_MENU_SETDAY:
						show_setting_int("DAY ", "DAY  ", g_dateday, false);
						break;
#endif						
#ifdef FEATURE_AUTO_DATE
					case STATE_MENU_AUTODATE:
						show_setting_string("ADTE", "ADATE", g_autodate ? " on " : " off", false);
						break;
					case STATE_MENU_REGION:
						show_setting_string("REGN", "REGION", g_region ? " USA" : " EUR", false);
						break;
#endif						
#ifdef FEATURE_WmGPS
					case STATE_MENU_GPS:
						show_setting_string("GPS", "GPS", gps_setting(g_gps_enabled), false);
						break;
					case STATE_MENU_DST:
#ifdef FEATURE_AUTO_DST
						show_setting_string("DST", "DST", dst_setting(g_DST_mode), false);
#else
						show_setting_int("DST", "DST", g_DST, false);
#endif
						break;
					case STATE_MENU_ZONEH:
						show_setting_int("TZ-H", "TZ-H", g_TZ_hour, false);
						break;
					case STATE_MENU_ZONEM:
						show_setting_int("TZ-M", "TZ-M", g_TZ_minutes, false);
						break;
#endif
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
#ifdef FEATURE_AUTO_DATE
						case MODE_DATE:
							clock_mode = save_mode;  // 11oct12/wbp
							break;
#endif							
						default:
							clock_mode = MODE_NORMAL;
					}
			}

			// read RTC & update display approx every 200ms  (wm)
			static uint16_t cnt = 0;
			if (cnt++ > 19) {
				display_time(clock_mode);  // read RTC and display time
				cnt = 0;
				}

		}
		
		// fixme: alarm should not be checked when setting time or alarm
		if (g_alarm_switch && rtc_check_alarm())
			g_alarming = true;

#ifdef FEATURE_WmGPS
		if (g_gps_enabled)
			for (long i = 0; i < 2500; i++) {  // loop count set to create equivalent delay, and works at 9600 bps
				if (gpsDataReady())
					getGPSdata();  // get the GPS serial stream and possibly update the clock 
			}
		else
			_delay_ms(7);  // do something that takes about the same amount of time
#else
		_delay_ms(7);  // roughly 10 ms per loop
#endif

	}  // while (1)
}  // main()
