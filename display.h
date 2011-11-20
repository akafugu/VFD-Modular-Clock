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

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdbool.h>
#include <avr/io.h>

// HV5812 Data In
#define DATA_BIT PB3
#define DATA_PORT PORTB
#define DATA_DDR DDRB
#define DATA_HIGH DATA_PORT |= _BV(DATA_BIT)
#define DATA_LOW DATA_PORT &= ~(_BV(DATA_BIT))

// HV5812 Clock
#define CLOCK_BIT PB5
#define CLOCK_PORT PORTB
#define CLOCK_DDR DDRB
#define CLOCK_HIGH CLOCK_PORT |= _BV(CLOCK_BIT)
#define CLOCK_LOW CLOCK_PORT &= ~(_BV(CLOCK_BIT))

// HV5812 Latch / Strobe
#ifdef BOARD_1_0
//fixme: This feature should be removed before release: Version 1.0 of the board had LATCH at PB4.
#define LATCH_BIT PB4
#define LATCH_PORT PORTB
#define LATCH_DDR DDRB
#define LATCH_HIGH LATCH_PORT |= _BV(LATCH_BIT)
#define LATCH_LOW LATCH_PORT &= ~(_BV(LATCH_BIT))
#else
#define LATCH_BIT PC0
#define LATCH_PORT PORTC
#define LATCH_DDR DDRC
#define LATCH_HIGH LATCH_PORT |= _BV(LATCH_BIT)
#define LATCH_LOW LATCH_PORT &= ~(_BV(LATCH_BIT))
#endif

#define LATCH_ENABLE LATCH_LOW
#define LATCH_DISABLE LATCH_HIGH

// HV5812 Blank
#define BLANK_BIT PB0
#define BLANK_PORT PORTB
#define BLANK_DDR DDRB
#define BLANK_HIGH BLANK_PORT |= _BV(BLANK_BIT)
#define BLANK_LOW BLANK_PORT &= ~(_BV(BLANK_BIT))

// Shield signature
#define SIGNATURE_PORT  PORTD
#define SIGNATURE_DDR   DDRD
#define SIGNATURE_PIN   PIND
#define SIGNATURE_BIT_0 PD3
#define SIGNATURE_BIT_1 PD4
#define SIGNATURE_BIT_2 PD5

void display_init(uint8_t brightness);
int get_digits(void);
void detect_shield(void);

void set_time(uint8_t hour, uint8_t min, uint8_t sec);
void set_temp(int8_t t, uint8_t f);
void set_number(uint16_t num);
void set_char_at(char c, uint8_t offset);
void set_string(char* str);

void set_brightness(uint8_t brightness);

void set_blink(bool on);

enum shield_t {
	SHIELD_NONE = 0,
	SHIELD_IV17,
	SHIELD_IV6,
};

#endif // DISPLAY_H_
