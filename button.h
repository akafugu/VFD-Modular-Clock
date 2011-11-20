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

#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdbool.h>

#define BUTTON_PORT PORTB
#define BUTTON_DDR  DDRB
#define BUTTON_PIN  PINB

#define BUTTON1_BIT  PB6
#define BUTTON2_BIT  PB7

#define SWITCH_PORT PORTD
#define SWITCH_DDR  DDRD
#define SWITCH_PIN  PIND
#define SWITCH_BIT  PD2

struct BUTTON_STATE
{
	bool b1_keydown : 1;
	bool b1_keyup : 1;
	bool b1_repeat : 1;
	bool b2_keydown : 1;
	bool b2_keyup : 1;
	bool b2_repeat : 1;
	
	bool both_held : 1;
	bool none_held : 1;
};

struct BUTTON_STATE_OLD
{
	bool pressed, released, held;
};

/** returns true if the alarm switch is on (right side) or false if off (left side) */
bool get_alarm_switch(void);

void get_button_state_old(struct BUTTON_STATE_OLD* button1, struct BUTTON_STATE_OLD* button2);

void get_button_state(struct BUTTON_STATE* buttons);

void button_timer(void);

#endif
