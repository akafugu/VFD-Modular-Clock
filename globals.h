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
 #ifndef GLOBALS_H_
#define GLOBALS_H_

extern uint8_t b_24h_clock;
extern uint8_t b_show_temp;
extern uint8_t b_show_dots;
extern uint8_t b_brightness;
extern uint8_t b_volume;
#ifdef FEATURE_SET_DATE
extern uint8_t b_dateyear;
extern uint8_t b_datemonth;
extern uint8_t b_dateday;
#endif
#ifdef FEATURE_FLW
extern uint8_t b_flw_enabled;
#endif
#ifdef FEATURE_WmGPS 
extern uint8_t b_gps_enabled;
extern uint8_t b_TZ_hour;
extern uint8_t b_TZ_minute;
extern uint8_t b_gps_updating;  // for signalling GPS update on some displays
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
extern uint8_t b_DST_mode;  // DST off, on, auto?
extern uint8_t b_DST_offset;  // DST offset in Hours
extern uint8_t b_DST_updated;  // DST update flag = allow update only once per day
#endif
#ifdef FEATURE_AUTO_DATE
extern uint8_t b_Region;
extern uint8_t b_AutoDate;
#endif
#ifdef FEATURE_AUTO_DIM
extern uint8_t b_AutoDim;
extern uint8_t b_AutoDimHour;
extern uint8_t b_AutoDimLevel;
extern uint8_t b_AutoBrtHour;
extern uint8_t b_AutoBrtLevel;
#endif
#ifdef FEATURE_AUTO_DST
extern uint8_t b_DST_Rule0;
extern uint8_t b_DST_Rule1;
extern uint8_t b_DST_Rule2;
extern uint8_t b_DST_Rule3;
extern uint8_t b_DST_Rule4;
extern uint8_t b_DST_Rule5;
extern uint8_t b_DST_Rule6;
extern uint8_t b_DST_Rule7;
extern uint8_t b_DST_Rule8;
#endif

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
int8_t g_TZ_hour;
int8_t g_TZ_minute;
int8_t g_gps_updating;  // for signalling GPS update on some displays
// debugging counters 
int8_t g_gps_cks_errors;  // gps checksum error counter
int8_t g_gps_parse_errors;  // gps parse error counter
int8_t g_gps_time_errors;  // gps time error counter
#endif
#if defined FEATURE_WmGPS || defined FEATURE_AUTO_DST
int8_t g_DST_mode;  // DST off, on, auto?
int8_t g_DST_offset;  // DST offset in Hours
int8_t g_DST_updated;  // DST update flag = allow update only once per day
#endif
#ifdef FEATURE_AUTO_DST  // DST rules
//DST_Rules dst_rules = {{3,1,2,2},{11,1,1,2},1};   // initial values from US DST rules as of 2011
int8_t g_DST_Rules[9];
#endif
#ifdef FEATURE_AUTO_DATE
int8_t g_Region;
int8_t g_AutoDate;
#endif
#ifdef FEATURE_AUTO_DIM
int8_t g_AutoDim;
int8_t g_AutoDimHour;
int8_t g_AutoDimLevel;
int8_t g_AutoBrtHour;
int8_t g_AutoBrtLevel;
#endif
uint8_t g_has_dots; // can current shield show dot (decimal points)
#ifdef FEATURE_FLW
uint8_t g_has_eeprom; // set to true if there is a four letter word EEPROM attached
#endif

void globals_init(void);

#endif

