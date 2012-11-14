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

// Settings saved to eeprom
uint8_t EEMEM b_24h_clock = 1;
uint8_t EEMEM b_show_temp = 0;
uint8_t EEMEM b_show_dots = 1;
uint8_t EEMEM b_brightness = 8;
uint8_t EEMEM b_volume = 0;
#ifdef FEATURE_FLW
uint8_t EEMEM b_flw_enabled = 0;
#endif
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
uint8_t EEMEM b_AutoDate = 0;
#endif
#ifdef FEATURE_AUTO_DIM
uint8_t EEMEM b_AutoDim = 0;
uint8_t EEMEM b_AutoDimHour = 22;
uint8_t EEMEM b_AutoDimLevel = 2;
uint8_t EEMEM b_AutoBrtHour = 7;
uint8_t EEMEM b_AutoBrtLevel = 8;
#endif

menu_values menu_offon[] = { {false, "off"}, {true, "on"} };
menu_values menu_gps[] = { {0, "off"}, {48, "48"}, {96, "96"} };
#if defined FEATURE_AUTO_DST
menu_values menu_adst[] = { {0, "off"}, {1, "on"}, {2, "auto"} };
#endif
const menu_values menu_volume[] = { {0, "lo"}, {1, "hi"} };
menu_values menu_region[] = { {0, "ymd"}, {1, "dmy"}, {2, "mdy"} };

menu_item menuBrt = {MENU_BRIGHTNESS, "BRIT", "BRITE", menu_num, &g_brightness, &b_brightness, 0, 10, {NULL} };
menu_item menu24h = {MENU_24H, "24H", "24H", menu_tf, &g_24h_clock, &b_24h_clock, 0, 2, {menu_offon}};
menu_item menuVol = {MENU_VOL, "VOL", "VOL", menu_list, &g_volume, &b_volume, 0, 2, {menu_volume}};
#ifdef FEATURE_SET_DATE						
menu_item menuYear = {MENU_SETYEAR, "YEAR", "YEAR", menu_num, &g_dateyear, NULL, 10, 29, {NULL}};
menu_item menuMonth = {MENU_SETMONTH, "MNTH", "MONTH", menu_num, &g_datemonth, NULL, 1, 12, {NULL}};
menu_item menuDay = {MENU_SETDAY, "DAY", "DAY", menu_num, &g_dateday, NULL, 1, 31, {NULL}};
#endif
#ifdef FEATURE_AUTO_DATE
menu_item menuAdate = {MENU_AUTODATE, "ADTE", "ADATE", menu_tf, &g_AutoDate, &b_AutoDate, 0, 2, {menu_offon}};
menu_item menuRegion = {MENU_REGION, "REGN", "RGION", menu_list, &g_Region, &b_Region, 0, 3, {menu_region}};
#endif
#ifdef FEATURE_AUTO_DIM
menu_item menuAdim = {MENU_AUTODIM, "ADIM", "ADIM", menu_tf, &g_AutoDim, &b_AutoDim, 0, 2, {menu_offon}};
menu_item menuAdimHr = {MENU_AUTODIM_HOUR,"ADMH", "ADMH", menu_num, &g_AutoDimHour, &b_AutoDimHour, 0, 23, {NULL}};
menu_item menuAdimLvl = {MENU_AUTODIM_LEVEL,"ADML", "ADML", menu_num, &g_AutoDimLevel, &b_AutoDimLevel, 0, 10, {NULL}};
menu_item menuAbrtHr = {MENU_AUTOBRT_HOUR,"ABTH", "ABTH", menu_num, &g_AutoBrtHour, &b_AutoBrtHour, 0, 23, {NULL}};
menu_item menuAbrtLvl = {MENU_AUTOBRT_LEVEL,"ABTL", "ABTL", menu_num, &g_AutoBrtLevel, &b_AutoBrtLevel, 1, 10, {NULL}};
#endif
#if defined FEATURE_AUTO_DST
menu_item menuDST = {MENU_DST, "DST", "DST", menu_list, &g_DST_mode, &b_DST_mode, 0, 3, {menu_adst}};
#elif defined FEATURE_WmGPS
menu_item menuDST = {MENU_DST, "DST", "DST", menu_tf, &g_DST_mode, &b_DST_mode, 0, 2, {menu_offon}};
#endif
#if defined FEATURE_WmGPS
menu_item menuGPS = {MENU_GPS, "GPS", "GPS", menu_list, &g_gps_enabled, &b_gps_enabled, 0, 3, {menu_gps}};
menu_item menuTZh = {MENU_TZH, "TZH", "TZ-H", menu_num, &g_TZ_hour, &b_TZ_hour, -12, 12, {NULL}};
menu_item menuTZm = {MENU_TZM, "TZM", "TZ-M", menu_num, &g_TZ_minutes, &b_TZ_minutes, 0, 59, {NULL}};
#endif
#if defined FEATURE_GPS_DEBUG
menu_item menuGPSc = {MENU_GPSC, "GPSC", "GPSC", menu_num, NULL, NULL, 0, 0, {NULL}};
menu_item menuGPSp = {MENU_GPSP, "GPSP", "GPSP", menu_num, NULL, NULL, 0, 0, {NULL}};
menu_item menuGPSt = {MENU_GPST, "GPST", "GPST", menu_num, NULL, NULL, 0, 0, {NULL}};
#endif
menu_item menuTemp = {MENU_TEMP, "TEMP", "TEMP", menu_tf, &g_show_temp, &b_show_temp, 0, 2, {menu_offon}};
menu_item menuDots = {MENU_DOTS, "DOTS", "DOTS", menu_tf, &g_show_dots, &b_show_dots, 0, 2, {menu_offon}};
#if defined FEATURE_FLW
menu_item menuFLW = {MENU_FLW, "FLW", "FLW", menu_tf, &g_flw_enabled, &b_flw_enabled, 0, 2, {menu_offon}};
#endif

menu_item * menuItems[] = { 
	&menuBrt, &menu24h, &menuVol,
#ifdef FEATURE_SET_DATE
	&menuYear, &menuMonth, &menuDay,
#endif
#ifdef FEATURE_AUTO_DATE
	&menuAdate, &menuRegion,
#endif
#ifdef FEATURE_AUTO_DIM
	&menuAdim, &menuAdimHr, &menuAdimLvl, &menuAbrtHr, &menuAbrtLvl,
#endif
#if defined FEATURE_AUTO_DST || defined FEATURE_WmGPS
	&menuDST,
#endif
#if defined FEATURE_WmGPS
	&menuGPS, &menuTZh, &menuTZm,
#endif
	&menuTemp, &menuDots,
#ifdef FEATURE_FLW
	&menuFLW,
#endif
	NULL};

uint8_t menuIdx = 0;
menu_item * menuPtr;  // current menu item

#if defined FEATURE_AUTO_DST
extern DST_Rules dst_rules;   // initial values from US DST rules as of 2011
const DST_Rules dst_rules_lo = {{1,1,1,0},{1,1,1,0},0};  // low limit
const DST_Rules dst_rules_hi = {{12,7,5,23},{12,7,5,23},1};  // high limit
#endif

extern tmElements_t* tm_; // current local date and time as TimeElements (pointer)

void menu_init(void)
{
//	menuItems[0] = &menuBrt;
	// read eeprom
	g_24h_clock  = eeprom_read_byte(&b_24h_clock);
	g_show_temp  = eeprom_read_byte(&b_show_temp);
	g_show_dots  = eeprom_read_byte(&b_show_dots);
	g_brightness = eeprom_read_byte(&b_brightness);
	g_volume     = eeprom_read_byte(&b_volume);
#ifdef FEATURE_FLW
	g_flw_enabled = eeprom_read_byte(&b_flw_enabled);
#endif
#ifdef FEATURE_WmGPS
	g_gps_enabled = eeprom_read_byte(&b_gps_enabled);
	g_TZ_hour = eeprom_read_byte(&b_TZ_hour) - 12;
	if ((g_TZ_hour<-12) || (g_TZ_hour>12))  g_TZ_hour = 0;  // add range check
	g_TZ_minutes = eeprom_read_byte(&b_TZ_minutes);
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
	g_DST_mode = eeprom_read_byte(&b_DST_mode);
	g_DST_offset = eeprom_read_byte(&b_DST_offset);
	g_DST_updated = false;  // allow automatic DST update
#endif
#ifdef FEATURE_AUTO_DATE
	g_Region = eeprom_read_byte(&b_Region);
	g_AutoDate = eeprom_read_byte(&b_AutoDate);
#endif
#ifdef FEATURE_AUTO_DIM
	g_AutoDim = eeprom_read_byte(&b_AutoDim);
	g_AutoDimHour = eeprom_read_byte(&b_AutoDimHour);
	g_AutoDimLevel = eeprom_read_byte(&b_AutoDimLevel);
	g_AutoBrtHour = eeprom_read_byte(&b_AutoBrtHour);
	g_AutoBrtLevel = eeprom_read_byte(&b_AutoBrtLevel);
#endif
#ifdef FEATURE_SET_DATE
	g_dateyear = 12;
	g_datemonth = 1;
	g_dateday = 1;
#endif
	save_mode = clock_mode = MODE_NORMAL;
	menu_state = STATE_CLOCK;
	menu_update = false;
	menuPtr = menuItems[0];
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

void menu_action(menu_item * menuPtr)
{
	switch(menuPtr->menuNum) {
		case MENU_BRIGHTNESS:
			set_brightness(*menuPtr->setting);
			break;
		case MENU_VOL:
			piezo_init();
			beep(1000, 1);
			break;
		case MENU_SETYEAR:
		case MENU_SETMONTH:
		case MENU_SETDAY:
			set_date(g_dateyear, g_datemonth, g_dateday);
			break;
		case MENU_GPS:
			gps_init(g_gps_enabled);  // change baud rate
			break;
		case MENU_DST:
			g_DST_updated = false;  // allow automatic DST adjustment again
			setDSToffset(g_DST_mode);
			break;
		case MENU_TZH:
		case MENU_TZM:
			tGPSupdate = 0;  // allow GPS to refresh
			break;
	}
}

////				menu_state++;
				// show temperature setting only when running on a DS3231
////				if (menu_state == STATE_MENU_TEMP && !rtc_is_ds3231()) menu_state++;
				// don't show dots settings for shields that have no dots
////				if (menu_state == STATE_MENU_DOTS && !g_has_dots) menu_state++;
#ifdef FEATURE_FLW
				// don't show FLW settings when there is no EEPROM with database
////				if (menu_state == STATE_MENU_FLW && !g_has_eeprom) menu_state++;
#endif
////				if (menu_state == STATE_MENU_LAST) menu_state = STATE_CLOCK;  //wbp

void menu(uint8_t n, uint8_t update, uint8_t show)
{
	switch (n) {
		case 0:
			menuIdx = 0;  // restart menu
			menuPtr = menuItems[0];
			break;
		case 1:  // show/update current item value
			break;
		case 2:
			menuIdx++;  // next menu item
			menuPtr = menuItems[menuIdx];
			if (menuPtr == NULL) {
				menu_state = STATE_CLOCK;
				return;
			}
			break;
	}
	char * valstr = "---";
	int valnum;
	valnum = *(menuPtr->setting);
	const menu_values * menuValues;
	menuValues = *menuPtr->menuList;
	uint8_t idx = 0;
	switch (menuPtr->menuType) {
		case menu_num:
			if ((n == 1) && update) {
				valnum++;
				if (valnum > menuPtr->hiLimit)
					valnum = menuPtr->loLimit;
				*menuPtr->setting = valnum;
				if (menuPtr->eeAddress != NULL) {
					if (menuPtr->menuNum == MENU_TZH)
						eeprom_update_byte(menuPtr->eeAddress, valnum+12);
					else
						eeprom_update_byte(menuPtr->eeAddress, valnum);
					}
				menu_action(menuPtr);
			}
			show_setting_int(menuPtr->shortName, menuPtr->longName, valnum, show);
			break;
		case menu_tf:  // true/false (or really, false/true)
			if ((n == 1) && update) {
				valnum = !valnum;
				*menuPtr->setting = valnum;
				if (menuPtr->eeAddress != NULL) 
					eeprom_update_byte(menuPtr->eeAddress, valnum);
				menu_action(menuPtr);
			}
			if (valnum)
				valstr = (char*)menuValues[1].valName;  // true
			else
				valstr = (char*)menuValues[0].valName;  // false
			show_setting_string(menuPtr->shortName, menuPtr->longName, valstr, show);
			break;
		case menu_list:
			for (uint8_t i=0;i<menuPtr->hiLimit;i++) {
				if (menuValues[i].value == valnum) {
					idx = i;
					}
			}
			valstr = (char*)menuValues[idx].valName;
			if ((n == 1) && update) {
				idx++;
				if (idx >= menuPtr->hiLimit)  // for lists, hilimit is the # of elements!
					idx = 0;
				valnum = menuValues[idx].value;
				valstr = (char*)menuValues[idx].valName;
				*menuPtr->setting = valnum;
				if (menuPtr->eeAddress != NULL) 
					eeprom_update_byte(menuPtr->eeAddress, valnum);
				menu_action(menuPtr);
				//// store new value in EE
			}
			show_setting_string(menuPtr->shortName, menuPtr->longName, valstr, show);
			break;
		case menu_sub:  // sub menu item, just show name, right button will select
			show_setting_int(menuPtr->shortName, menuPtr->longName, 0, false);
			break;
	}
#ifdef oldmenu
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
#endif
}  // menu
