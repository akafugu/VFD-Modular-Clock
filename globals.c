/*
 * Globals for VFD Modular Clock
 * (C) 2011-2012 Akafugu Corporation
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
#define FEATURE_AUTO_DIM  // temp

#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "globals.h"

// Settings saved to eeprom
uint8_t EEMEM b_dummy = 0;  // dummy item to test for bug
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
uint8_t EEMEM b_TZ_minute = 0;
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
#ifdef FEATURE_AUTO_DST
//DST_Rules dst_rules = {{3,1,2,2},{11,1,1,2},1};   // initial values from US DST rules as of 2011
//DST_Rules dst_rules = {{10,1,1,2},{4,1,1,2},1};   // DST Rules for parts of OZ including NSW (for JG)
//#define DST_NSW
#ifdef DST_NSW
uint8_t EEMEM b_DST_Rule0 = 10;  // DST start month
uint8_t EEMEM b_DST_Rule1 = 1;  // DST start dotw
uint8_t EEMEM b_DST_Rule2 = 1;  // DST start week
uint8_t EEMEM b_DST_Rule3 = 2;  // DST start hour
uint8_t EEMEM b_DST_Rule4 = 4; // DST end month
uint8_t EEMEM b_DST_Rule5 = 1;  // DST end dotw
uint8_t EEMEM b_DST_Rule6 = 1;  // DST end week
uint8_t EEMEM b_DST_Rule7 = 2;  // DST end hour
uint8_t EEMEM b_DST_Rule8 = 1;  // DST offset
#else
uint8_t EEMEM b_DST_Rule0 = 3;  // DST start month
uint8_t EEMEM b_DST_Rule1 = 1;  // DST start dotw
uint8_t EEMEM b_DST_Rule2 = 2;  // DST start week
uint8_t EEMEM b_DST_Rule3 = 2;  // DST start hour
uint8_t EEMEM b_DST_Rule4 = 11; // DST end month
uint8_t EEMEM b_DST_Rule5 = 1;  // DST end dotw
uint8_t EEMEM b_DST_Rule6 = 1;  // DST end week
uint8_t EEMEM b_DST_Rule7 = 2;  // DST end hour
uint8_t EEMEM b_DST_Rule8 = 1;  // DST offset
#endif
#endif

 void globals_init(void)
 {
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
	g_TZ_minute = eeprom_read_byte(&b_TZ_minute);
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
#ifdef FEATURE_AUTO_DST
//DST_Rules dst_rules = {{3,1,2,2},{11,1,1,2},1};   // initial values from US DST rules as of 2011
	g_DST_Rules[0] = eeprom_read_byte(&b_DST_Rule0);  // DST start month
	g_DST_Rules[1] = eeprom_read_byte(&b_DST_Rule1);  // DST start dotw
	g_DST_Rules[2] = eeprom_read_byte(&b_DST_Rule2);  // DST start week
	g_DST_Rules[3] = eeprom_read_byte(&b_DST_Rule3);  // DST start hour
	g_DST_Rules[4] = eeprom_read_byte(&b_DST_Rule4); // DST end month
	g_DST_Rules[5] = eeprom_read_byte(&b_DST_Rule5);  // DST end dotw
	g_DST_Rules[6] = eeprom_read_byte(&b_DST_Rule6);  // DST end week
	g_DST_Rules[7] = eeprom_read_byte(&b_DST_Rule7);  // DST end hour
	g_DST_Rules[8] = eeprom_read_byte(&b_DST_Rule8);  // DST offset
#endif
 }