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

// in_ctr.c -- for the Nintendo 3DS

#include "quakedef.h"

#include <3ds.h>
#include "ctr.h"

circlePosition cstick;
circlePosition circlepad;
touchPosition oldtouch, touch;

cvar_t  csensitivity = {"csensitivity","3", true};
cvar_t  circlepadsensitivity = {"circlepadsensitivity","10.0", true};

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

extern uint8_t keyboardToggled;

void IN_Move (usercmd_t *cmd)
{

  float speed;

  if (in_speed.state & 1)
    speed = cl_movespeedkey.value;
  else
    speed = 1;

  if(hidKeysDown() & KEY_TOUCH){
    hidTouchRead(&touch);
    oldtouch = touch;
  }

  else if(hidKeysHeld() & KEY_TOUCH && !keyboardToggled){
    hidTouchRead(&touch);
    touch.px =  (touch.px + oldtouch.px) / 2;
    touch.py =  (touch.py + oldtouch.py) / 2;
    cl.viewangles[YAW] -= (touch.px - oldtouch.px) * sensitivity.value/2;
    cl.viewangles[PITCH] += (touch.py - oldtouch.py) * sensitivity.value/2;
    oldtouch = touch;
    V_StopPitchDrift ();
  }

  hidCircleRead(&circlepad);
  //CirclePad deadzone to fix ghost movements
  if(abs(circlepad.dy) > 15){
    float y_value = circlepad.dy / 156.0f;
    cmd->forwardmove += (y_value * circlepadsensitivity.value) * speed * cl_forwardspeed.value;
  }
  if(abs(circlepad.dx) > 15){
    float x_value = circlepad.dx / 156.0f;
    if((in_strafe.state & 1) || (lookstrafe.value))
      cmd->sidemove += (x_value * circlepadsensitivity.value) * speed * cl_sidespeed.value;
    else
      cl.viewangles[YAW] -= m_side.value * x_value;
  }

  //cStick is only available on N3DS... Until libctru implements support for circlePad Pro
  if(isN3DS){

    hidCstickRead(&cstick);

    if(m_pitch.value < 0)
      cstick.dy = -cstick.dy;

    cstick.dx = abs(cstick.dx) < 10 ? 0 : cstick.dx * csensitivity.value * 0.01;
    cstick.dy = abs(cstick.dy) < 10 ? 0 : cstick.dy * csensitivity.value * 0.01;

    cl.viewangles[YAW] -= cstick.dx;
    cl.viewangles[PITCH] -= cstick.dy;
  }

  if(!lookspring.value)
    V_StopPitchDrift ();  

}
