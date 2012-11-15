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

#ifndef GPS_H_
#define GPS_H_

// String buffer size:
#define GPSBUFFERSIZE 96 
//The year the clock was programmed, used for error checking
#define PROGRAMMING_YEAR 12

//int8_t g_TZ_hour;
//int8_t g_TZ_minutes;
unsigned long tGPSupdate;  // really time_t

// we double buffer: read into one line and leave one for the main program
volatile char gpsBuffer1[GPSBUFFERSIZE];
volatile char gpsBuffer2[GPSBUFFERSIZE];
// our index into filling the current line
volatile uint8_t gpsBufferPtr;
// pointers to the double buffers
volatile char *gpsNextBuffer;
volatile char *gpsLastBuffer;
volatile uint8_t gpsDataReady_;

//GPS serial data handling functions:
uint8_t gpsDataReady(void);
void GPSread(void);
char *gpsNMEA(void);
void parseGPSdata(char *gpsBuffer);

uint8_t leapyear(uint16_t y);
void uart_init(uint16_t BRR);
void gps_init(uint8_t gps);

#if (F_CPU == 16000000)
#define BRRL_4800 207    // for 16MHZ
#define BRRL_9600 103    // for 16MHZ
#define BRRL_192 52    // for 16MHZ
#elif (F_CPU == 8000000)
#define BRRL_4800 103
#define BRRL_9600 52
#define BRRL_192 26    
#endif

#endif // GPS_H_

