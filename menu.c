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
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>
#include "globals.h"
#include "menu.h"
#include "display.h"
#include "piezo.h"
#include "gps.h"
#include "adst.h"

menu_value menu_offon[2] = { {false, "off"}, {true, "on"} };
menu_value menu_gps[3] = { {0, "off"}, {48, "48"}, {96, "96"} };
#if defined FEATURE_AUTO_DST
menu_value menu_adst[3] = { {0, "off"}, {1, "on"}, {2, "auto"} };
#endif
menu_value menu_volume[2] = { {0, "lo"}, {1, "hi"} };
menu_value menu_region[3] = { {0, "dmy"}, {1, "mdy"}, {2, "ymd"} };

menu_item menu24h = {MENU_24H,menu_tf,"24H","24H",&g_24h_clock,&b_24h_clock,0,2,{menu_offon}};
menu_item menuBrt = {MENU_BRIGHTNESS,menu_num,"BRIT","BRITE",&g_brightness,&b_brightness,0,10,{NULL} };
#ifdef FEATURE_AUTO_DATE
menu_item menuAdate_ = {MENU_AUTODATE,menu_hasSub,"ADT=","ADATE",NULL,NULL,0,0,{NULL}};
menu_item menuAdate = {MENU_AUTODATE,menu_tf+menu_isSub,"ADTE","ADATE",&g_AutoDate,&b_AutoDate,0,2,{menu_offon}};
menu_item menuRegion = {MENU_REGION,menu_list+menu_isSub,"REGN","RGION",&g_Region,&b_Region,0,3,{menu_region}};
#endif
#ifdef FEATURE_AUTO_DIM
menu_item menuAdim_ = {MENU_AUTODIM,menu_hasSub,"ADM=","ADIM ",NULL,NULL,0,0,{NULL}};
menu_item menuAdim = {MENU_AUTODIM_ENABLE,menu_tf+menu_isSub,"ADIM","ADIM",&g_AutoDim,&b_AutoDim,0,2,{menu_offon}};
menu_item menuAdimHr = {MENU_AUTODIM_HOUR,menu_num+menu_isSub,"ADMH","ADMH",&g_AutoDimHour,&b_AutoDimHour,0,23,{NULL}};
menu_item menuAdimLvl = {MENU_AUTODIM_LEVEL,menu_num+menu_isSub,"ADML","ADML",&g_AutoDimLevel,&b_AutoDimLevel,0,10,{NULL}};
menu_item menuAbrtHr = {MENU_AUTOBRT_HOUR,menu_num+menu_isSub,"ABTH","ABTH",&g_AutoBrtHour,&b_AutoBrtHour,0,23,{NULL}};
menu_item menuAbrtLvl = {MENU_AUTOBRT_LEVEL,menu_num+menu_isSub,"ABTL","ABTL",&g_AutoBrtLevel,&b_AutoBrtLevel,1,10,{NULL}};
#endif
#ifdef FEATURE_SET_DATE						
menu_item menuDate_ = {MENU_DATE,menu_hasSub,"DAT=","DATE ",NULL,NULL,0,0,{NULL}};
menu_item menuYear = {MENU_DATEYEAR,menu_num+menu_isSub,"YEAR","YEAR",&g_dateyear,NULL,10,29,{NULL}};
menu_item menuMonth = {MENU_DATEMONTH,menu_num+menu_isSub,"MNTH","MONTH",&g_datemonth,NULL,1,12,{NULL}};
menu_item menuDay = {MENU_DATEDAY,menu_num+menu_isSub,"DAY","DAY",&g_dateday,NULL,1,31,{NULL}};
#endif
menu_item menuDots = {MENU_DOTS,menu_tf+menu_disabled,"DOTS","DOTS",&g_show_dots,&b_show_dots,0,1,{menu_offon}};
#ifdef FEATURE_AUTO_DST
menu_item menuDST_ = {MENU_DST,menu_hasSub,"DST=","DST  ",NULL,NULL,0,0,{NULL}};
menu_item menuDST = {MENU_DST_ENABLE,menu_list+menu_isSub,"DST","DST",&g_DST_mode,&b_DST_mode,0,3,{menu_adst}};
#elif defined FEATURE_WmGPS
menu_item menuDST = {MENU_DST_ENABLE,menu_tf,"DST","DST",&g_DST_mode,&b_DST_mode,0,2,{menu_offon}};
#endif
#ifdef FEATURE_AUTO_DST
menu_item menuRules = {MENU_RULES,menu_hasSub+menu_isSub,"RUL=","RULES",NULL,NULL,0,0,{NULL}};
menu_item menuRule0 = {MENU_RULE0,menu_num+menu_isSub,"RUL0","RULE0",&g_DST_Rules[0],&b_DST_Rule0,1,7,{NULL}};
menu_item menuRule1 = {MENU_RULE1,menu_num+menu_isSub,"RUL1","RULE1",&g_DST_Rules[1],&b_DST_Rule1,1,7,{NULL}};
menu_item menuRule2 = {MENU_RULE2,menu_num+menu_isSub,"RUL2","RULE2",&g_DST_Rules[2],&b_DST_Rule2,1,5,{NULL}};
menu_item menuRule3 = {MENU_RULE3,menu_num+menu_isSub,"RUL3","RULE3",&g_DST_Rules[3],&b_DST_Rule3,0,23,{NULL}};
menu_item menuRule4 = {MENU_RULE4,menu_num+menu_isSub,"RUL4","RULE4",&g_DST_Rules[4],&b_DST_Rule4,1,12,{NULL}};
menu_item menuRule5 = {MENU_RULE5,menu_num+menu_isSub,"RUL5","RULE5",&g_DST_Rules[5],&b_DST_Rule5,1,7,{NULL}};
menu_item menuRule6 = {MENU_RULE6,menu_num+menu_isSub,"RUL6","RULE6",&g_DST_Rules[6],&b_DST_Rule6,1,5,{NULL}};
menu_item menuRule7 = {MENU_RULE7,menu_num+menu_isSub,"RUL7","RULE7",&g_DST_Rules[7],&b_DST_Rule7,0,23,{NULL}};
menu_item menuRule8 = {MENU_RULE8,menu_num+menu_isSub,"RUL8","RULE8",&g_DST_Rules[8],&b_DST_Rule8,1,1,{NULL}};  // offset can't be changed
#endif
#if defined FEATURE_WmGPS
menu_item menuGPS_ = {MENU_GPS,menu_hasSub,"GPS=","GPS  ",NULL,NULL,0,0,{NULL}};
menu_item menuGPS = {MENU_GPS_ENABLE,menu_list+menu_isSub,"GPS","GPS",&g_gps_enabled,&b_gps_enabled,0,3,{menu_gps}};
menu_item menuTZh = {MENU_TZH,menu_num+menu_isSub,"TZH","TZ-H",&g_TZ_hour,&b_TZ_hour,-12,12,{NULL}};
menu_item menuTZm = {MENU_TZM,menu_num+menu_isSub,"TZM","TZ-M",&g_TZ_minute,&b_TZ_minute,0,59,{NULL}};
#endif
#if defined FEATURE_GPS_DEBUG
menu_item menuGPSc = {MENU_GPSC,menu_num+menu_isSub,"GPSC","GPSC",&g_gps_cks_errors,NULL,0,0,{NULL}};
menu_item menuGPSp = {MENU_GPSP,menu_num+menu_isSub,"GPSP","GPSP",&g_gps_parse_errors,NULL,0,0,{NULL}};
menu_item menuGPSt = {MENU_GPST,menu_num+menu_isSub,"GPST","GPST",&g_gps_time_errors,NULL,0,0,{NULL}};
#endif
menu_item menuTemp = {MENU_TEMP,menu_tf+menu_disabled,"TEMP","TEMP",&g_show_temp,&b_show_temp,0,2,{menu_offon}};
#if defined FEATURE_FLW
menu_item menuFLW = {MENU_FLW,menu_tf+menu_disabled,"FLW","FLW",&g_flw_enabled,&b_flw_enabled,0,2,{menu_offon}};
#endif
menu_item menuVol = {MENU_VOL,menu_list,"VOL","VOL",&g_volume,&b_volume,0,2,{menu_volume}};

menu_item * menuItems[] = { 
	&menu24h, 
#ifdef FEATURE_AUTO_DATE
	&menuAdate_, &menuAdate, &menuRegion,
#endif
#ifdef FEATURE_AUTO_DIM
	&menuAdim_,	&menuAdim, &menuAdimHr, &menuAdimLvl, &menuAbrtHr, &menuAbrtLvl,
#endif
	&menuBrt, 
#ifdef FEATURE_SET_DATE
	&menuDate_, &menuYear, &menuMonth, &menuDay,
#endif
	&menuDots,
#if defined FEATURE_AUTO_DST
	&menuDST_, &menuDST,
#elif defined FEATURE_WmGPS
	&menuDST,
#endif
#ifdef FEATURE_AUTO_DST
	&menuRules,	
	&menuRule0, &menuRule1, &menuRule2, &menuRule3, &menuRule4, &menuRule5, &menuRule6, &menuRule7, &menuRule8,
#endif
#ifdef FEATURE_FLW
	&menuFLW,
#endif
#if defined FEATURE_WmGPS
	&menuGPS_, &menuGPS, &menuTZh, &menuTZm,
#endif
#if defined FEATURE_GPS_DEBUG
	&menuGPSc, &menuGPSp, &menuGPSt,
#endif
	&menuTemp,
	&menuVol,
	NULL};  // end of list marker must be here

extern tmElements_t* tm_; // current local date and time as TimeElements (pointer)

#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
void setDSToffset(uint8_t mode) {
	int8_t adjOffset;
	uint8_t newOffset;
#ifdef FEATURE_AUTO_DST
	if (mode == 2) {  // Auto DST
		if (g_DST_updated) return;  // already done it once today
		if (tm_ == NULL) return;  // safet check
		newOffset = getDSToffset(tm_, g_DST_Rules);  // get current DST offset based on DST Rules
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
	DSTinit(tm_, g_DST_Rules);  // re-compute DST start, end for new date
	g_DST_updated = false;  // allow automatic DST adjustment again
	setDSToffset(g_DST_mode);  // set DSToffset based on new date
#endif
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
		case MENU_DATEYEAR:
		case MENU_DATEMONTH:
		case MENU_DATEDAY:
			set_date(g_dateyear, g_datemonth, g_dateday);
			break;
		case MENU_GPS_ENABLE:
			gps_init(g_gps_enabled);  // change baud rate
			break;
		case MENU_RULE0:
		case MENU_RULE1:
		case MENU_RULE2:
		case MENU_RULE3:
		case MENU_RULE4:
		case MENU_RULE5:
		case MENU_RULE6:
		case MENU_RULE7:
		case MENU_RULE8:
		case MENU_DST_ENABLE:
			g_DST_updated = false;  // allow automatic DST adjustment again
			DSTinit(tm_, g_DST_Rules);  // re-compute DST start, end for new data
			setDSToffset(g_DST_mode);
			break;
		case MENU_TZH:
		case MENU_TZM:
			tGPSupdate = 0;  // allow GPS to refresh
			break;
	}
}

volatile uint8_t menuIdx = 0;
uint8_t update = false;  // right button updates value?
uint8_t show = false;  // show value?

void menu_enable(menu_number num, uint8_t enable)
{
	uint8_t idx = 0;
	menu_item * mPtr = menuItems[0];  // start with first menu item
	while(mPtr != NULL) {
		if (mPtr->menuNum == num) {
//			beep(1920,1);  // debug
			if (enable)
				mPtr->flags &= (255-menu_disabled);  // turn off disabled flag
			else
				mPtr->flags |= menu_disabled;  // turn on disabled flag
			return;
		}
		mPtr = menuItems[++idx];
	}
}

menu_item * nextItem(uint8_t skipSub)  // next menu item
{
	menu_item * menuPtr = menuItems[menuIdx];  // current menu item
	uint8_t inSub = (menuPtr->flags & menu_isSub) && !(menuPtr->flags & menu_hasSub);  // are we in a sub menu now?
	menuPtr = menuItems[++menuIdx];  // next menu item
	while ((menuPtr != NULL) && ((menuPtr->flags & menu_disabled) || (skipSub && (menuPtr->flags & menu_isSub) && !inSub)) ) { 
		menuPtr = menuItems[++menuIdx];  // next menu item
	}
	return menuPtr;
}

void menu(uint8_t btn)
{
	menu_item * menuPtr = menuItems[menuIdx];  // current menu item
	uint8_t digits = get_digits();
	tick();
	switch (btn) {
		case 0:  // start at top of menu
			menuIdx = 0;  // restart menu
			menuPtr = menuItems[menuIdx];
			update = false;
			show = false;
			break;
		case 1:  // right button - show/update current item value
			if (menuPtr->flags & menu_hasSub) {
				menuPtr = nextItem(false);  // show first submenu item
				update = false;
			}
			else {
				show = true;  // show value
				if (digits>6)
					update = true;
			}
			break;
		case 2:  // left button - show next menu item
			menuPtr = nextItem(true);  // next menu items (skip subs)
			update = false;
			show = false;
			break;
		case 3:  // auto menu timeout
			if (digits == 4)  return;  // too confusing in IV-17?
			if (menuPtr->flags & menu_hasSub) {
				return;  // just show the item as before
			}
			else {
				show = true;  // show value
				if (digits>6)
					update = true;
			}
			break;
	}
	if (menuPtr == NULL) {  // check for end of menu
		menuIdx = 0;
		update = false;
		menu_state = STATE_CLOCK;
		return;
	}
	char * valstr = "";
//	char * shortName = (char *)menuPtr->shortName;
//	char * longName = (char *)menuPtr->longName;
	char shortName[5];
	char longName[7];
	strcpy(shortName,menuPtr->shortName);
	strcpy(longName,menuPtr->longName);
	int valnum = *(menuPtr->setting);
	const menu_value * menuValues = *menuPtr->menuList;  //copy menu values
	uint8_t idx = 0;
// numeric menu item
	if (menuPtr->flags & menu_num) {
			if (update) {
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
			show_setting_int(shortName, longName, valnum, show);
			if (show)
				update = true;
			else
				show = true;
		}
// true/false menu item
	else if (menuPtr->flags & menu_tf) {
			if (update) {
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
			show_setting_string(shortName, longName, valstr, show);
			if (show)
				update = true;
			else
				show = true;
		}
// list menu item
		else if (menuPtr->flags & menu_list) {
			for (uint8_t i=0;i<menuPtr->hiLimit;i++) {
				if (menuValues[i].value == valnum) {
					idx = i;
					}
			}
			valstr = (char*)menuValues[idx].valName;
			if (update) {
				idx++;
				if (idx >= menuPtr->hiLimit)  // for lists, hilimit is the # of elements!
					idx = 0;
				valnum = menuValues[idx].value;
				valstr = (char*)menuValues[idx].valName;
				*menuPtr->setting = valnum;
				if (menuPtr->eeAddress != NULL) 
					eeprom_update_byte(menuPtr->eeAddress, valnum);
				menu_action(menuPtr);
			}
			show_setting_string(shortName, longName, valstr, show);
			if (show)
				update = true;
			else
				show = true;
		}
// top of sub menu item
		else if (menuPtr->flags & menu_hasSub) {
			valstr = "";
			strcat(longName, "-");  // indicate top of sub
			show_setting_string(shortName, longName, valstr, false);
		}
}  // menu

void menu_init(void)
{
	menu_state = STATE_CLOCK;
	menu_enable(MENU_TEMP, rtc_is_ds3231());  // show temperature setting only when running on a DS3231
	menu_enable(MENU_DOTS, g_has_dots);  // don't show dots settings for shields that have no dots
#ifdef FEATURE_FLW
	menu_enable(MENU_FLW, g_has_eeprom);  // don't show FLW settings when there is no EEPROM with database
#endif
}
