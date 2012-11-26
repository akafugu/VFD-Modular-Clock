 /*
 * VFD Modular Clock
 * (C) 2011 Akafugu Corporation
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
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "display.h"
#include "rtc.h"
#ifdef FEATURE_WmGPS
#include "gps.h"
#endif
#ifdef FEATURE_FLW
#include "flw.h"
#endif

void write_vfd_iv6(uint8_t digit, uint8_t segments);
void write_vfd_iv17(uint8_t digit, uint16_t segments);
void write_vfd_iv18(uint8_t digit, uint8_t segments);
void write_vfd_iv22(uint8_t digit, uint8_t segments);

void write_vfd_8bit(uint8_t data);
void clear_display(void);

bool get_alarm_switch(void);

// see font-16seg.c
uint16_t calculate_segments_16(uint8_t character);

// see font-7seg.c
uint8_t calculate_segments_7(uint8_t character);

enum shield_t shield = SHIELD_NONE;
uint8_t digits = 6;
uint8_t mpx_count = 8;  // wm
volatile char data[10]; // Digit data (with a little extra room)
uint8_t us_counter = 0; // microsecond counter
uint8_t multiplex_counter = 0;
#ifdef FEATURE_WmGPS
uint8_t gps_counter = 0;
#endif

// globals from main.c
extern uint8_t g_show_dots;
extern uint8_t g_has_dots;
extern uint8_t g_alarm_switch;
extern uint8_t g_brightness;
extern uint8_t g_gps_enabled;
extern uint8_t g_gps_updating;
extern uint8_t g_has_eeprom;

// variables for controlling display blink
uint8_t blink;
uint16_t blink_counter = 0;
volatile uint8_t display_on = true;

// dots [bit 0~5]
uint8_t dots = 0;

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

int get_digits(void)
{
	return digits;
}

// detect which shield is connected
void detect_shield(void)
{
	// read shield bits
	uint8_t sig = 	
		(((SIGNATURE_PIN & _BV(SIGNATURE_BIT_0)) ? 0b1   : 0) |
		 ((SIGNATURE_PIN & _BV(SIGNATURE_BIT_1)) ? 0b10  : 0) |
		 ((SIGNATURE_PIN & _BV(SIGNATURE_BIT_2)) ? 0b100 : 0 ));
	// set common defaults
	mpx_count = 8;
	g_has_dots = true;
	switch (sig) {
		case(1):  // IV-17 shield
			shield = SHIELD_IV17;
			digits = 4;
			mpx_count = 4;
			g_has_dots = false;
			break;
		case(2):  // IV-6 shield
			shield = SHIELD_IV6;
			digits = 6;
			break;
		case(6):  // IV-22 shield
			shield = SHIELD_IV22;
			digits = 4;
			break;
		case(7):  // IV-18 shield (note: save value as no shield - all bits on)
			shield = SHIELD_IV18;
			digits = 8;
			mpx_count = 7; 
			break;
		default:
			shield = SHIELD_NONE;
			break;
	}
}

void display_init(uint8_t brightness)
{
	// outputs
	DATA_DDR  |= _BV(DATA_BIT);
	CLOCK_DDR |= _BV(CLOCK_BIT);
	LATCH_DDR |= _BV(LATCH_BIT);
	BLANK_DDR |= _BV(BLANK_BIT);

	// inputs
	SIGNATURE_DDR &= ~(_BV(SIGNATURE_BIT_0));
	SIGNATURE_DDR &= ~(_BV(SIGNATURE_BIT_1));
	SIGNATURE_DDR &= ~(_BV(SIGNATURE_BIT_2));
	
	// enable pullups for shield bits
	SIGNATURE_PORT |= _BV(SIGNATURE_BIT_0);
	SIGNATURE_PORT |= _BV(SIGNATURE_BIT_1);
	SIGNATURE_PORT |= _BV(SIGNATURE_BIT_2);

	LATCH_ENABLE;
	clear_display();

	detect_shield();

	// Inititalize timer for multiplexing
	TCCR0B |= (1<<CS01); // Set Prescaler to clk/8 : 1 click = 1us. CS21=1
	TIMSK0 |= (1<<TOIE0); // Enable Overflow Interrupt Enable
	TCNT0 = 0; // Initialize counter
	
	set_brightness(brightness);
}

// brightness value: 1 (low) - 10 (high)
void set_brightness(uint8_t brightness) {
	g_brightness = brightness;  // update global so it stays consistent 16nov12/wbp
	// workaround: IV17 shield not compatible with PWM dimming method
	// using simple software dimming instead
	if (shield == SHIELD_IV17) {
		return;
	}

	if (brightness > 10) brightness = 10;
	brightness = (10 - brightness) * 25; // translate to PWM value

	// Brightness is set by setting the PWM duty cycle for the blank
	// pin of the VFD driver.
	// 255 = min brightness, 0 = max brightness 
	OCR0A = brightness;

	// fast PWM, fastest clock, set OC0A (blank) on match
	TCCR0A = _BV(WGM00) | _BV(WGM01);  
 
	TCCR0A |= _BV(COM0A1);
}

void set_blink(bool OnOff)
{
	blink = OnOff;
	if (!blink) display_on = true;
}

void flash_display(uint16_t ms)  // this does not work but why???
{
	display_on = false;
	_delay_ms(ms);
	display_on = true;
}

// display multiplexing routine for 4 digits: run once every 1 ms
void display_multiplex_iv17(void)
{
	clear_display();
	switch (multiplex_counter) {
		case 0:
			write_vfd_iv17(0, calculate_segments_16(display_on ? data[0] : ' '));
			break;
		case 1:
			write_vfd_iv17(1, calculate_segments_16(display_on ? data[1] : ' '));
			break;
		case 2:
			write_vfd_iv17(2, calculate_segments_16(display_on ? data[2] : ' '));
			break;
		case 3:
			write_vfd_iv17(3, calculate_segments_16(display_on ? data[3] : ' '));
			break;
	}
	multiplex_counter++;
	// g_brightness == 1 thru 10
	if (multiplex_counter == (4 + (18 - (g_brightness-1)*2))) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 2ms
void display_multiplex_iv6(void)
{
	clear_display();
	switch (multiplex_counter) {
		case 0:
			write_vfd_iv6(0, calculate_segments_7(display_on ? data[0] : ' '));
			break;
		case 1:
			write_vfd_iv6(1, calculate_segments_7(display_on ? data[1] : ' '));
			break;
		case 2:
			write_vfd_iv6(2, calculate_segments_7(display_on ? data[2] : ' '));
			break;
		case 3:
			write_vfd_iv6(3, calculate_segments_7(display_on ? data[3] : ' '));
			break;
		case 4:
			write_vfd_iv6(4, calculate_segments_7(display_on ? data[4] : ' '));
			break;
		case 5:
			write_vfd_iv6(5, calculate_segments_7(display_on ? data[5] : ' '));
			break;
	}
	multiplex_counter++;
	if (multiplex_counter == 6) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 2ms
void display_multiplex_iv18(void)
{
	uint8_t seg = 0;
	clear_display();
	switch (multiplex_counter) {
		case 0:
			write_vfd_iv18(0, calculate_segments_7(display_on ? data[7] : ' '));
			break;
		case 1:
			write_vfd_iv18(1, calculate_segments_7(display_on ? data[6] : ' '));
			break;
		case 2:
			write_vfd_iv18(2, calculate_segments_7(display_on ? data[5] : ' '));
			break;
		case 3:
			write_vfd_iv18(3, calculate_segments_7(display_on ? data[4] : ' '));
			break;
		case 4:
			write_vfd_iv18(4, calculate_segments_7(display_on ? data[3] : ' '));
			break;
		case 5:
			write_vfd_iv18(5, calculate_segments_7(display_on ? data[2] : ' '));
			break;
		case 6:
			write_vfd_iv18(6, calculate_segments_7(display_on ? data[1] : ' '));
			break;
		case 7:
			write_vfd_iv18(7, calculate_segments_7(display_on ? data[0] : ' '));
			break;
		case 8:  // show alarm switch status
			if (g_alarm_switch)
//				write_vfd_iv18(8, (1<<7));
				seg = (1<<7);
//			else
//				write_vfd_iv18(8, 0);
			if (g_gps_updating)
				seg |= (1<<6);
			write_vfd_iv18(8, seg);
			break;
	}
	multiplex_counter++;
	if (multiplex_counter == 9) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 2ms
void display_multiplex_iv22(void)
{
	clear_display();
	switch (multiplex_counter) {
		case 0:
			write_vfd_iv22(0, calculate_segments_7(display_on ? data[0] : ' '));
			break;
		case 1:
			write_vfd_iv22(1, calculate_segments_7(display_on ? data[1] : ' '));
			break;
		case 2:
			write_vfd_iv22(2, calculate_segments_7(display_on ? data[2] : ' '));
			break;
		case 3:
			write_vfd_iv22(3, calculate_segments_7(display_on ? data[3] : ' '));
			break;
	}
	multiplex_counter++;
	if (multiplex_counter == 4) multiplex_counter = 0;
}

void display_multiplex(void)
{
	switch (shield) {
		case SHIELD_IV6:
			display_multiplex_iv6();
			break;
		case SHIELD_IV17:
			display_multiplex_iv17();
			break;
		case SHIELD_IV18:
			display_multiplex_iv18();
			break;
		case SHIELD_IV22:
			display_multiplex_iv22();
			break;
		default:
			break;
	}
}

void button_timer(void);
//uint8_t interrupt_counter = 0;  // moved to display.h
uint16_t button_counter = 0;

// 1 click = 1us. Overflow every 256 us
// (3906.25 times a second)
 
ISR(TIMER0_OVF_vect)
{
	// control blinking: on time is slightly longer than off time
	if (blink) {
		if (display_on) {
			if (++blink_counter >= 0x900) {  // on time 0.5898 seconds
				display_on = false;
				blink_counter = 0;
			}
		}
		else {  // !display_on
			if (++blink_counter >= 0x750) {  // off time 0.4792 seconds
				display_on = true;
				blink_counter = 0;
			}
		}
	}
	
	// button polling
	if (++button_counter == 71) {
		button_timer();
		button_counter = 0;
	}
	
	// display multiplex - 
//	if (++interrupt_counter == 9) {  // wm
	if (++interrupt_counter == mpx_count) {  // every 0.002048 seconds, 0.001024 for iv-17
		display_multiplex();  
		interrupt_counter = 0;
	}

#ifdef FEATURE_WmGPS
	if (++gps_counter == 4) {  // every 0.001024 seconds
		GPSread();  // check for data on the serial port
		gps_counter = 0;
	}
#endif

// IV-18 done twice??? See display_multiplex()
//	// display multiplex (IV-18 shield)  
//	if (shield == SHIELD_IV18 && ++interrupt_counter == 7) {
//		display_multiplex_iv18();
//		interrupt_counter = 0;
//	}
}

// utility functions
uint8_t print_digits(int8_t num, uint8_t offset)
{
	if (num < 0) {
		data[offset-1] = '-';  // note assumption that offset is always positive!
		num = -num;
	}
	data[offset+1] = num % 10;
	num /= 10;
	data[offset] = num % 10;
	return offset+2;
}

#ifdef skip01
uint8_t print_digits4(int num, uint8_t offset)
{
//	if (num < 0) {
//		data[offset-1] = '-';  // note assumption that offset is always positive!
//		num = -num;
//	}
	data[offset+3] = num % 10;
	num /= 10;
	data[offset+2] = num % 10;
	num /= 10;
	data[offset+1] = num % 10;
	num /= 10;
	data[offset] = num % 10;
	return offset+4;
}
#endif

uint8_t print_ch(char ch, uint8_t offset)
{
	data[offset++] = ch;
	return offset;
}

uint8_t print_hour(uint8_t num, uint8_t offset, bool _24h_clock)
{
	data[offset+1] = num % 10;  // units
	//num /= 10;
	uint8_t h2 = num / 10 % 10;  // tens
	data[offset] = h2;
	if (!_24h_clock && (h2 == 0)) {
		data[offset] = ' ';  // blank leading zero
	}
	return offset+2;
}

uint8_t print_strn(char* str, uint8_t offset, uint8_t n)
{
	uint8_t i = 0;

//	while (n-- >= 0) {
	while (n-- > 0) {
		if (str[i] == '\0') break;
		data[offset++] = str[i++];
	}

	return offset;
}

extern uint8_t g_volume;

unsigned long g_offset = 0; // offset for where to search for next word in eeprom
#ifdef FEATURE_FLW
char g_flw[6]; // contains actual four letter word
extern uint8_t g_flw_print_offset; // offset for where to start printing four letter words
#endif

uint8_t prev_sec = 0;

// set dots based on mode and seconds
void print_dots(uint8_t mode, bool _24h_clock, uint8_t seconds)
{
	if (g_show_dots) {
		if (digits == 8 && mode == 0) {
			if (_24h_clock) {
				sbi(dots, 3);
				sbi(dots, 5);
			}
			else{
				sbi(dots, 2);  // 28oct12/wbp
				sbi(dots, 4);  // 28oct12/wbp
			}
		}
		else if (digits == 6 && mode == 0) {
			sbi(dots, 1);
			sbi(dots, 3);
		}
		else if (digits == 4 && seconds % 2 && mode == 0) {
			sbi(dots, 1);
		}
	}
}

// shows time based on mode
// 4 digits: hour:min / sec
// 6 digits: hour:min:sec / hour-min
// 8 digits: hour:min:sec / hour-min-sec
void show_time(tmElements_t* te, bool _24h_clock, uint8_t mode)
{
	dots = 0;

	uint8_t offset = 0;
//	uint8_t hour = _24h_clock ? t->hour : t->twelveHour;
	uint8_t hour = te->Hour;
	bool pm = false;
	
	if (!_24h_clock) {
		if (hour == 0) {
			hour = 12;  // 12 midnight is 12 am
		}
		else if (hour == 12) {
			pm = true;  // 12 noon is 12 pm
		}
		else if (hour > 12) {
			pm = true;
			hour = hour - 12;
		}
	}	
	
	print_dots(mode, _24h_clock, te->Second);

	if (mode == 0) { // normal display mode
		if (digits == 8) { // " HH.MM.SS "
			if (_24h_clock) { 
				offset = print_ch(' ', offset);  // 28oct12/wbp  no am/pm for 24 hour
			}
			else {
				if (pm)
					offset = print_ch('P', offset);
				else
					offset = print_ch('A', offset);  // 28oct12/wbp 'A' for am
				offset = print_ch(' ', offset);  // 28oct12/wbp shift time to right 1 char
			}
			offset = print_hour(hour, offset, _24h_clock);  // wm
			offset = print_digits(te->Minute, offset);
			offset = print_digits(te->Second, offset);
			if (_24h_clock) 
				offset = print_ch(' ', offset);
		}
		else if (digits == 6) { // "HH.MM.SS"
			offset = print_hour(hour, offset, _24h_clock);  // wm
			offset = print_digits(te->Minute, offset);
			offset = print_digits(te->Second, offset);			
		}
		else { // HH.MM
			offset = print_hour(hour, offset, _24h_clock);  // wm
			offset = print_digits(te->Minute, offset);
		}
	}
	else if (mode == 1) { // extra display mode
		if (digits == 8) { // "HH-MM-SS"
			offset = print_digits(hour, offset);
			offset = print_ch('-', offset);
			offset = print_digits(te->Minute, offset);
			offset = print_ch('-', offset);
			offset = print_digits(te->Second, offset);
		}
		else if (digits == 6) { // " HH-MM"
			offset = print_digits(hour, offset);
			offset = print_ch('-', offset);
			offset = print_digits(te->Minute, offset);
			if (!_24h_clock && pm)
				offset = print_ch('P', offset);
			else
				offset = print_ch(' ', offset); 
		}
		else { // HH.MM
			if (_24h_clock) {
				offset = print_ch(' ', offset);
				offset = print_digits(te->Second, offset);
				offset = print_ch(' ', offset);
			}
			else {
				if (pm)
					offset = print_ch('P', offset);
				else
					offset = print_ch('A', offset);

				offset = print_ch('M', offset);
				offset = print_digits(te->Second, offset);
			}
		}
	}
	/*
	else if (mode == 2 && g_has_eeprom && prev_sec != te->Second) {
		g_offset = get_word(g_offset, g_flw);
		prev_sec = te->Second;
		
		if (digits == 8) {
			print_offset++;
			if (print_offset == 5) print_offset = 0;
		}
		else if (digits == 6) {
			print_offset++;
			if (print_offset == 3) print_offset = 0;
		}
		else {
			print_offset = 0;
		}
		
		data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';
		print_strn(g_flw, print_offset, 4);
	}
	*/
}

#ifdef FEATURE_FLW
// shows FLW
void show_flw(tmElements_t* te)
{
	static uint8_t print_offset = 0;

	if (g_has_eeprom && prev_sec != te->Second) {
		g_offset = get_word(g_offset, g_flw);
		prev_sec = te->Second;
		
		if (digits == 8) {
			print_offset++;
			if (print_offset == 5) print_offset = 0;
		}
		else if (digits == 6) {
			print_offset++;
			if (print_offset == 3) print_offset = 0;
		}
		else {
			print_offset = 0;
		}
		
		data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';
		print_strn(g_flw, print_offset, 4);
	}
}
#endif

// shows time - used when setting time
void show_time_setting(uint8_t hour, uint8_t min, uint8_t sec)
{
	dots = 0;
	uint8_t offset = 0;
	
	switch (digits) {
	case 8:
		offset = print_ch(' ', offset);
		offset = print_ch(' ', offset);
		// fall-through
	case 6:
		offset = print_digits(hour, offset);
		offset = print_ch('-', offset);
		offset = print_digits(min, offset);
		offset = print_ch(' ', offset);
		break;
	case 4:
		offset = print_digits(hour, offset);
		offset = print_digits(min, offset);		
	}
}

void show_temp(int8_t t, uint8_t f)
{
	dots = 0;
	
	if (digits == 6) {
		data[5] = 'C';
		
		uint16_t num = f;
		
		data[4] = num % 10;
		num /= 10;
		data[3] = num % 10;
		
		sbi(dots, 2);

		num = t;
		data[2] = num % 10;
		num /= 10;
		data[1] = num % 10;
		num /= 10;
		data[0] = ' ';
	}	
	else {
		sbi(dots, 1);		
		data[3] = 'C';
		
		uint16_t num = t*100 + f/10;
		data[2] = num % 10;
		num /= 10;
		data[1] = num % 10;
		num /= 10;
		data[0] = num % 10;
	}
}

void set_string(char* str)
{
	if (!str) return;
	dots = 0;
	data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';
	
	for (int i = 0; i <= digits-1; i++) {
		if (!*str) break;
		data[i] = *(str++);
	}
}

// shows setting string
void show_setting_string(char* short_str, char* long_str, char* value, bool show_setting)
{
	data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';
	if (get_digits() == 8) {
		set_string(short_str);
		print_strn(value, 4, 4);
	}
	else if (get_digits() == 6) {
		if (show_setting)
			print_strn(value, 2, 4);
		else
			set_string(long_str);
	}
	else {
		if (show_setting)
			print_strn(value, 0, 4);
		else
			set_string(short_str);
	}
}

#ifdef FEATURE_AUTO_DATE
// scroll the date - called every 100 ms
void show_date(tmElements_t *te_, uint8_t region, uint8_t scroll)
{
	dots = 0;
//	uint8_t di;
	char sl;
	char d[18];
	d[0] = d[1] = ' ';
	if (shield == SHIELD_IV17)
		sl = '/';
	else
		sl = '-';
	switch (region) {
		case 0:  // DMY
			d[2] = te_->Day / 10;
			d[3] = te_->Day % 10;
			d[4] = d[7] = sl;
			d[5] = te_->Month / 10;
			d[6] = te_->Month % 10;
			d[8] = '2';
			d[9] = '0';
			d[10] = te_->Year / 10;
			d[11] = te_->Year % 10;
			break;
		case 1:  // MDY
			d[2] = te_->Month / 10;
			d[3] = te_->Month % 10;
			d[4] = d[7] = sl;
			d[5] = te_->Day / 10;
			d[6] = te_->Day % 10;
			d[8] = '2';
			d[9] = '0';
			d[10] = te_->Year / 10;
			d[11] = te_->Year % 10;
			break;
		case 2:  // YMD
			d[2] = '2';
			d[3] = '0';
			d[4] = te_->Year / 10;
			d[5] = te_->Year % 10;
			d[6] = d[9] = sl;
			d[7] = te_->Month / 10;
			d[8] = te_->Month % 10;
			d[10] = te_->Day / 10;
			d[11] = te_->Day % 10;
			break;
		}
	d[12] = d[13] = ' ';
	d[14] = d[15] = d[16] = d[17] = ' ';
//	di = (scroll_ctr++) * 10 / 38;
	switch (digits) {
	case 8:
		for (uint8_t i = 0; i < 8; i++) {
			data[i] = d[(scroll+i)%18];
		}
		break;
	case 6:
		for (uint8_t i = 0; i < 6; i++) {
			data[i] = d[(scroll+i)%16];
		}
		break;
	case 4:
		for (uint8_t i = 0; i < 4; i++) {
			data[i] = d[(scroll+i)%14];
		}
		break;
	}
}
#endif

void show_setting_int(char* short_str, char* long_str, int value, bool show_setting)
{
	data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';
	if (get_digits() == 8) {
		set_string(long_str);
		print_digits(value, 6);
	}
	else if (get_digits() == 6) {
		if (show_setting)
			print_digits(value, 4);
		else
			set_string(long_str);
	}
	else {  // 4 digits
		if (show_setting)
			print_digits(value, 2);
		else
			set_string(short_str);
	}
}

#ifdef skip1
void show_setting_int4(char* short_str, char* long_str, int value, bool show_setting)
{
	data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';

	if (get_digits() == 8) {
		set_string(long_str);
		print_digits4(value, 4);
	}
	else if (get_digits() == 6) {
		if (show_setting)
			print_digits(value, 2);
		else
			set_string(short_str);
	}
	else {
		if (show_setting)
			print_digits4(value, 0);
		else
			set_string(short_str);
	}
}
#endif

void show_set_time(void)
{
	if (get_digits() == 8)
		set_string("Set Time");
	else if (get_digits() == 6)
		set_string(" Time ");
	else
		set_string("Time");
}

void show_set_alarm(void)
{
	if (get_digits() == 8)
		set_string("Set Alrm");
	else if (get_digits() == 6)
		set_string("Alarm");
	else
		set_string("Alrm");
}

void show_alarm_text(void)
{
	if (get_digits() == 8)
		set_string("Alarm   ");
	else if (get_digits() == 6)
		set_string("Alarm");
	else
		set_string("Alrm");
}

void show_alarm_time(uint8_t hour, uint8_t min, uint8_t sec)
{
	if (get_digits() == 8) {
		dots = 1<<2;
		uint8_t offset = 4;

		data[0] = 'A';
		data[1] = 'l';
		data[2] = 'r';
		data[3] = ' ';
		offset = print_digits(hour, offset);
		offset = print_digits(min, offset);		
	}
	else {
		show_time_setting(hour, min, 0);
	}
}

void show_alarm_off(void)
{
	if (get_digits() == 8) {
		set_string("Alrm off");
	}
	else {
		set_string(" off");
	}
}

// Write 8 bits to HV5812 driver
void write_vfd_8bit(uint8_t data)
{
	// shift out MSB first
	for (uint8_t i = 0; i < 8; i++)  {
		if (!!(data & (1 << (7 - i))))
			DATA_HIGH;
		else
			DATA_LOW;

		CLOCK_HIGH;
		CLOCK_LOW;
  }
}

// Writes to the HV5812 driver for IV-6
// HV1~6:   Digit grids, 6 bits
// HV7~14:  VFD segments, 8 bits
// HV15~20: NC
void write_vfd_iv6(uint8_t digit, uint8_t segments)
{
	if (dots & (1<<digit))
		segments |= (1<<7); // DP is at bit 7
	
	uint32_t val = (1 << digit) | ((uint32_t)segments << 6);
	
	write_vfd_8bit(0); // unused upper byte: for HV518P only
	write_vfd_8bit(val >> 16);
	write_vfd_8bit(val >> 8);
	write_vfd_8bit(val);
	
	LATCH_DISABLE;
	LATCH_ENABLE;	
}

// Writes to the HV5812 driver for IV-17
// HV1~4:  Digit grids, 4 bits
// HV 5~2: VFD segments, 16-bits
void write_vfd_iv17(uint8_t digit, uint16_t segments)
{
	uint32_t val = (1 << digit) | ((uint32_t)segments << 4);

	write_vfd_8bit(0); // unused upper byte: for HV518P only
	write_vfd_8bit(val >> 16);
	write_vfd_8bit(val >> 8);
	write_vfd_8bit(val);

	LATCH_DISABLE;
	LATCH_ENABLE;
}

uint32_t t;

// Writes to the HV5812 driver for IV-6
// HV1~10:  Digit grids, 10 bits
// HV11~18: VFD segments, 8 bits
// HV19~20: NC
void write_vfd_iv18(uint8_t digit, uint8_t segments)
{
	if (dots & (1<<digit))
		segments |= (1<<7); // DP is at bit 7
	
	uint32_t val = (1 << digit) | ((uint32_t)segments << 10);

	write_vfd_8bit(0); // unused upper byte: for HV518P only
	write_vfd_8bit(val >> 16);
	write_vfd_8bit(val >> 8);
	write_vfd_8bit(val);
	
	LATCH_DISABLE;
	LATCH_ENABLE;	
}


// Writes to the HV5812 driver for IV-22
// HV1~4:   Digit grids, 4 bits
// HV5~6:   NC
// HV7~14:  VFD segments, 8 bits
// HV15~20: NC
void write_vfd_iv22(uint8_t digit, uint8_t segments)
{
	uint32_t val = (1 << digit) | ((uint32_t)segments << 6);
	
	write_vfd_8bit(0); // unused upper byte: for HV518P only
	write_vfd_8bit(val >> 16);
	write_vfd_8bit(val >> 8);
	write_vfd_8bit(val);
	
	LATCH_DISABLE;
	LATCH_ENABLE;	
}

void clear_display(void)
{
	write_vfd_8bit(0);
	write_vfd_8bit(0);
	write_vfd_8bit(0);
	write_vfd_8bit(0);

	LATCH_DISABLE;
	LATCH_ENABLE;
}

void set_char_at(char c, uint8_t offset)
{
	data[offset] = c;
}

