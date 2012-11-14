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
#ifndef MENU_H_
#define MENU_H_

typedef enum {
	menu_num = 0,  // simple numeric value
	menu_tf = 1, // true/false
	menu_list = 2,  // select one from a list
	menu_sub = 3,  // sub menu
} menu_types;

typedef struct {
	const int8_t value;  // list of possible values in order
	const char * valName;  // list of value names for display
} menu_values;

typedef struct {
	const uint8_t menuNum;  // menu item number
	char * shortName;
	char * longName;
	const menu_types menuType;
	int8_t * setting;
	const int8_t loLimit;  // low limit for num
	const int8_t hiLimit;  // high limit for num, # of values for list
	const menu_values * menuList[];
} menu_item;

// menu states
typedef enum {
	// basic states
	STATE_CLOCK = 0,  // show clock time
	STATE_SET_CLOCK,
	STATE_SET_ALARM,
	// menu
	STATE_MENU,
	STATE_MENU_LAST,
} menu_state_t;

typedef enum {
	MENU_BRIGHTNESS = 0,
	MENU_24H,
	MENU_VOL,
//	MENU_DATE,
	MENU_SETYEAR,
	MENU_SETMONTH,
	MENU_SETDAY,
	MENU_AUTODATE,
	MENU_REGION,
	MENU_AUTODIM,
	MENU_AUTODIM_HOUR,
	MENU_AUTODIM_LEVEL,
	MENU_AUTOBRT_HOUR,
	MENU_AUTOBRT_LEVEL,
	MENU_DST,
	MENU_GPS,
	MENU_GPSC,  // GPS error counter
	MENU_GPSP,  // GPS error counter
	MENU_GPST,  // GPS error counter
	MENU_TZH,
	MENU_TZM,
	MENU_TEMP,
	MENU_DOTS,
	MENU_FLW,
} menu_number;

menu_state_t menu_state;
uint8_t menu_update;

int8_t g_24h_clock;
int8_t g_show_temp;
int8_t g_show_dots;
int8_t g_brightness;
int8_t g_volume;
#ifdef FEATURE_SET_DATE
int8_t g_dateyear;
int8_t g_datemonth;
int8_t g_dateday;
#endif
#ifdef FEATURE_FLW
int8_t g_flw_enabled;
#endif
#ifdef FEATURE_WmGPS 
int8_t g_gps_enabled;
int8_t g_gps_updating;  // for signalling GPS update on some displays
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
int8_t g_DST_mode;  // DST off, on, auto?
int8_t g_DST_offset;  // DST offset in Hours
int8_t g_DST_updated;  // DST update flag = allow update only once per day
#endif
#ifdef FEATURE_AUTO_DATE
int8_t g_region;
int8_t g_autodate;
#endif
#ifdef FEATURE_AUTO_DIM
int8_t g_AutoDim;
int8_t g_AutoDimHour;
int8_t g_AutoDimLevel;
int8_t g_AutoBrtHour;
int8_t g_AutoBrtLevel;
#endif

#define MENU_TIMEOUT 220  
#ifdef FEATURE_AUTO_MENU
#define BUTTON2_TIMEOUT 100
#endif

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

display_mode_t clock_mode;
display_mode_t save_mode;  // for restoring mode after autodate display

#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
void setDSToffset(uint8_t mode);
#endif
void menu_init(void);
void menu(uint8_t n, uint8_t update, uint8_t show);

#endif