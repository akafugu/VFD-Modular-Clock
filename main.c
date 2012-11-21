/*
 * VFD Modular Clock
 * (C) 2011-2012 Akafugu Corporation
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
*todo:
 * ?
 *
 * 19nov12 redo main loop timing now that GPS read is interrupt driven
 * 18nov12 move FEATURE_ADIM to Makefile, cleanup FEATURE ifdefs
 * 17nov12 right-align string displays for 4 digit display
 *  tested wtih all 3 displays
 * 16nov12 - reduced ram 
 *  sub sub menu (for DST Rules)
 *  add tick for button push in menu
 * 15nov12 - fix bug when setting dst auto
 * 14nov12 table driven Menu
 * 12nov12 add Auto dim/brt feature
 * 11nov12 interrupt driven GPS read
 *  add local FEATURE_GPS_DEBUG to control if gps debug counters are in menu
 * 10nov12 add gps error counters to menu
 * 09nov12 rewrite GPS parse
 * 08nov12 auto menu feature
 * 07nov12 check GPS time vs. previous, add GPS error counter
 * 06nov12 rtc: separate alarm time, adst: add DSTinit function
 * 05nov12 fix DST calc bug
 *  speed up alarm check in rtc (not done)
 * 03nov12 fix for ADST falling back
 *  change region to "DMY/MDY/YMD"
 * 30oct12 menu changes - single menu block instead of 2
 * 29oct12 "gps_updating" flag, use segment on IV18 to show updates
 *  shift time 1 pos for 12 hour display
 * 25oct12 implement Auto DST
 * 24oct12 fix date display for larger displays
 * 22oct12 add DATE to menu, add FEATURE_SET_DATE and FEATURE_AUTO_DATE
 *  added to menu: YEAR, MONTH, DAY
 * 12oct12 fix blank display when setting time (blink)
 *  return to AMPM after AutoDate display
 * 05oct12 clean up GPS mods for commit
 *  add get/set time in time_t to RTC
 *  slight blink when RTC updated from GPS to show signal reception
 * 26sap12
 *  fix EUR date display
 *  add GPS 48, 96 (support 9600 baud GPS) 
 * 25sep12 improve date scrolling
 *  add Time.h/Time.c, use tmElements_t structure instead of tm
 *  add '/' to font-16seg.c
 * 24sep12 - add time zone & DST menu items
 *  show current value without change on 1st press of b1
 *  scroll the date for smaller displays
 * 21sep12 - GPS support
 * 02sep12 - post to Github
 * 28aug12 - clean up display multiplex code
 * 27aug12 - 10 step brightness for IV-17
 * 26aug12 - display "off" after "alarm" if alarm switch off
 * 25aug12 - change Menu timeout to ~2 seconds
 *  show Alarm time when Alarm switch is set to enable the alarm
 *  get time from RTC every 200 ms instead of every 150 ms
 *  blank hour leading zero if not 24 hr display mode
 *  minor typos & cleanup
 */

//#define FEATURE_AUTO_MENU  // temp
#define FEATURE_GPS_DEBUG  // enables GPS debugging counters & menu items
//#define FEATURE_AUTO_DIM  // moved to Makefile
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
//#include <avr/eeprom.h>
#include <avr/wdt.h>     // Watchdog timer to repair lockups

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "Time.h"
#include "globals.h"
#include "menu.h"
#include "display.h"
#include "button.h"
#include "twi.h"
#include "rtc.h"
#ifdef FEATURE_FLW
#include "flw.h"
#endif
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

// Cached settings
#ifdef FEATURE_AUTO_DST
//DST_Rules dst_rules = {{10,1,1,2},{4,1,1,2},1};   // DST Rules for parts of OZ including NSW (for JG)
//DST_Rules dst_rules = {{3,1,2,2},{11,1,1,2},1};   // initial values from US DST rules as of 2011
//uint8_t dst_rules[] = {3,1,2,2,11,1,1,2,1};   // initial values from US DST rules as of 2011
// DST Rules: Start(month, dotw, week, hour), End(month, dotw, week, hour), Offset
// DOTW is Day of the Week, 1=Sunday, 7=Saturday
// N is which occurrence of DOTW
// Current US Rules:  March, Sunday, 2nd, 2am, November, Sunday, 1st, 2 am, 1 hour
#endif

#define MENU_TIMEOUT 20  // 2.0 seconds
#ifdef FEATURE_AUTO_MENU
#define BUTTON2_TIMEOUT 100
#endif

#ifdef FEATURE_AUTO_DATE
uint16_t g_autodisp = 50;  // how long to display date 5.0 seconds
#endif
uint8_t g_autotime = 54;  // controls when to display date and when to display time in FLW mode

uint8_t g_alarming = false; // alarm is going off
uint8_t g_alarm_switch;
uint16_t g_show_special_cnt = 0;  // display something special ("time", "alarm", etc)
//time_t g_tNow = 0;  // current local date and time as time_t, with DST offset
tmElements_t* tm_; // current local date and time as TimeElements (pointer)
uint8_t alarm_hour = 0, alarm_min = 0, alarm_sec = 0;

extern enum shield_t shield;

void initialize(void)
{

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

	piezo_init();
	beep(440, 1);
	globals_init();
	display_init(g_brightness);

	g_alarm_switch = get_alarm_switch();
	
#ifdef FEATURE_FLW
	g_has_eeprom = has_eeprom();
	if (!g_has_eeprom)
		g_flw_enabled = false;
	if (tm_ && g_has_eeprom)
		seed_random(tm_->Hour * 10000 + tm_->Minute + 100 + tm_->Second);
#endif

	beep(1320, 1);
	menu_init();  // must come after display, flw init
	rtc_get_alarm_s(&alarm_hour, &alarm_min, &alarm_sec);

	// set up interrupt for alarm switch
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18);
	
#ifdef FEATURE_WmGPS
  // setup uart for GPS
	gps_init(g_gps_enabled);
#endif
#ifdef FEATURE_AUTO_DST
	if (g_DST_mode)
		DSTinit(tm_, g_DST_Rules);  // re-compute DST start, end	
#endif
}

// display modes
typedef enum {
	MODE_NORMAL = 0, // normal mode: show time/seconds
	MODE_AMPM, // shows time AM/PM
#ifdef FEATURE_FLW
	MODE_FLW,  // shows four letter words
#endif
#ifdef FEATURE_AUTO_DATE
	MODE_DATE, // shows date
#endif
	MODE_LAST,  // end of display modes for right button pushes
	MODE_ALARM_TEXT,  // show "alarm" (wm)
	MODE_ALARM_TIME,  // show alarm time (wm)
} display_mode_t;
struct BUTTON_STATE buttons;

display_mode_t clock_mode = MODE_NORMAL;
display_mode_t save_mode = MODE_NORMAL;  // for restoring mode after autodate display
//uint8_t menu_update = false;

// Alarm switch changed interrupt
ISR( PCINT2_vect )
{
	if ( (SWITCH_PIN & _BV(SWITCH_BIT)) == 0)
		g_alarm_switch = false;
	else {
		g_alarm_switch = true;
		}
	g_show_special_cnt = 10;  // show alarm text for 1 second
	if (get_digits() == 8)
		clock_mode = MODE_ALARM_TIME;
	else
		clock_mode = MODE_ALARM_TEXT;
}

uint8_t scroll_ctr;
void display_time(display_mode_t mode)  // (wm)  runs approx every 100 ms
{
	static uint16_t counter = 0;
#ifdef FEATURE_AUTO_DATE
	if (mode == MODE_DATE) {
		show_date(tm_, g_Region, (scroll_ctr++) * 10 / 38);  // show date from last rtc_get_time() call
	}
	else
#endif	
	if (mode == MODE_ALARM_TEXT) {
		show_alarm_text();
	}
	else if (mode == MODE_ALARM_TIME) {
		if (g_alarm_switch) {
			rtc_get_alarm_s(&alarm_hour, &alarm_min, &alarm_sec);
			show_alarm_time(alarm_hour, alarm_min, 0);
		}
		else {
			show_alarm_off();
		}
	}
// alternate temp and time every 2.5 seconds
	else if (g_show_temp && rtc_is_ds3231() && counter > 250) {
		int8_t t;
		uint8_t f;
		ds3231_get_temp_int(&t, &f);
		show_temp(t, f);
	}
	else {
		tm_ = rtc_get_time();
		if (tm_ == NULL) return;
#ifdef FEATURE_SET_DATE
		g_dateyear = tm_->Year;  // save year for Menu
		g_datemonth = tm_->Month;  // save month for Menu
		g_dateday = tm_->Day;  // save day for Menu
#endif
#ifdef FEATURE_AUTO_DST
		if (tm_->Second%10 == 0)  // check DST Offset every 10 seconds (60?)
			setDSToffset(g_DST_mode); 
		if ((tm_->Hour == 0) && (tm_->Minute == 0) && (tm_->Second == 0)) {  // MIDNIGHT!
			g_DST_updated = false;
			if (g_DST_mode)
				DSTinit(tm_, g_DST_Rules);  // re-compute DST start, end
		}
#endif
#ifdef FEATURE_AUTO_DATE
		if (g_AutoDate && (tm_->Second == g_autotime) ) { 
			save_mode = clock_mode;  // save current mode
			clock_mode = MODE_DATE;  // display date now
			g_show_special_cnt = g_autodisp;  // show date for g_autodisp ms
			scroll_ctr = 0;  // reset scroll position
			}
		else
#endif		
#ifdef FEATURE_FLW
		if (mode == MODE_FLW) {
			if ((tm_->Second >= g_autotime - 3) && (tm_->Second < g_autotime))
				show_time(tm_, g_24h_clock, 0); // show time briefly each minute
			else
				show_flw(tm_); // otherwise show FLW
		}
		else
#endif
			show_time(tm_, g_24h_clock, mode);
	}
	counter++;
	if (counter == 500) counter = 0;
}

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
#ifdef FEATURE_AUTO_MENU
  uint16_t button2_release_timer = 0;
#endif
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

//	piezo_init();
//	beep(440, 1);
//	beep(1320, 1);
	beep(440, 1);

	_delay_ms(500);
	//set_string("--------");

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
				rtc_get_alarm_s(&alarm_hour, &alarm_min, &alarm_sec);
				time_to_set = alarm_hour*60 + alarm_min;
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
			if (button_released_timer >= MENU_TIMEOUT) {
				set_blink(false);
				button_released_timer = 0;
				button_speed = 1;
				
				// fixme: should be different in 12h mode
				if (menu_state == STATE_SET_CLOCK) {
					rtc_set_time_s(time_to_set / 60, time_to_set % 60, 0);
#if defined FEATURE_AUTO_DST
					g_DST_updated = false;  // allow automatic DST adjustment again
#endif
				}
				else {
					alarm_hour = time_to_set / 60;
					alarm_min  = time_to_set % 60;
					alarm_sec  = 0;
					rtc_set_alarm_s(alarm_hour, alarm_min, alarm_sec);
				}

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
		else if (menu_state == STATE_CLOCK && buttons.b2_keyup) {  // Left button enters menu
			menu_state = STATE_MENU;
//			if (get_digits() < 8)  // only set first time flag for 4 or 6 digit displays
//				menu_update = false;  // set first time flag
//			show_setting_int("BRIT", "BRITE", g_brightness, false);
      menu(0);  // show first menu item 
			buttons.b2_keyup = 0; // clear state
#ifdef FEATURE_AUTO_MENU
      button2_release_timer = BUTTON2_TIMEOUT;  // start button 2 timer
#endif      
		}
		// Right button toggles display mode
		else if (menu_state == STATE_CLOCK && buttons.b1_keyup) {
			clock_mode++;

#ifdef FEATURE_FLW
			if (clock_mode == MODE_FLW && !g_has_eeprom) clock_mode++;
			if (clock_mode == MODE_FLW && !g_flw_enabled) clock_mode++;
#endif

#ifdef FEATURE_AUTO_DATE
			if (clock_mode == MODE_DATE) {
				g_show_special_cnt = g_autodisp;  // show date for g_autodisp ms
				scroll_ctr = 0;  // reset scroll position
			}
#endif			
			if (clock_mode == MODE_LAST) clock_mode = MODE_NORMAL;
			buttons.b1_keyup = 0; // clear state
		}
		else if (menu_state == STATE_MENU) {
			if (buttons.none_held)
				button_released_timer++;
			else
				button_released_timer = 0;
//			if (button_released_timer >= 80) {
			if (button_released_timer >= MENU_TIMEOUT) {  
				button_released_timer = 0;
#ifdef FEATURE_AUTO_MENU
        button2_release_timer = 0;
#endif
				menu_state = STATE_CLOCK;
			}

#ifdef FEATURE_AUTO_MENU
      if (button2_release_timer > 0) {
        button2_release_timer--;
        if (button2_release_timer == 0) {
          menu(3);  // show or update current menu item value
        }
      }
#endif
			
			if (buttons.b1_keyup) {  // right button
#ifdef FEATURE_AUTO_MENU
        button2_release_timer = 0;  // cancel button 2 timer
#endif
				menu(1);  // right button
				buttons.b1_keyup = false;
//				menu_update = true;  // b1 not first time now
			}  // if (buttons.b1_keyup) 

			if (buttons.b2_keyup) {  // left button
//				if (get_digits() < 8)  // only set first time flag for 4 or 6 digit displays
//					menu_update = false;  // reset b1 first time flag
#ifdef FEATURE_AUTO_MENU
        button2_release_timer = BUTTON2_TIMEOUT;  // restart button2 timer
#endif
				
				menu(2);  // left button
				buttons.b2_keyup = 0; // clear state
			}
		}
		else {
			if (g_show_special_cnt>0) {
				g_show_special_cnt--;
				if (g_show_special_cnt == 0) {
					switch (clock_mode) {
						case MODE_ALARM_TEXT:
							clock_mode = MODE_ALARM_TIME;
							g_show_special_cnt = 10;
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
			}

			// read RTC & update display approx every 100ms  (wm)
			display_time(clock_mode);  // read RTC and display time

		}
		
		// fixme: alarm should not be checked when setting time or alarm
		// fixme: alarm will be missed if time goes by Second=0 while in menu
		if (g_alarm_switch && rtc_check_alarm_cached(tm_, alarm_hour, alarm_min, alarm_sec))
			g_alarming = true;

#ifdef FEATURE_AUTO_DIM			
		if ((g_AutoDim) && (tm_->Minute == 0) && (tm_->Second == 0))  {  // Auto Dim enabled?
			if (tm_->Hour == g_AutoDimHour)
				set_brightness(g_AutoDimLevel);
			else if (tm_->Hour == g_AutoBrtHour)
				set_brightness(g_AutoBrtLevel);
		}
#endif

#ifdef FEATURE_WmGPS
		if (g_gps_enabled) {
			if (gpsDataReady()) {
				parseGPSdata(gpsNMEA());  // get the GPS serial stream and possibly update the clock 
				}
			else
				_delay_ms(2);
			}
		else
			_delay_ms(2);
#endif

		_delay_ms(74);  // tuned so loop runs 10 times a second

		}  // while (1)
}  // main()
