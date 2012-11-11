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
 * 05nov12 fix DST calc bug
 * speed up alarm check in rtc (not done)
 *
 * 03nov12 fix for ADST falling back
 * change region to "DMY/MDY/YMD"
 *
 * 30oct12 menu changes - single menu block instead of 2
 *
 * 29oct12 "gps_updating" flag, use segment on IV18 to show updates
 * shift time 1 pos for 12 hour display
 *
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
#include <string.h>

#include "Time.h"
#include "display.h"
#include "button.h"
#include "twi.h"
#include "rtc.h"
#include "flw.h"
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
uint8_t EEMEM b_flw_enabled = false;
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
uint8_t EEMEM b_Region = 0;  // default European date format Y/M/D
uint8_t EEMEM b_AutoDate = false;
#endif
// Cached settings
uint8_t g_24h_clock = true;
uint8_t g_show_temp = false;
uint8_t g_show_dots = true;
uint8_t g_brightness = 5;
uint8_t g_volume = 0;
uint8_t g_flw_enabled = false;
#ifdef FEATURE_SET_DATE
uint8_t g_dateyear = 12;
uint8_t g_datemonth = 1;
uint8_t g_dateday = 1;
#endif
#ifdef FEATURE_WmGPS 
uint8_t g_gps_enabled = 0;
uint8_t g_gps_updating = 0;  // for signalling GPS update on some displays
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
uint8_t g_DST_mode;  // DST off, on, auto?
uint8_t g_DST_offset;  // DST offset in Hours
uint8_t g_DST_updated = false;  // DST update flag = allow update only once per day
#endif
#ifdef FEATURE_AUTO_DST
//DST_Rules dst_rules = {{10,1,1,2},{4,1,1,2},1};   // DST Rules for parts of OZ including NSW (for JG)
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
uint16_t g_autodisp = 550;  // how long to display date
#endif

uint8_t g_autotime = 54;  // controls when to display date and when to display time in FLW mode

// Other globals
uint8_t g_has_dots = false; // can current shield show dot (decimal points)
uint8_t g_has_eeprom = false; // set to true if there is a four letter word EEPROM attached
uint8_t g_alarming = false; // alarm is going off
uint8_t g_alarm_switch;
uint16_t g_show_special_cnt = 0;  // display something special ("time", "alarm", etc)
//time_t g_tNow = 0;  // current local date and time as time_t, with DST offset
tmElements_t* tm_; // current local date and time as TimeElements (pointer)
uint8_t alarm_hour = 0, alarm_min = 0, alarm_sec = 0;

extern enum shield_t shield;

void initialize(void)
{
	// read eeprom
	g_24h_clock  = eeprom_read_byte(&b_24h_clock);
	g_show_temp  = eeprom_read_byte(&b_show_temp);
	g_brightness = eeprom_read_byte(&b_brightness);
	g_volume     = eeprom_read_byte(&b_volume);
	g_flw_enabled = eeprom_read_byte(&b_flw_enabled);
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
	g_has_eeprom = has_eeprom();

	if (!g_has_eeprom)
		g_flw_enabled = false;
	
	tm_ = rtc_get_time();	
	if (tm_ && g_has_eeprom)
		seed_random(tm_->Hour * 10000 + tm_->Minute + 100 + tm_->Second);

	rtc_get_alarm_s(&alarm_hour, &alarm_min, &alarm_sec);

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
//	STATE_MENU_DATE,
	STATE_MENU_SETYEAR,
	STATE_MENU_SETMONTH,
	STATE_MENU_SETDAY,
#endif
#ifdef FEATURE_AUTO_DATE
	STATE_MENU_AUTODATE,
	STATE_MENU_REGION,
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
	STATE_MENU_DST,
#endif
#ifdef FEATURE_WmGPS
	STATE_MENU_GPS,
	STATE_MENU_ZONEH,
	STATE_MENU_ZONEM,
#endif
	STATE_MENU_TEMP,
	STATE_MENU_DOTS,
	STATE_MENU_FLW,
	STATE_MENU_LAST,
} menu_state_t;

typedef enum {
	SUB_MENU_OFF = 0,
	SUB_MENU_DATE_YEAR,
	SUB_MENU_DATE_MONTH,
	SUB_MENU_DATE_DAY,
	SUB_MENU_DATE_EXIT,
	SUB_MENU_RULE_SMONTH,
	SUB_MENU_RULE_SDOTW,
	SUB_MENU_RULE_SWEEK,
	SUB_MENU_RULE_SHOUR,
	SUB_MENU_RULE_EMONTH,
	SUB_MENU_RULE_EDOTW,
	SUB_MENU_RULE_EWEEK,
	SUB_MENU_RULE_EHOUR,
	SUB_MENU_RULE_OFFSET,
	SUB_MENU_RULE_EXIT,
} sub_menu_state_t;

menu_state_t menu_state = STATE_CLOCK;
sub_menu_state_t submenu_state = SUB_MENU_OFF;
bool menu_b1_first = false;

// display modes
typedef enum {
	MODE_NORMAL = 0, // normal mode: show time/seconds
	MODE_AMPM, // shows time AM/PM
	MODE_FLW,  // shows four letter words
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

#ifdef FEATURE_AUTO_DATE
char reg_setting_[5];
char* region_setting(uint8_t reg)
{
	switch (reg) {
		case(0):
			strcpy(reg_setting_," dmy");
			break;
		case(1):
			strcpy(reg_setting_," mdy");
			break;
		case(2):
			strcpy(reg_setting_," ymd");
			break;
		default:
			strcpy(reg_setting_," ???");
	}
	return reg_setting_;
}
#endif

#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
void setDSToffset(uint8_t mode) {
	int8_t adjOffset;
	uint8_t newOffset;

#ifdef FEATURE_AUTO_DST
	if (mode == 2) {  // Auto DST
		if (g_DST_updated) return;  // already done it once today
		if (tm_ == NULL) return;  // safet check
		newOffset = getDSToffset(tm_, &dst_rules);  // get current DST offset based on DST Rules
	}
	else
#endif // FEATURE_AUTO_DST
		newOffset = mode;  // 0 or 1
	adjOffset = newOffset - g_DST_offset;  // offset delta
	if (adjOffset == 0)  return;  // nothing to do
	g_DST_updated = true;
	beep(400, 1);  // debugging
	time_t tNow = rtc_get_time_t();  // fetch current time from RTC as time_t
	tNow += adjOffset * SECS_PER_HOUR;  // add or subtract new DST offset
	rtc_set_time_t(tNow);  // adjust RTC
	g_DST_offset = newOffset;
	eeprom_update_byte(&b_DST_offset, g_DST_offset);
	// save DST_updated in ee ???
}
#endif

void display_time(display_mode_t mode)  // (wm)  runs approx every 200 ms
{
	static uint16_t counter = 0;
//	uint8_t hour = 0, min = 0, sec = 0;
	
#ifdef FEATURE_AUTO_DATE
	if (mode == MODE_DATE) {
		show_date(tm_, g_region);  // show date from last rtc_get_time() call
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
	else if (g_show_temp && rtc_is_ds3231() && counter > 125) {
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
		if (tm_->Second%10 == 0)  // check DST Offset every 10 seconds
			setDSToffset(g_DST_mode); 
		if ((tm_->Hour == 0) && (tm_->Minute == 0) && (tm_->Second == 0))
			g_DST_updated = false;
#endif
#ifdef FEATURE_AUTO_DATE
		if (g_autodate && (tm_->Second == g_autotime) ) { 
			save_mode = clock_mode;  // save current mode
			clock_mode = MODE_DATE;  // display date now
			g_show_special_cnt = g_autodisp;  // show date for g_autodisp ms
			scroll_ctr = 0;  // reset scroll position
			}
		else
#endif		

		if (mode == MODE_FLW) {
			if (tm_->Second >= g_autotime - 3 && tm_->Second < g_autotime)
				show_time(tm_, g_24h_clock, 0); // show time briefly each minute
			else
				show_flw(tm_); // otherwise show FLW
		}
		else
			show_time(tm_, g_24h_clock, mode);
	}
	counter++;
	if (counter == 250) counter = 0;
}

#ifdef FEATURE_SET_DATE
void set_date(uint8_t yy, uint8_t mm, uint8_t dd) {
	tm_ = rtc_get_time();  // refresh current time 
	tm_->Year = yy;
	tm_->Month = mm;
	tm_->Day = dd;
	rtc_set_time(tm_);
#ifdef FEATURE_AUTO_DST
	g_DST_updated = false;  // allow automatic DST adjustment again
#endif
}
#endif

void menu(bool update, bool show)
{
	switch (menu_state) {
		case STATE_MENU_BRIGHTNESS:
			if (update) {	
				g_brightness++;
				if (g_brightness > 10) g_brightness = 1;
				eeprom_update_byte(&b_brightness, g_brightness);
				set_brightness(g_brightness);
			}
			show_setting_int("BRIT", "BRITE", g_brightness, show);
			break;
		case STATE_MENU_24H:
			if (update)	{
				g_24h_clock = !g_24h_clock;
				eeprom_update_byte(&b_24h_clock, g_24h_clock);
			}
			show_setting_string("24H", "24H", g_24h_clock ? " on" : " off", show);
			break;
		case STATE_MENU_VOL:
			if (update)	{
				g_volume = !g_volume;
				eeprom_update_byte(&b_volume, g_volume);
				piezo_init();
				beep(1000, 1);
			}
			show_setting_string("VOL", "VOL", g_volume ? " hi" : " lo", show);
			break;
#ifdef FEATURE_SET_DATE						
		case STATE_MENU_SETYEAR:
			if (update) {
				g_dateyear++;
				if (g_dateyear > 29) g_dateyear = 10;  // 20 years...
				set_date(g_dateyear, g_datemonth, g_dateday);
			}
			show_setting_int("YEAR", "YEAR ", g_dateyear, show);
			break;
		case STATE_MENU_SETMONTH:
			if (update) {
				g_datemonth++;
				if (g_datemonth > 12) g_datemonth = 1;  
				set_date(g_dateyear, g_datemonth, g_dateday);
				}
			show_setting_int("MNTH", "MONTH", g_datemonth, show);
			break;
		case STATE_MENU_SETDAY:
			if (update) {
				g_dateday++;
				if (g_dateday > 31) g_dateday = 1;  
				set_date(g_dateyear, g_datemonth, g_dateday);
			}
			show_setting_int("DAY", "DAY  ", g_dateday, show);
			break;
#endif						
#ifdef FEATURE_AUTO_DATE						
		case STATE_MENU_AUTODATE:
			if (update)	{
				g_autodate = !g_autodate;
				eeprom_update_byte(&b_AutoDate, g_autodate);
			}
			show_setting_string("ADTE", "ADATE", g_autodate ? " on " : " off", show);
			break;
		case STATE_MENU_REGION:
			if (update)	{
				g_region = (g_region+1)%3;  // 0 = YMD, 1 = MDY, 2 = DMY
				eeprom_update_byte(&b_Region, g_region);
				}
			show_setting_string("REGN", "REGION", region_setting(g_region), show);
			break;
#endif						
#ifdef FEATURE_WmGPS
		case STATE_MENU_GPS:
			if (update)	{
				g_gps_enabled = (g_gps_enabled+48)%144;  // 0, 48, 96
				eeprom_update_byte(&b_gps_enabled, g_gps_enabled);
				gps_init(g_gps_enabled);  // change baud rate
			}
			show_setting_string("GPS", "GPS", gps_setting(g_gps_enabled), show);
			break;
#endif
#if defined FEATURE_AUTO_DST
		case STATE_MENU_DST:
			if (update) {	
				g_DST_mode = (g_DST_mode+1)%3;  //  0: off, 1: on, 2: auto
				eeprom_update_byte(&b_DST_mode, g_DST_mode);
				g_DST_updated = false;  // allow automatic DST adjustment again
				setDSToffset(g_DST_mode);
			}
			show_setting_string("DST", "DST", dst_setting(g_DST_mode), show);
			break;
#elif defined FEATURE_WmGPS
		case STATE_MENU_DST:
			if (update) {	
				g_DST_mode = (g_DST_mode+1)%2;  //  0: off, 1: on
				eeprom_update_byte(&b_DST_mode, g_DST_mode);
				setDSToffset(g_DST_mode);
			}
			show_setting_string("DST", "DST", g_DST_mode ? " on" : " off", show);
			break;
#endif
#ifdef FEATURE_WmGPS
		case STATE_MENU_ZONEH:
			if (update)	{
				g_TZ_hour++;
				if (g_TZ_hour > 12) g_TZ_hour = -12;
				eeprom_update_byte(&b_TZ_hour, g_TZ_hour + 12);
			}
			show_setting_int("TZ-H", "TZ-H ", g_TZ_hour, show);
			break;
		case STATE_MENU_ZONEM:
			if (update)	{
				g_TZ_minutes = (g_TZ_minutes + 15) % 60;;
				eeprom_update_byte(&b_TZ_minutes, g_TZ_minutes);
			}
			show_setting_int("TZ-M", "TZ-M ", g_TZ_minutes, show);
			break;
#endif
		case STATE_MENU_TEMP:
			if (update)	{
				g_show_temp = !g_show_temp;
				eeprom_update_byte(&b_show_temp, g_show_temp);
			}
			show_setting_string("TEMP", "TEMP", g_show_temp ? " on" : " off", show);
			break;
		case STATE_MENU_DOTS:
			if (update)	{
				g_show_dots = !g_show_dots;
				eeprom_update_byte(&b_show_dots, g_show_dots);
			}
			show_setting_string("DOTS", "DOTS", g_show_dots ? " on" : " off", show);
			break;
		case STATE_MENU_FLW:
			if (update)	{
				g_flw_enabled = !g_flw_enabled;
				eeprom_update_byte(&b_flw_enabled, g_flw_enabled);
			}
				
			show_setting_string("FLW", "FLW", g_flw_enabled ? " on" : " off", show);
			break;
		default:
			break; // do nothing
	}  // switch (menu_state)
}  // menu

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
				if (menu_state == STATE_SET_CLOCK) {
					rtc_set_time_s(time_to_set / 60, time_to_set % 60, 0);
#if defined FEATURE_AUTO_DST
					g_DST_updated = false;  // allow automatic DST adjustment again
#endif
				}
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
			if (get_digits() < 8)  // only set first time flag for 4 or 6 digit displays
				menu_b1_first = true;  // set first time flag
			show_setting_int("BRIT", "BRITE", g_brightness, false);
			buttons.b2_keyup = 0; // clear state
		}
		// Right button toggles display mode
		else if (menu_state == STATE_CLOCK && buttons.b1_keyup) {
			clock_mode++;

			if (clock_mode == MODE_FLW && !g_has_eeprom) clock_mode++;
			if (clock_mode == MODE_FLW && !g_flw_enabled) clock_mode++;

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
			
			if (buttons.b1_keyup) {  // right button
				menu(!menu_b1_first, true);
				buttons.b1_keyup = false;
				menu_b1_first = false;  // b1 not first time now
			}  // if (buttons.b1_keyup) 

			if (buttons.b2_keyup) {  // left button
				if (get_digits() < 8)  // only set first time flag for 4 or 6 digit displays
					menu_b1_first = true;  // reset b1 first time flag
			
				menu_state++;
				
				// show temperature setting only when running on a DS3231
				if (menu_state == STATE_MENU_TEMP && !rtc_is_ds3231()) menu_state++;

				// don't show dots settings for shields that have no dots
				if (menu_state == STATE_MENU_DOTS && !g_has_dots) menu_state++;

				// don't show FLW settings when there is no EEPROM with database
				if (menu_state == STATE_MENU_FLW && !g_has_eeprom) menu_state++;

				if (menu_state == STATE_MENU_LAST) menu_state = STATE_MENU_BRIGHTNESS;
				
				menu(false,false);  
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
		if (g_alarm_switch && rtc_check_alarm_t(tm_))
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
