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
#include "display.h"
#include "rtc.h"

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
volatile char data[8]; // Digit data
uint8_t us_counter = 0; // microsecond counter
uint8_t multiplex_counter = 0;

// globals from main.c
extern uint8_t g_show_dots;
extern uint8_t g_has_dots;
extern uint8_t g_alarm_switch;

// variables for controlling display blink
uint8_t blink;
uint16_t blink_counter = 0;
uint8_t display_on = 1;

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

	if (sig == 1) { // IV-17 shield
		shield = SHIELD_IV17;
		digits = 4;
		g_has_dots = false;
	}
	else if (sig == 2) { // IV-6 shield
		shield = SHIELD_IV6;
		digits = 6;
		g_has_dots = true;
	}
	else if (sig == 6) { // IV-22 shield
		shield = SHIELD_IV22;
		digits = 4;
		g_has_dots = true;
	}
	else { // IV-18 shield
		shield = SHIELD_IV18;
		digits = 8;
		g_has_dots = true;
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
	TCCR0B = (1<<CS01); // Set Prescaler to clk/8 : 1 click = 1us. CS01=1
	TIMSK0 |= (1<<TOIE0); // Enable Overflow Interrupt Enable
	TCNT0 = 0; // Initialize counter
	
	set_brightness(brightness);
}

// brightness value: 1 (low) - 10 (high)
void set_brightness(uint8_t brightness) {
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

void set_blink(bool on)
{
	blink = on;
	if (!blink) display_on = 1;
}

// display multiplexing routine for 4 digits: run once every 5us
void display_multiplex_iv17(void)
{
	if (multiplex_counter == 0) {
		clear_display();
		write_vfd_iv17(0, calculate_segments_16(display_on ? data[0] : ' '));
	}
	else if (multiplex_counter == 1) {
		clear_display();
		write_vfd_iv17(1, calculate_segments_16(display_on ? data[1] : ' '));
	}
	else if (multiplex_counter == 2) {
		clear_display();
		write_vfd_iv17(2, calculate_segments_16(display_on ? data[2] : ' '));
	}
	else if (multiplex_counter == 3) {
		clear_display();
		write_vfd_iv17(3, calculate_segments_16(display_on ? data[3] : ' '));
	}
	else {
		clear_display();
	}

	multiplex_counter++;

	if (multiplex_counter == 4) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 5us
void display_multiplex_iv6(void)
{
	if (multiplex_counter == 0) {
		clear_display();
		write_vfd_iv6(0, calculate_segments_7(display_on ? data[0] : ' '));
	}
	else if (multiplex_counter == 1) {
		clear_display();
		write_vfd_iv6(1, calculate_segments_7(display_on ? data[1] : ' '));
	}
	else if (multiplex_counter == 2) {
		clear_display();
		write_vfd_iv6(2, calculate_segments_7(display_on ? data[2] : ' '));
	}
	else if (multiplex_counter == 3) {
		clear_display();
		write_vfd_iv6(3, calculate_segments_7(display_on ? data[3] : ' '));
	}
	else if (multiplex_counter == 4) {
		clear_display();
		write_vfd_iv6(4, calculate_segments_7(display_on ? data[4] : ' '));
	}
	else if (multiplex_counter == 5) {
		clear_display();
		write_vfd_iv6(5, calculate_segments_7(display_on ? data[5] : ' '));
	}
	else {
		clear_display();
	}
	
	multiplex_counter++;
	
	if (multiplex_counter == 6) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 5us
void display_multiplex_iv18(void)
{
	if (multiplex_counter == 0) {
		clear_display();
		write_vfd_iv18(0, calculate_segments_7(display_on ? data[7] : ' '));
	}
	else if (multiplex_counter == 1) {
		clear_display();
		write_vfd_iv18(1, calculate_segments_7(display_on ? data[6] : ' '));
	}
	else if (multiplex_counter == 2) {
		clear_display();
		write_vfd_iv18(2, calculate_segments_7(display_on ? data[5] : ' '));
	}
	else if (multiplex_counter == 3) {
		clear_display();
		write_vfd_iv18(3, calculate_segments_7(display_on ? data[4] : ' '));
	}
	else if (multiplex_counter == 4) {
		clear_display();
		write_vfd_iv18(4, calculate_segments_7(display_on ? data[3] : ' '));
	}
	else if (multiplex_counter == 5) {
		clear_display();
		write_vfd_iv18(5, calculate_segments_7(display_on ? data[2] : ' '));
	}
	else if (multiplex_counter == 6) {
		clear_display();
		write_vfd_iv18(6, calculate_segments_7(display_on ? data[1] : ' '));
	}
	else if (multiplex_counter == 7) {
		clear_display();
		write_vfd_iv18(7, calculate_segments_7(display_on ? data[0] : ' '));
	}
	else if (multiplex_counter == 8) {
		clear_display();

		if (g_alarm_switch)
			write_vfd_iv18(8, (1<<7));
		else
			write_vfd_iv18(8, 0);
	}
	else {
		clear_display();
	}
	
	multiplex_counter++;
	
	if (multiplex_counter == 9) multiplex_counter = 0;
}

// display multiplexing routine for IV6 shield: run once every 5us
void display_multiplex_iv22(void)
{
	if (multiplex_counter == 0) {
		clear_display();
		write_vfd_iv6(0, calculate_segments_7(display_on ? data[0] : ' '));
	}
	else if (multiplex_counter == 1) {
		clear_display();
		write_vfd_iv6(1, calculate_segments_7(display_on ? data[1] : ' '));
	}
	else if (multiplex_counter == 2) {
		clear_display();
		write_vfd_iv6(2, calculate_segments_7(display_on ? data[2] : ' '));
	}
	else if (multiplex_counter == 3) {
		clear_display();
		write_vfd_iv6(3, calculate_segments_7(display_on ? data[3] : ' '));
	}
	else {
		clear_display();
	}

	multiplex_counter++;

	if (multiplex_counter == 4) multiplex_counter = 0;
}

void display_multiplex(void)
{
	if (shield == SHIELD_IV6)
		display_multiplex_iv6();
	else if (shield == SHIELD_IV17)
		display_multiplex_iv17();
	else if (shield == SHIELD_IV18)
		display_multiplex_iv18();
	else if (shield == SHIELD_IV22)
		display_multiplex_iv22();
}

void button_timer(void);
uint8_t interrupt_counter = 0;
uint16_t button_counter = 0;

// 1 click = 1us. Overflow every 255 us
ISR(TIMER0_OVF_vect)
{
	// control blinking: on time is slightly longer than off time
	if (blink && display_on && ++blink_counter >= 0x900) {
		display_on = false;
		blink_counter = 0;
	}
	else if (blink && !display_on && ++blink_counter >= 0x750) {
		display_on = true;
		blink_counter = 0;
	}
	
	// button polling
	if (++button_counter == 71) {
		button_timer();
		button_counter = 0;
	}
	
	// display multiplex
	if (++interrupt_counter == 9) {
		display_multiplex();
		interrupt_counter = 0;
	}
}

// utility functions
uint8_t print_digits(uint8_t num, uint8_t offset)
{
	data[offset+1] = num % 10;
	num /= 10;
	data[offset] = num % 10;
	return offset+2;
}

uint8_t print_ch(char ch, uint8_t offset)
{
	data[offset++] = ch;
	return offset;
}

uint8_t print_strn(char* str, uint8_t offset, uint8_t n)
{
	uint8_t i = 0;

	while (n-- >= 0) {
		data[offset++] = str[i++];
		if (str[i] == '\0') break;
	}

	return offset;
}

extern uint8_t g_volume;

// set dots based on mode and seconds
void print_dots(uint8_t mode, uint8_t seconds)
{
	if (g_show_dots) {
		if (digits == 8 && mode == 0) {
			sbi(dots, 3);
			sbi(dots, 5);
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
void show_time(struct tm* t, bool _24h_clock, uint8_t mode)
{
	dots = 0;

	uint8_t offset = 0;
	uint8_t hour = _24h_clock ? t->hour : t->twelveHour;

	print_dots(mode, t->sec);

	if (mode == 0) { // normal display mode
		if (digits == 8) { // " HH.MM.SS "
			if (!_24h_clock && !t->am)
				offset = print_ch('P', offset);
			else
				offset = print_ch(' ', offset); 
			offset = print_digits(hour, offset);
			offset = print_digits(t->min, offset);
			offset = print_digits(t->sec, offset);
			offset = print_ch(' ', offset);
		}
		else if (digits == 6) { // "HH.MM.SS"
			offset = print_digits(hour, offset);
			offset = print_digits(t->min, offset);
			offset = print_digits(t->sec, offset);			
		}
		else { // HH.MM
			offset = print_digits(hour, offset);
			offset = print_digits(t->min, offset);
		}
	}
	else if (mode == 1) { // extra display mode
		if (digits == 8) { // "HH-MM-SS"
			offset = print_digits(hour, offset);
			offset = print_ch('-', offset);
			offset = print_digits(t->min, offset);
			offset = print_ch('-', offset);
			offset = print_digits(t->sec, offset);
		}
		else if (digits == 6) { // " HH-MM"
			offset = print_digits(hour, offset);
			offset = print_ch('-', offset);
			offset = print_digits(t->min, offset);
			if (!_24h_clock && !t->am)
				offset = print_ch('P', offset);
			else
				offset = print_ch(' ', offset); 
		}
		else { // HH.MM
			if (_24h_clock) {
				offset = print_ch(' ', offset);
				offset = print_digits(t->sec, offset);
				offset = print_ch(' ', offset);
			}
			else {
				if (t->am)
					offset = print_ch('A', offset);
				else
					offset = print_ch('P', offset);

				offset = print_ch('M', offset);
				offset = print_digits(t->sec, offset);
			}
		}
	}
}

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
		print_strn(value, 4, 3);
	}
	else if (get_digits() == 6) {
		if (show_setting)
			print_strn(value, 2, 3);
		else
			set_string(long_str);
	}
	else {
		if (show_setting)
			print_strn(value, 0, 3);
		else
			set_string(short_str);
	}
}

void show_setting_int(char* short_str, char* long_str, int value, bool show_setting)
{
	data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = ' ';

	if (get_digits() == 8) {
		set_string(long_str);
		print_digits(value, 6);
	}
	else if (get_digits() == 6) {
		set_string(long_str);
		print_digits(value, 4);
	}
	else {
		if (show_setting)
			print_digits(value, 2);
		else
			set_string(short_str);
	}
}

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

