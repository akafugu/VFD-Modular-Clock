/*
 * Auto DST support for VFD Modular Clock
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

#include <avr/io.h>
#include <string.h>
#include "Time.h"
#include "adst.h"

// globals from main.c
extern uint8_t g_DST_mode;  // DST off, on, auto?
extern uint8_t g_DST_offset;  // DST offset in hours
extern uint8_t g_DST_update;  // DST update flag

// Number of days at the beginning of the month if not leap year
static const uint16_t monthDays[]={0,31,59,90,120,151,181,212,243,273,304,334};
// value used to prevent looping when "falling back"
static long seconds_last = 0;

// Calculate day of the week - Sunday=1, Saturday=7  (non ISO)
uint8_t dotw(uint8_t year, uint8_t month, uint8_t day)
{
  uint16_t m, y;
	m = month;
	y = 2000 + year;  // a reasonable assumption good for another 80+ years...
  if (m < 3)  {
    m += 12;
    y -= 1;
  }
	return (day + (2 * m) + (6 * (m+1)/10) + y + (y/4) - (y/100) + (y/400) + 1) % 7 + 1;
}

long yearSeconds(uint8_t yr, uint8_t mo, uint8_t da, uint8_t h, uint8_t m, uint8_t s)
{
  long dn;
  dn = monthDays[(mo-1)]+da;  // # days so far if not leap year
  if ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0)  // if leap year
    dn ++;  // add 1 day
  dn = dn * 86400 + h*3600 + m*60 + s;
  return dn;
} 

long DSTseconds(uint8_t year, uint8_t month, uint8_t doftw, uint8_t n, uint8_t hour)
{
	uint8_t dom = monthDays[month-1];
	if ( (year%4 == 0) && (month == 2) )
		dom ++;  // february has 29 days this year
	uint8_t dow = dotw(year, month, 1);  // DOW for 1st day of month for DST event
	uint8_t day = doftw - dow;  // number of days until 1st dotw in given month
//	if (day<1)  day += 7;  // make sure it's positive - doesn't work with uint!
  if (doftw >= dow)
    day = doftw - dow;
  else
    day = doftw + 7 - dow;
	day = 1 + day + (n-1)*7;  // date of dotw for this year
	while (day > dom)  // handles "last DOW" case
		day -= 7;
  return yearSeconds(year,month,day,hour,0,0);  // seconds til DST event this year
}

// DST Rules: Start(month, dotw, n, hour), End(month, dotw, n, hour), Offset
// DOTW is Day of the Week.  1=Sunday, 7=Saturday
// N is which occurrence of DOTW
// Current US Rules: March, Sunday, 2nd, 2am, November, Sunday, 1st, 2 am, 1 hour
// 		3,1,2,2,  11,1,1,2,  1
uint8_t getDSToffset(tmElements_t* te, DST_Rules* rules)
{
	// if current time & date is at or past the first DST rule and before the second, return 1
	// otherwise return 0
  // seconds til start of DST this year
  long seconds1 = DSTseconds(te->Year, rules->Start.Month, rules->Start.DOTW, rules->Start.Week, rules->Start.Hour);  
	// seconds til end of DST this year
  long seconds2 = DSTseconds(te->Year, rules->End.Month, rules->End.DOTW, rules->End.Week, rules->End.Hour);  
	long seconds_now = yearSeconds(te->Year, te->Month, te->Day, te->Hour, te->Minute, te->Second);
// NOTE - the following could be improved - if user sets date, or even time, reset "seconds_last" ???
	if (seconds_now < seconds_last)  // is time less than it was?
		seconds_now = seconds_last;  // prevent loop when setting time back
	if (seconds2>seconds1) {  // northern hemisphere
		if ((seconds_now >= seconds1) && (seconds_now < seconds2))  // spring ahead
			return(rules->Offset);  // return Offset 
		else {  // fall back
			seconds_last = seconds_now;
			return(0);  // return 0
		}
	}
	else {  // southern hemisphere
		if ((seconds_now >= seconds2) && (seconds_now < seconds1))  // fall ahead
			return(rules->Offset);  // return Offset
		else {  // spring back
			seconds_last = seconds_now;
			return(0);  // return 0
		}
	}
}

char dst_setting_[5];
char* dst_setting(uint8_t dst)
{
	switch (dst) {
		case(0):
			strcpy(dst_setting_,"off");
			break;
		case(1):
			strcpy(dst_setting_,"on ");
			break;
		case(2):
			strcpy(dst_setting_,"auto");
			break;
		default:
			strcpy(dst_setting_,"???");
	}
	return dst_setting_;
}
