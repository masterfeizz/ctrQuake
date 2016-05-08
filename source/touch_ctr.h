#ifndef __TOUCH__
#define __TOUCH__

//Touchscreen mode identifiers
#define TMODE_TOUCHPAD 1
#define TMODE_KEYBOARD 2
#define TMODE_SETTINGS 3

void Touch_TouchpadTap();
void Touch_KeyboardTap();
void Touch_ProcessTap();
void Touch_DrawOverlay();
void Touch_Init();
void Touch_Update();

#endif
