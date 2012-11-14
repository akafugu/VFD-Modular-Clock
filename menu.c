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
#include "globals.h"
#include "menu.h"
#include "display.h"
#include "piezo.h"
#include "gps.h"
#include "adst.h"

menu_values menu_offon[] = { {false, "off"}, {true, "on"} };
menu_values menu_gps[] = { {0, "off"}, {48, "48"}, {96, "96"} };
#if defined FEATURE_AUTO_DST
menu_values menu_adst[] = { {0, "off"}, {1, "on"}, {2, "auto"} };
#endif
const menu_values menu_volume[] = { {0, "lo"}, {1, "hi"} };
menu_values menu_region[] = { {0, "ymd"}, {1, "dmy"}, {2, "mdy"} };

menu_item menuBrt = {MENU_BRIGHTNESS, menu_noflags,"BRIT","BRITE",menu_num,&g_brightness,&b_brightness,0,10,{NULL} };
menu_item menu24h = {MENU_24H,menu_noflags,"24H","24H",menu_tf,&g_24h_clock,&b_24h_clock,0,2,{menu_offon}};
menu_item menuVol = {MENU_VOL,menu_noflags,"VOL","VOL",menu_list,&g_volume,&b_volume,0,2,{menu_volume}};
#ifdef FEATURE_SET_DATE						
menu_item menuYear = {MENU_SETYEAR,menu_noflags,"YEAR","YEAR",menu_num,&g_dateyear,NULL,10,29,{NULL}};
menu_item menuMonth = {MENU_SETMONTH,menu_noflags,"MNTH","MONTH",menu_num,&g_datemonth,NULL,1,12,{NULL}};
menu_item menuDay = {MENU_SETDAY,menu_noflags,"DAY","DAY",menu_num,&g_dateday,NULL,1,31,{NULL}};
#endif
#ifdef FEATURE_AUTO_DATE
menu_item menuAdate = {MENU_AUTODATE,menu_noflags,"ADTE","ADATE",menu_tf,&g_AutoDate,&b_AutoDate,0,2,{menu_offon}};
menu_item menuRegion = {MENU_REGION,menu_noflags,"REGN","RGION",menu_list,&g_Region,&b_Region,0,3,{menu_region}};
#endif
#ifdef FEATURE_AUTO_DIM
menu_item menuAdim = {MENU_AUTODIM,menu_noflags,"ADIM","ADIM",menu_tf,&g_AutoDim,&b_AutoDim,0,2,{menu_offon}};
menu_item menuAdimHr = {MENU_AUTODIM_HOUR,menu_noflags,"ADMH","ADMH",menu_num,&g_AutoDimHour,&b_AutoDimHour,0,23,{NULL}};
menu_item menuAdimLvl = {MENU_AUTODIM_LEVEL,menu_noflags,"ADML","ADML",menu_num,&g_AutoDimLevel,&b_AutoDimLevel,0,10,{NULL}};
menu_item menuAbrtHr = {MENU_AUTOBRT_HOUR,menu_noflags,"ABTH","ABTH",menu_num,&g_AutoBrtHour,&b_AutoBrtHour,0,23,{NULL}};
menu_item menuAbrtLvl = {MENU_AUTOBRT_LEVEL,menu_noflags,"ABTL","ABTL",menu_num,&g_AutoBrtLevel,&b_AutoBrtLevel,1,10,{NULL}};
#endif
#if defined FEATURE_AUTO_DST
menu_item menuDST = {MENU_DST,menu_noflags,"DST","DST",menu_list,&g_DST_mode,&b_DST_mode,0,3,{menu_adst}};
#elif defined FEATURE_WmGPS
menu_item menuDST = {MENU_DST,menu_noflags,"DST","DST",menu_tf,&g_DST_mode,&b_DST_mode,0,2,{menu_offon}};
#endif
#if defined FEATURE_WmGPS
menu_item menuGPS = {MENU_GPS,menu_noflags,"GPS","GPS",menu_list,&g_gps_enabled,&b_gps_enabled,0,3,{menu_gps}};
menu_item menuTZh = {MENU_TZH,menu_noflags,"TZH","TZ-H",menu_num,&g_TZ_hour,&b_TZ_hour,-12,12,{NULL}};
menu_item menuTZm = {MENU_TZM,menu_noflags,"TZM","TZ-M",menu_num,&g_TZ_minute,&b_TZ_minute,0,59,{NULL}};
#endif
#if defined FEATURE_GPS_DEBUG
menu_item menuGPSc = {MENU_GPSC,menu_noflags,"GPSC","GPSC",menu_num,NULL,NULL,0,0,{NULL}};
menu_item menuGPSp = {MENU_GPSP,menu_noflags,"GPSP","GPSP",menu_num,NULL,NULL,0,0,{NULL}};
menu_item menuGPSt = {MENU_GPST,menu_noflags,"GPST","GPST",menu_num,NULL,NULL,0,0,{NULL}};
#endif
menu_item menuTemp = {MENU_TEMP,menu_noflags,"TEMP","TEMP",menu_tf,&g_show_temp,&b_show_temp,0,2,{menu_offon}};
menu_item menuDots = {MENU_DOTS,menu_noflags,"DOTS","DOTS",menu_tf,&g_show_dots,&b_show_dots,0,2,{menu_offon}};
#if defined FEATURE_FLW
menu_item menuFLW = {MENU_FLW,menu_noflags,"FLW","FLW",menu_tf,&g_flw_enabled,&b_flw_enabled,0,2,{menu_offon}};
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
	NULL};  // end of list marker must be here

uint8_t menuIdx = 0;

#if defined FEATURE_AUTO_DST
extern DST_Rules dst_rules;   // initial values from US DST rules as of 2011
const DST_Rules dst_rules_lo = {{1,1,1,0},{1,1,1,0},0};  // low limit
const DST_Rules dst_rules_hi = {{12,7,5,23},{12,7,5,23},1};  // high limit
#endif

extern tmElements_t* tm_; // current local date and time as TimeElements (pointer)
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

void menu_enable(menu_number num, uint8_t enable)
{
	menu_item * menuPtr = NULL;  // current menu item
	for (uint8_t i=0;i<MENU_LAST;i++) {
		menuPtr = menuItems[i];
		if (num == menuPtr->menuNum) {
			if (enable)
				menuPtr->menuFlags &= menu_disabled;
			else
				menuPtr->menuFlags |= menu_disabled;
		}
	}
}

void menu(uint8_t n, uint8_t update, uint8_t show)
{
	menu_item * menuPtr = menuItems[menuIdx];  // current menu item
	switch (n) {
		case 0:
			menuIdx = 0;  // restart menu
			menuPtr = menuItems[menuIdx];
			break;
		case 1:  // show/update current item value
			break;
		case 2:
			menuIdx++;  // next menu item
			menuPtr = menuItems[menuIdx];
			while ((menuPtr != NULL) && (menuPtr->menuFlags & menu_disabled)) {
				menuIdx++;  // next menu item
				menuPtr = menuItems[menuIdx];
			}
			if (menuPtr == NULL) {
				menuIdx = 0;
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
}  // menu

void menu_init(void)
{
//	menuItems[0] = &menuBrt;
	menu_state = STATE_CLOCK;
	menu_update = false;
	menuIdx = 0;
	menu_enable(MENU_TEMP, rtc_is_ds3231());  // show temperature setting only when running on a DS3231
	menu_enable(MENU_DOTS, g_has_dots);  // don't show dots settings for shields that have no dots
#ifdef FEATURE_FLW
	menu_enable(MENU_FLW, g_has_eeprom);  // don't show FLW settings when there is no EEPROM with database
#endif
}
