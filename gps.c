/*
 * GPS support for VFD Modular Clock
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

#include <avr/interrupt.h>
#include <string.h>
#include <util/delay.h>
#include <stdlib.h>
#include "gps.h"
#include "display.h"
#include "piezo.h"
#include "rtc.h"
#include "Time.h"

// globals from main.c
extern uint8_t g_DST_offset;  // DST offset
extern uint8_t g_gps_updating;
extern enum shield_t shield;
extern uint16_t g_gps_errors;  // GPS error counter
extern uint16_t g_gps_cks_errors;  // GPS checksum error counter

// String buffer for processing GPS data:
char gpsBuffer[GPSBUFFERSIZE+2];
//volatile uint8_t gpsEnabled = 0;
#define gpsTimeoutLimit 5  // 5 seconds until we display the "no gps" message
uint16_t gpsTimeout = 0;  // how long since we received valid GPS data?
time_t tGPSupdate = 0;

char gps_setting_[4];
char* gps_setting(uint8_t gps)
{
	switch (gps) {
		case(0):
			strcpy(gps_setting_,"off");
			break;
		case(48):
			strcpy(gps_setting_," 48");
			break;
		case(96):
			strcpy(gps_setting_," 96");
			break;
		default:
			strcpy(gps_setting_," ??");
	}
	return gps_setting_;
}

// Set DST offset and save in EE prom

//Check to see if there is any serial data.
uint8_t gpsDataReady(void) {
  return (UCSR0A & _BV(RXC0));
}

// get data from gps and update the clock (if possible)
void getGPSdata(void) {
	char charReceived = UDR0;  // get a byte from the port
	uint8_t bufflen = strlen(gpsBuffer);
	//If the buffer has not been started, check for '$'
	if ( ( bufflen == 0 ) &&  ( '$' != charReceived ) )
		return;  // wait for start of next sentence from GPS
	if ( bufflen < (GPSBUFFERSIZE - 1) ) {  // is there room left? (allow room for null term)
		if ( '\r' != charReceived ) {  // end of sentence?
			strncat(gpsBuffer, &charReceived, 1);  // add char to buffer
			return;
		}
		strncat(gpsBuffer, "*", 1);  // mark end of buffer just in case
		//beep(1000, 1);  // debugging
		// end of sentence - is this the message we are looking for?
		parseGPSdata();  // check for GPRMC sentence and set clock
	}  // if space left in buffer
	// either buffer is full, or the message has been processed. reset buffer for next message
	memset( gpsBuffer, 0, GPSBUFFERSIZE );  // clear GPS buffer
}  // getGPSdata

uint32_t parsedecimal(char *str) {
  uint32_t d = 0;
  while (str[0] != 0) {
   if ((str[0] > '9') || (str[0] < '0'))
     return d;  // no more digits
	 d = (d*10) + (str[0] - '0');
   str++;
  }
  return d;
}
char hex[17] = "0123456789ABCDEF";
uint8_t atoh(char x) {
  return (strchr(hex, x) - hex);
}
uint32_t hex2i(char *str, uint8_t len) {
  uint32_t d = 0;
	for (uint8_t i=0; i<len; i++) {
	 d = (d*10) + (strchr(hex, str[i]) - hex);
	}
	return d;
}
//  225446       Time of fix 22:54:46 UTC
//  A            Navigation receiver warning A = OK, V = warning
//  4916.45,N    Latitude 49 deg. 16.45 min North
//  12311.12,W   Longitude 123 deg. 11.12 min West
//  000.5        Speed over ground, Knots
//  054.7        Course Made Good, True
//  191194       Date of fix  19 November 1994
//  020.3,E      Magnetic variation 20.3 deg East
//  *68          mandatory checksum

//$GPRMC,225446.000,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68\r\n
// 0         1         2         3         4         5         6         7
// 0123456789012345678901234567890123456789012345678901234567890123456789012
//    0     1       2    3    4     5    6   7     8      9     10  11 12
void parseGPSdata() {
	time_t tNow;
	tmElements_t tm;
	uint8_t gpsCheck1, gpsCheck2;  // checksums
//	char gpsTime[10];  // time including fraction hhmmss.fff
	char gpsFixStat;  // fix status
//	char gpsLat[7];  // ddmm.ff  (with decimal point)
//	char gpsLatH;  // hemisphere 
//	char gpsLong[8];  // dddmm.ff  (with decimal point)
//	char gpsLongH;  // hemisphere 
//	char gpsSpeed[5];  // speed over ground
//	char gpsCourse[5];  // Course
//	char gpsDate[6];  // Date
//	char gpsMagV[5];  // Magnetic variation 
//	char gpsMagD;  // Mag var E/W
//	char gpsCKS[2];  // Checksum without asterisk
	char *ptr;
  uint32_t tmp;
  char *tok = &gpsBuffer[0];
	if ( strncmp( gpsBuffer, "$GPRMC,", 7 ) == 0 ) {  
		//beep(1000, 1);
		//Calculate checksum from the received data
		ptr = &gpsBuffer[1];  // start at the "G"
		gpsCheck1 = 0;  // init collector
		 /* Loop through entire string, XORing each character to the next */
		while (*ptr != '*') // count all the bytes up to the asterisk
		{
			gpsCheck1 ^= *ptr;
			ptr++;
			if (ptr>(gpsBuffer+GPSBUFFERSIZE)) goto GPSerror;  // do we really need this???
		}
		// now get the checksum from the string itself, which is in hex
    gpsCheck2 = atoh(*(ptr+1)) * 16 + atoh(*(ptr+2));
		if (gpsCheck1 == gpsCheck2) {  // if checksums match, process the data
			//beep(1000, 1);
			tok = strtok(gpsBuffer, ",*\r");  // parse $GPRMC
			if (tok == NULL) goto GPSerror;
			tok = strtok(NULL, ",*\r");  // Time including fraction hhmmss.fff
			if (tok == NULL) goto GPSerror;
			if ((strlen(tok) < 6) || (strlen(tok) > 10)) goto GPSerror;  // check time length
//			strncpy(gpsTime, tok, 10);  // copy time string hhmmss
			tmp = parsedecimal(tok);   // parse integer portion
			tm.Hour = tmp / 10000;
			tm.Minute = (tmp / 100) % 100;
			tm.Second = tmp % 100;
			tok = strtok(NULL, ",*\r");  // Status
			if (tok == NULL) goto GPSerror;
			gpsFixStat = tok[0];
			if (gpsFixStat == 'A') {  // if data valid, parse time & date
				gpsTimeout = 0;  // reset gps timeout counter
				tok = strtok(NULL, ",*\r");  // Latitude including fraction
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsLat, tok, 7);  // copy Latitude ddmm.ff
				tok = strtok(NULL, ",*\r");  // Latitude N/S
				if (tok == NULL) goto GPSerror;
//				gpsLatH = tok[0];
				tok = strtok(NULL, ",*\r");  // Longitude including fraction hhmm.ff
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsLong, tok, 7);
				tok = strtok(NULL, ",*\r");  // Longitude Hemisphere
				if (tok == NULL) goto GPSerror;
//				gpsLongH = tok[0];
				tok = strtok(NULL, ",*\r");  // Ground speed 000.5
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsSpeed, tok, 5);
				tok = strtok(NULL, ",*\r");  // Track angle (course) 054.7
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsCourse, tok, 5);
				tok = strtok(NULL, ",*\r");  // Date ddmmyy
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsDate, tok, 6);
				if (strlen(tok) != 6) goto GPSerror;  // check date length
				tmp = parsedecimal(tok); 
				tm.Day = tmp / 10000;
				tm.Month = (tmp / 100) % 100;
				tm.Year = tmp % 100;
				tok = strtok(NULL, "*\r");  // magnetic variation & dir
				if (tok == NULL) goto GPSerror;
				if (tok == NULL) goto GPSerror;
				tok = strtok(NULL, ",*\r");  // Checksum
				if (tok == NULL) goto GPSerror;
//				strncpy(gpsCKS, tok, 2);  // save checksum chars
				
				tm.Year = y2kYearToTm(tm.Year);  // convert yy year to (yyyy-1970) (add 30)
				tNow = makeTime(&tm);  // convert to time_t
				
				if ((tGPSupdate>0) && (abs(tNow-tGPSupdate)>SECS_PER_DAY))  goto GPSerror;  // GPS time jumped more than 1 day

				if ((tm.Second == 0) || ((tNow - tGPSupdate)>=60)) {  // update RTC once/minute or if it's been 60 seconds
					//beep(1000, 1);  // debugging
					g_gps_updating = true;
					tGPSupdate = tNow;  // remember time of this update
					tNow = tNow + (long)(g_TZ_hour + g_DST_offset) * SECS_PER_HOUR;  // add time zone hour offset & DST offset
					if (g_TZ_hour < 0)  // add or subtract time zone minute offset
						tNow = tNow - (long)g_TZ_minutes * SECS_PER_HOUR;
					else
						tNow = tNow + (long)g_TZ_minutes * SECS_PER_HOUR;
					rtc_set_time_t(tNow);  // set RTC from adjusted GPS time & date
					if (shield != SHIELD_IV18)
						flash_display(100);  // flash display to show GPS update 28oct12/wbp - shorter blink
				}
				else
					g_gps_updating = false;

			} // if fix status is A
		} // if checksums match
		else  // checksums do not match
			g_gps_cks_errors++;  // increment error count
		return;
GPSerror:
		beep(2093,1);  // error signal
		flash_display(200);  // flash display to show GPS error
		g_gps_errors++;  // increment error count
		strcpy(gpsBuffer, "");  // wipe GPS buffer
	}  // if "$GPRMC"
}

void uart_init(uint16_t BRR) {
  /* setup the main UART */
  UBRR0 = BRR;               // set baudrate counter

  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(USBS0) | (3<<UCSZ00);
  DDRD |= _BV(PD1);
  DDRD &= ~_BV(PD0);

}

void gps_init(uint8_t gps) {
	switch (gps) {
		case(0):  // no GPS
			break;
		case(48):
			uart_init(BRRL_4800);
			break;
		case(96):
			uart_init(BRRL_9600);
			break;
	}
	tGPSupdate = 0;  // reset GPS last update time
}