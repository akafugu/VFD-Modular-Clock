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
#ifdef FEATURE_AUTO_DIM
	STATE_MENU_AUTODIM,
	STATE_MENU_AUTODIM_HOUR,
	STATE_MENU_AUTODIM_LEVEL,
	STATE_MENU_AUTOBRT_HOUR,
	STATE_MENU_AUTOBRT_LEVEL,
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
	STATE_MENU_DST,
#endif
#ifdef FEATURE_WmGPS
	STATE_MENU_GPS,
#ifdef FEATURE_GPS_DEBUG
	STATE_MENU_GPSC,  // GPS error counter
	STATE_MENU_GPSP,  // GPS error counter
	STATE_MENU_GPST,  // GPS error counter
#endif
	STATE_MENU_ZONEH,
	STATE_MENU_ZONEM,
#endif
	STATE_MENU_TEMP,
	STATE_MENU_DOTS,
#ifdef FEATURE_FLW
	STATE_MENU_FLW,
#endif
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

menu_state_t menu_state;
sub_menu_state_t submenu_state;
uint8_t menu_update;

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
void menu(uint8_t update, uint8_t show);

#endif