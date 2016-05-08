/*
Copyright (C) 2015 Felipe Izzo

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

//FIX ME: load all hardcoded values from file

#include "quakedef.h"

#include <3ds.h>
#include "touch_ctr.h"

//Keyboard is currently laid out on a 14*4 grid of 20px*20px boxes for lazy implementation
char keymap[14 * 6] = {
  K_ESCAPE , K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, 0,
  '`' , '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', K_BACKSPACE,
  K_TAB, 'q' , 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '|',
  0, 'a' , 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', K_ENTER, K_ENTER,
  K_SHIFT, 'z' , 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_UPARROW, 0,
  0, 0 , 0, 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, K_LEFTARROW, 	K_DOWNARROW, K_RIGHTARROW
};

u16* touchOverlay;
u16* keyboardOverlay;
uint8_t keyboardToggled;
char lastKey = 0;
int tmode;
u16* tfb;
touchPosition oldtouch, touch;
u64 tick;

u64 lastTap = 0;

int shiftToggle = 0;

void Touch_Init(){
  tmode = TMODE_TOUCHPAD; //Start in touchpad Mode
  tfb = (u16*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

  //Load overlay files from sdmc for easier testing
  FILE *texture = fopen("touchOverlay.bin", "rb");
  if(!texture)
    Sys_Error("Could not open touchpadOverlay.bin\n");
  fseek(texture, 0, SEEK_END);
  int size = ftell(texture);
  fseek(texture, 0, SEEK_SET);
  touchOverlay = malloc(size);
  fread(touchOverlay, 1, size, texture);
  fclose(texture);

  texture = fopen("keyboardOverlay.bin", "rb");
  if(!texture)
    Sys_Error("Could not open keyboardOverlay.bin\n");
  fseek(texture, 0, SEEK_END);
  size = ftell(texture);
  fseek(texture, 0, SEEK_SET);
  keyboardOverlay = malloc(size);
  fread(keyboardOverlay, 1, size, texture);
  fclose(texture);
}

void Touch_DrawOverlay()
{
  int x, y;
  for(x=0; x<320; x++){
    for(y=0; y<240;y++){
      tfb[(x*240 + (239 - y))] = touchOverlay[(y*320 + x)];
    }
  }
  if(keyboardToggled)
    Touch_DrawKeyboard();
}

void Touch_DrawKeyboard()
{
  int x, y;
  for(x=0; x<320; x++){
    for(y=0; y<240;y++){
      tfb[(x*240 + (239 - y))] = keyboardOverlay[(y*320 + x)];
    }
  }
  if(shiftToggle)
  {
    for(x=26; x<29; x++){
      for(y=149; y<152;y++){
        tfb[((x)*240 + (239 - (y)))] = RGB8_to_565(0,255,0);
      }
    }
  }
}

void Touch_Update(){
  if(lastKey){
    Key_Event(lastKey, false);
    lastKey = 0;
  }

  if(hidKeysDown() & KEY_TOUCH){
    hidTouchRead(&touch);
    tick = Sys_FloatTime();
  }

  //If touchscreen is released in certain amount of time it's a tap
  if(hidKeysUp() & KEY_TOUCH){
    if((Sys_FloatTime() - tick) < 1.0) //FIX ME: find optimal timeframe
      Touch_ProcessTap();
  }
}

void Touch_KeyboardToggle()
{
  if(keyboardToggled)
    shiftToggle = 0;
    Key_Event(K_SHIFT,false);

  keyboardToggled = !keyboardToggled;

  Touch_DrawOverlay();
}

void Touch_ProcessTap()
{
  if(touch.px > 268 && touch.py > 14 && touch.py < 226 )
    Touch_TopBarTap();
  else if (touch.py > 62 && touch.py < 188 && touch.px > 12 && touch.px < 308 && keyboardToggled)
    Touch_KeyboardTap();
  else if (touch.py > 214 && touch.px > 142 && touch.px < 178)
    Touch_KeyboardToggle();
}

void Touch_TopBarTap()
{
  uint16_t y = (touch.py - 14)/42;
  lastKey = K_AUX9 + y;
  Key_Event(lastKey, true);
}

void Touch_KeyboardTap()
{
  char key = keymap[((touch.py - 62)/21) * 14 + (touch.px - 12)/21];
  if(key == K_SHIFT){
    shiftToggle = !shiftToggle;
    Key_Event(K_SHIFT,shiftToggle);
    Touch_DrawOverlay();
  }
  else {
    Key_Event(key, true);
    lastKey = key;
  }
}