/*
Copyright (C) 2017 Felipe Izzo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include <3ds.h>

#include "keyboard_overlay_bin.h"
#include "touch_overlay_bin.h"

int keyboard_toggled;

static u16* touchpad_overlay;
static u16* keyboard_overlay;
static char last_key;
static int shift_toggled;
static u16* tfb;
static touchPosition touch;

//Keyboard is currently laid out on a 14*4 grid of 20px*20px boxes for easy(lazy) implementation
char keymap[14 * 6] = {
	K_ESCAPE , K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, 0,
	'`' , '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', K_BACKSPACE,
	K_TAB, 'q' , 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '|',
	0, 'a' , 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', K_ENTER, K_ENTER,
	K_SHIFT, 'z' , 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_UPARROW, 0,
	0, 0 , 0, 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, K_LEFTARROW, 	K_DOWNARROW, K_RIGHTARROW
};

void Touch_DrawOverlay()
{
	u16* overlay;
	int x, y;

	if(keyboard_toggled)
		overlay = keyboard_overlay;
	else
		overlay = touchpad_overlay;

	for(x = 0; x < 320; x++)
		for(y = 0; y < 240; y++)
			tfb[(x*240 + (239 - y))] = overlay[(y*320 + x)];

	if(keyboard_toggled && shift_toggled)
	{
		for(x = 26; x < 29; x++)
      		for(y = 149; y < 152; y++)
				tfb[(x*240 + (239 - y))] = RGB8_to_565(0,255,0);
	}

}

void Touch_Init()
{
	tfb = (u16*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	touchpad_overlay = (u16*)touch_overlay_bin;
	keyboard_overlay = (u16*)keyboard_overlay_bin;
	shift_toggled = 0;

	Touch_DrawOverlay();
}

void Touch_KeyboardToggle()
{
	if(keyboard_toggled)
	{
		shift_toggled = 0;
		Key_Event(K_SHIFT, false);
	}

	keyboard_toggled = !keyboard_toggled;

	Touch_DrawOverlay();
}

void Touch_TouchpadUpdate()
{
	char key = 0;

	if(touch.px > 268 && touch.py > 14 && touch.py < 226)
		key = K_AUX9 + (touch.py - 14)/42;

	if(key != last_key)
	{
		if(last_key)
			Key_Event(last_key, false);
		if(key)
			Key_Event(key, true);
	}

	last_key = key;
}

void Touch_KeyboardUpdate()
{
	char key = 0;

	if(touch.py > 62 && touch.py < 188 && touch.px > 12 && touch.px < 308)
		key = keymap[((touch.py - 62) / 21) * 14 + (touch.px - 12) / 21];

	if(key == K_SHIFT && key != last_key)
	{
		shift_toggled = !shift_toggled;
		Key_Event(K_SHIFT, shift_toggled);
		Touch_DrawOverlay();
	}

	else if(key != last_key)
	{
		if(last_key && (last_key != K_SHIFT))
			Key_Event(last_key, false);
		if(key)
			Key_Event(key, true);
	}

	last_key = key;
}

void Touch_Update()
{
	if(hidKeysHeld() & KEY_TOUCH)
	{
		hidTouchRead(&touch);

		if(!keyboard_toggled)
			Touch_TouchpadUpdate();
		else
			Touch_KeyboardUpdate();
	}

	if(hidKeysDown() & KEY_TOUCH)
	{
		if (touch.py > 214 && touch.px > 142 && touch.px < 178)
			Touch_KeyboardToggle();
	}

	if(hidKeysUp() & KEY_TOUCH)
	{
		if(last_key && (last_key != K_SHIFT))
		{
			Key_Event(last_key, false);
			last_key = 0;
		}
	}
}