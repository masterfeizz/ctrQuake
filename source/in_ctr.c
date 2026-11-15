/*
Copyright (C) 2015 Felipe Izzo
Copyright (C) 1996-1997 Id Software, Inc.

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

// in_ctr.c -- analog input for the Nintendo 3DS: circle pad moves, C-stick and
// touch screen look. Ported from the software renderer branch (master), which
// the hardware-rendering branch was missing (it linked in_null.c).

#include <3ds.h>
#include <stdlib.h>
#include "quakedef.h"

extern int keyboard_toggled;	// touch_ctr.c

static touchPosition oldtouch;
static qboolean touchlook;

cvar_t	csensitivity = {"csensitivity","3", true};
cvar_t	circlepadsensitivity = {"circlepadsensitivity","10.0", true};

void IN_Init (void)
{
	Cvar_RegisterVariable (&csensitivity);
	Cvar_RegisterVariable (&circlepadsensitivity);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	circlePosition	circlepad, cstick;
	touchPosition	touch;
	float	speed;

	if (in_speed.state & 1)
		speed = cl_movespeedkey.value;
	else
		speed = 1;

	// touch screen look; the right-hand column is the touch buttons
	// (touch_ctr.c), and the bottom center toggles the keyboard
	if (hidKeysDown() & KEY_TOUCH)
	{
		hidTouchRead (&touch);
		oldtouch = touch;
		touchlook = !keyboard_toggled && touch.px <= 268 &&
			!(touch.py > 214 && touch.px > 142 && touch.px < 178);
	}
	else if ((hidKeysHeld() & KEY_TOUCH) && touchlook && !keyboard_toggled)
	{
		hidTouchRead (&touch);
		touch.px = (touch.px + oldtouch.px) / 2;
		touch.py = (touch.py + oldtouch.py) / 2;
		cl.viewangles[YAW] -= (touch.px - oldtouch.px) * sensitivity.value/2;
		cl.viewangles[PITCH] += (touch.py - oldtouch.py) * sensitivity.value/2;
		oldtouch = touch;
		V_StopPitchDrift ();
	}

	// circle pad, with a deadzone against ghost movement
	hidCircleRead (&circlepad);
	if (abs(circlepad.dy) > 15)
	{
		float y_value = circlepad.dy / 156.0f;
		cmd->forwardmove += (y_value * circlepadsensitivity.value) * speed * cl_forwardspeed.value;
	}
	if (abs(circlepad.dx) > 15)
	{
		float x_value = circlepad.dx / 156.0f;
		if ((in_strafe.state & 1) || (lookstrafe.value))
			cmd->sidemove += (x_value * circlepadsensitivity.value) * speed * cl_sidespeed.value;
		else
			cl.viewangles[YAW] -= m_side.value * x_value;
	}

	// C-stick (New 3DS; reads zero elsewhere)
	hidCstickRead (&cstick);
	if (m_pitch.value < 0)
		cstick.dy = -cstick.dy;
	if (abs(cstick.dx) >= 10)
		cl.viewangles[YAW] -= cstick.dx * csensitivity.value * 0.01f;
	if (abs(cstick.dy) >= 10)
		cl.viewangles[PITCH] -= cstick.dy * csensitivity.value * 0.01f;

	if (cl.viewangles[PITCH] > 80)
		cl.viewangles[PITCH] = 80;
	if (cl.viewangles[PITCH] < -70)
		cl.viewangles[PITCH] = -70;

	if (!lookspring.value)
		V_StopPitchDrift ();
}
