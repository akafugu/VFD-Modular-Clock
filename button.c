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
#include "button.h"

bool get_alarm_switch(void)
{
	if (SWITCH_PIN & _BV(SWITCH_BIT))
		return true;
	return false;
}

uint8_t saved_keystatus = 0x00;
uint8_t keydown_keys = 0x00;
uint8_t keyup_keys = 0x00;
uint8_t keyrepeat_keys = 0x00;

uint16_t keyboard_counter[2] = {0, 0};
uint8_t button_bit[2] = { _BV(BUTTON1_BIT), _BV(BUTTON2_BIT) };

//#define REPEAT_SPEED	2000
#define REPEAT_SPEED	20

void button_timer(void)
{
	uint8_t keystatus = ~(BUTTON_PIN)&(_BV(BUTTON1_BIT) | _BV(BUTTON2_BIT));
	keydown_keys |= (uint8_t)(keystatus & ~(saved_keystatus));
	keyup_keys   |= (uint8_t)(~(keystatus) & saved_keystatus);
	saved_keystatus = keystatus;
	
	for(uint8_t i = 0; i < 2; i++)
	{
		if(~(keydown_keys)&button_bit[i])
			; // Do nothing, no keyrepeat is needed
		else if(keyup_keys&button_bit[i])
			keyboard_counter[i] = 0;
		else
		{
			if(keyboard_counter[i] >= REPEAT_SPEED)
			{
				keyrepeat_keys |= button_bit[i];
				keyboard_counter[i] = 0;
			}
			keyboard_counter[i]++;
		}
	}
}

#ifdef notused1
void get_button_state_old(struct BUTTON_STATE_OLD* button1, struct BUTTON_STATE_OLD* button2)
{
	button1->pressed = keydown_keys&_BV(BUTTON1_BIT);
	button1->released = keyup_keys&_BV(BUTTON1_BIT);
	button1->held = keyrepeat_keys&_BV(BUTTON1_BIT);
	
	// Reset if we got keyup
	if(keyup_keys&_BV(BUTTON1_BIT))
	{
		keydown_keys   &= ~(_BV(BUTTON1_BIT));
		keyup_keys     &= ~(_BV(BUTTON1_BIT));
		keyrepeat_keys &= ~(_BV(BUTTON1_BIT));
		keyboard_counter[0] = 0;
	}
	
	button2->pressed = keydown_keys&_BV(BUTTON2_BIT);
	button2->released = keyup_keys&_BV(BUTTON2_BIT);
	button2->held = keyrepeat_keys&_BV(BUTTON2_BIT);
	
	// Reset if we got keyup
	if(keyup_keys&_BV(BUTTON2_BIT))
	{
		keydown_keys   &= ~(_BV(BUTTON2_BIT));
		keyup_keys     &= ~(_BV(BUTTON2_BIT));
		keyrepeat_keys &= ~(_BV(BUTTON2_BIT));
		keyboard_counter[1] = 0;
	}
}
#endif

void get_button_state(struct BUTTON_STATE* buttons)
{
	buttons->b1_keydown = keydown_keys&_BV(BUTTON1_BIT);
	buttons->b1_keyup = keyup_keys&_BV(BUTTON1_BIT);
	buttons->b1_repeat = keyrepeat_keys&_BV(BUTTON1_BIT);
	
	// Reset if we got keyup
	if(keyup_keys&_BV(BUTTON1_BIT))
	{
		keydown_keys   &= ~(_BV(BUTTON1_BIT));
		keyup_keys     &= ~(_BV(BUTTON1_BIT));
		keyrepeat_keys &= ~(_BV(BUTTON1_BIT));
		keyboard_counter[0] = 0;
	}
	
	buttons->b2_keydown = keydown_keys&_BV(BUTTON2_BIT);
	buttons->b2_keyup = keyup_keys&_BV(BUTTON2_BIT);
	buttons->b2_repeat = keyrepeat_keys&_BV(BUTTON2_BIT);
	
	// Reset if we got keyup
	if(keyup_keys&_BV(BUTTON2_BIT))
	{
		keydown_keys   &= ~(_BV(BUTTON2_BIT));
		keyup_keys     &= ~(_BV(BUTTON2_BIT));
		keyrepeat_keys &= ~(_BV(BUTTON2_BIT));
		keyboard_counter[1] = 0;
	}

	buttons->both_held = (keydown_keys&_BV(BUTTON1_BIT)) && (keydown_keys&_BV(BUTTON2_BIT));
	//buttons->none_held = (~(saved_keystatus)&(_BV(BUTTON1_BIT) | _BV(BUTTON2_BIT)));
	buttons->none_held = ~(saved_keystatus)&(_BV(BUTTON1_BIT)) && ~(saved_keystatus)&(_BV(BUTTON2_BIT));

}
