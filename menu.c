/*
 * Menu for VFD Modular Clock
 * (C) 2012 William B Phelps
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

#define FEATURE_AUTO_MENU  // temp
#define FEATURE_GPS_DEBUG  // enables GPS debugging counters & menu items
#define FEATURE_AUTO_DIM  // temp

#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "display.h"
#include "piezo.h"
#include "gps.h"
#include "adst.h"

extern uint8_t b_24h_clock;
extern uint8_t b_show_temp;
extern uint8_t b_show_dots;
extern uint8_t b_brightness;
extern uint8_t b_volume;
#ifdef FEATURE_FLW
extern uint8_t b_flw_enabled;
#endif
#ifdef FEATURE_WmGPS
extern uint8_t b_gps_enabled;  // 0, 48, or 96 - default no gps
extern uint8_t b_TZ_hour;
extern uint8_t b_TZ_minutes;
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
extern uint8_t b_DST_mode;  // 0: off, 1: on, 2: Auto
extern uint8_t b_DST_offset;
#endif
#ifdef FEATURE_AUTO_DATE
extern uint8_t b_Region;  // default European date format Y/M/D
extern uint8_t b_AutoDate;
#endif
#ifdef FEATURE_AUTO_DIM
extern uint8_t b_AutoDim;
extern uint8_t b_AutoDimHour;
extern uint8_t b_AutoDimLevel;
extern uint8_t b_AutoBrtHour;
extern uint8_t b_AutoBrtLevel;
#endif

extern uint8_t g_24h_clock;
extern uint8_t g_show_temp;
extern uint8_t g_show_dots;
extern uint8_t g_brightness;
extern uint8_t g_volume;
#ifdef FEATURE_FLW
extern uint8_t g_flw_enabled;
#endif
#ifdef FEATURE_SET_DATE
extern uint8_t g_dateyear;
extern uint8_t g_datemonth;
extern uint8_t g_dateday;
#endif
#ifdef FEATURE_WmGPS 
extern uint8_t g_gps_enabled;
extern uint8_t g_gps_updating;  // for signalling GPS update on some displays
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
extern uint8_t g_DST_mode;  // DST off, on, auto?
extern uint8_t g_DST_offset;  // DST offset in Hours
extern uint8_t g_DST_updated;  // DST update flag = allow update only once per day
#endif
#ifdef FEATURE_AUTO_DATE
extern uint8_t g_region;
extern uint8_t g_autodate;
#endif
#ifdef FEATURE_AUTO_DIM
extern uint8_t g_AutoDim;
extern uint8_t g_AutoDimHour;
extern uint8_t g_AutoDimLevel;
extern uint8_t g_AutoBrtHour;
extern uint8_t g_AutoBrtLevel;
#endif
#if defined FEATURE_AUTO_DST
extern DST_Rules dst_rules;   // initial values from US DST rules as of 2011
const DST_Rules dst_rules_lo = {{1,1,1,0},{1,1,1,0},0};  // low limit
const DST_Rules dst_rules_hi = {{12,7,5,23},{12,7,5,23},1};  // high limit
#endif

extern tmElements_t* tm_; // current local date and time as TimeElements (pointer)

void menu_init(void)
{
	save_mode = clock_mode = MODE_NORMAL;
	menu_state = STATE_CLOCK;
	submenu_state = SUB_MENU_OFF;
	menu_update = false;
}

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
	if (adjOffset > 0)
		beep(880, 1);  // spring ahead
	else
		beep(440, 1);  // fall back
	time_t tNow = rtc_get_time_t();  // fetch current time from RTC as time_t
	tNow += adjOffset * SECS_PER_HOUR;  // add or subtract new DST offset
	rtc_set_time_t(tNow);  // adjust RTC
	g_DST_offset = newOffset;
	eeprom_update_byte(&b_DST_offset, g_DST_offset);
	g_DST_updated = true;
	// save DST_updated in ee ???
}
#endif

#ifdef FEATURE_SET_DATE
void set_date(uint8_t yy, uint8_t mm, uint8_t dd) {
	tm_ = rtc_get_time();  // refresh current time 
	tm_->Year = yy;
	tm_->Month = mm;
	tm_->Day = dd;
	rtc_set_time(tm_);
#ifdef FEATURE_AUTO_DST
	DSTinit(tm_, &dst_rules);  // re-compute DST start, end for new date
	g_DST_updated = false;  // allow automatic DST adjustment again
	setDSToffset(g_DST_mode);  // set DSToffset based on new date
#endif
}
#endif

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

void menu(uint8_t update, uint8_t show)
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
#ifdef FEATURE_AUTO_DIM
		case STATE_MENU_AUTODIM:
			if (update)	{
				g_AutoDim = !g_AutoDim;
				eeprom_update_byte(&b_AutoDim, g_AutoDim);
			}
			show_setting_string("ADIM", "ADIM", g_AutoDim ? " on " : " off", show);
			break;
		case STATE_MENU_AUTODIM_HOUR:
			if (update)	{
				g_AutoDimHour++;
				if (g_AutoDimHour > 23) g_AutoDimHour = 0;
				eeprom_update_byte(&b_AutoDimHour, g_AutoDimHour);
			}
			show_setting_int("ADMH", "ADIMH", g_AutoDimHour, show);
			break;
		case STATE_MENU_AUTODIM_LEVEL:
			if (update)	{
				g_AutoDimLevel++;
				if (g_AutoDimLevel > 10) g_AutoDimLevel = 1;  // level 0 for blank???
				eeprom_update_byte(&b_AutoDimLevel, g_AutoDimLevel);
			}
			show_setting_int("ADML", "ADIML", g_AutoDimLevel, show);
			break;
		case STATE_MENU_AUTOBRT_HOUR:
			if (update)	{
				g_AutoBrtHour++;
				if (g_AutoBrtHour > 23) g_AutoBrtHour = 0;
				eeprom_update_byte(&b_AutoBrtHour, g_AutoBrtHour);
			}
			show_setting_int("ABTH", "ABRTH", g_AutoBrtHour, show);
			break;
		case STATE_MENU_AUTOBRT_LEVEL:
			if (update)	{
				g_AutoBrtLevel++;
				if (g_AutoBrtLevel > 10) g_AutoBrtLevel = 1;  // level 0 for blank???
				eeprom_update_byte(&b_AutoBrtLevel, g_AutoBrtLevel);
			}
			show_setting_int("ABTL", "ABRTL", g_AutoBrtLevel, show);
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
#ifdef FEATURE_GPS_DEBUG
		case STATE_MENU_GPSC:
			if (update)	{
				g_gps_cks_errors = 0;  // reset error counter
			}
			show_setting_int4("GPSC", "GPSC", g_gps_cks_errors, show);
			break;
		case STATE_MENU_GPSP:
			if (update)	{
				g_gps_parse_errors = 0;  // reset error counter
			}
			show_setting_int4("GPSP", "GPSP", g_gps_parse_errors, show);
			break;
		case STATE_MENU_GPST:
			if (update)	{
				g_gps_time_errors = 0;  // reset error counter
			}
			show_setting_int4("GPST", "GPST", g_gps_time_errors, show);
			break;
#endif
		case STATE_MENU_ZONEH:
			if (update)	{
				g_TZ_hour++;
				if (g_TZ_hour > 12) g_TZ_hour = -12;
				eeprom_update_byte(&b_TZ_hour, g_TZ_hour + 12);
				tGPSupdate = 0;  // allow GPS to refresh
			}
			show_setting_int("TZH", "TZH", g_TZ_hour, show);
			break;
		case STATE_MENU_ZONEM:
			if (update)	{
				g_TZ_minutes = (g_TZ_minutes + 15) % 60;;
				eeprom_update_byte(&b_TZ_minutes, g_TZ_minutes);
				tGPSupdate = 0;  // allow GPS to refresh
			}
			show_setting_int("TZM", "TZM", g_TZ_minutes, show);
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
#ifdef FEATURE_FLW
		case STATE_MENU_FLW:
			if (update)	{
				g_flw_enabled = !g_flw_enabled;
				eeprom_update_byte(&b_flw_enabled, g_flw_enabled);
			}
			show_setting_string("FLW", "FLW", g_flw_enabled ? " on" : " off", show);
			break;
#endif
		default:
			break; // do nothing
	}  // switch (menu_state)
}  // menu
