/*
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

#include "quakedef.h"
#include "d_local.h"

#include <3ds.h>

viddef_t	vid;				// global video state

#define	BASEWIDTH	400
#define	BASEHEIGHT	240

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[512*1024];

uint16_t  	*framebuffer;
uint16_t 	d_8to16table[256];

void	VID_SetPalette (unsigned char *palette)
{
	int i;
	unsigned char r, g, b;

	if(palette==NULL)
		return;

	for(i = 0; i < 256; i++){
		r = palette[i*3+0];
		g = palette[i*3+1];
		b = palette[i*3+2];
		d_8to16table[i] = RGB8_to_565(r,g,b);
	}
}

void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{	
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.width = vid.conwidth = BASEWIDTH;
	vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;

	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));

	VID_SetPalette(palette);

	framebuffer = (uint16_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
}

void	VID_Shutdown (void)
{
}

void	VID_Update (vrect_t *rects)
{
	int x, y;
	for(x=0; x<400; x++)
	{
		for(y=0; y<240;y++)
		{
			framebuffer[(x*240 + (239 -y))] = d_8to16table[vid.buffer[y*400 + x]];
		}
	}
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


