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

// cd_ctr.c -- CD music for the Nintendo 3DS, played from Ogg Vorbis files:
// <gamedir>/music/trackNN.ogg (falls back to id1/music), the layout the 2021
// re-release and most source ports use. Decoded with Tremor on a thread into
// NDSP channel 1 (channel 0 is the sound effects mixer, snd_ctr.c).

#include <3ds.h>
#include <stdio.h>
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#include "quakedef.h"

#define MUSIC_CHANNEL	1
#define NUM_BUFS		4
#define BUF_FRAMES		4096		// per buffer; 4 x 4096 at 44.1 kHz is ~370 ms queued

static qboolean	initialized;
static qboolean	enabled = true;
static qboolean	playing, paused, looping;
static qboolean	decoding;			// more data to queue for the current track
static byte		playTrack;
static float	cdvolume = -1;

static OggVorbis_File	vf;
static FILE		*vf_file;
static int		vf_channels;

static ndspWaveBuf	wavebuf[NUM_BUFS];
static s16		*bufdata;

static Thread		thread;
static volatile qboolean	thread_run;
static LightLock	lock;

static void CD_CloseTrack (void)
{
	ndspChnWaveBufClear (MUSIC_CHANNEL);
	if (vf_file)
	{
		ov_clear (&vf);	// closes vf_file
		vf_file = NULL;
	}
	decoding = false;
	playing = false;
	paused = false;
}

// fill one buffer from the stream; false at the end of a non-looping track
static qboolean CD_Fill (ndspWaveBuf *wb)
{
	char	*out = (char *)wb->data_pcm16;
	int		want = BUF_FRAMES * 4, got = 0, section;
	long	n;

	while (got < want)
	{
		// mono decodes into half the room, then expands to stereo in place
		n = ov_read (&vf, out + got, vf_channels == 1 ? (want - got) / 2 : want - got, &section);
		if (n > 0)
		{
			if (vf_channels == 1)
			{
				s16	*s = (s16 *)(out + got);
				int	i, frames = n / 2;
				for (i = frames - 1; i >= 0; i--)
				{
					s[i*2+1] = s[i];
					s[i*2] = s[i];
				}
				got += frames * 4;
			}
			else
				got += n & ~3;
		}
		else if (n == 0 && looping)
		{
			if (ov_pcm_seek (&vf, 0) != 0)
				break;
		}
		else
			break;	// end of track or a read error
	}

	if (got == 0)
		return false;
	if (got < want)
		memset (out + got, 0, want - got);
	wb->nsamples = got / 4;
	DSP_FlushDataCache (out, want);
	ndspChnWaveBufAdd (MUSIC_CHANNEL, wb);
	return got == want;
}

static void CD_Thread (void *arg)
{
	int	i;

	while (thread_run)
	{
		LightLock_Lock (&lock);
		if (playing && decoding && !paused)
		{
			for (i = 0; i < NUM_BUFS && decoding; i++)
			{
				if (wavebuf[i].status == NDSP_WBUF_FREE ||
					wavebuf[i].status == NDSP_WBUF_DONE)
					decoding = CD_Fill (&wavebuf[i]);
			}
		}
		LightLock_Unlock (&lock);
		svcSleepThread (20 * 1000000LL);
	}
}

static qboolean CD_OpenTrack (byte track)
{
	char	name[MAX_OSPATH];
	vorbis_info	*vi;

	snprintf (name, sizeof(name), "%s/music/track%02d.ogg", com_gamedir, track);
	vf_file = fopen (name, "rb");
	if (!vf_file)
	{
		snprintf (name, sizeof(name), "%s/music/track%02d.ogg", "./"GAMENAME, track);
		vf_file = fopen (name, "rb");
	}
	if (!vf_file)
	{
		Con_DPrintf ("CDAudio: no music file for track %u\n", track);
		return false;
	}
	if (ov_open (vf_file, &vf, NULL, 0) != 0)
	{
		Con_Printf ("CDAudio: %s is not an Ogg Vorbis file\n", name);
		fclose (vf_file);
		vf_file = NULL;
		return false;
	}
	vi = ov_info (&vf, -1);
	vf_channels = vi->channels;
	if (vf_channels != 1 && vf_channels != 2)
	{
		Con_Printf ("CDAudio: %s has %d channels\n", name, vf_channels);
		ov_clear (&vf);
		vf_file = NULL;
		return false;
	}

	ndspChnReset (MUSIC_CHANNEL);
	ndspChnSetInterp (MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate (MUSIC_CHANNEL, (float)vi->rate);
	ndspChnSetFormat (MUSIC_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
	cdvolume = -1;	// reapply the mix after the reset
	return true;
}

void CDAudio_Play (byte track, qboolean loop)
{
	int	i;

	if (!initialized || !enabled)
		return;

	LightLock_Lock (&lock);
	if (playing && playTrack == track && !paused)
	{
		LightLock_Unlock (&lock);
		return;
	}
	CD_CloseTrack ();
	if (CD_OpenTrack (track))
	{
		looping = loop;
		playTrack = track;
		playing = true;
		decoding = true;
		for (i = 0; i < NUM_BUFS && decoding; i++)	// prime the queue
			decoding = CD_Fill (&wavebuf[i]);
	}
	LightLock_Unlock (&lock);
	CDAudio_Update ();
}

void CDAudio_Stop (void)
{
	if (!initialized)
		return;
	LightLock_Lock (&lock);
	CD_CloseTrack ();
	LightLock_Unlock (&lock);
}

void CDAudio_Pause (void)
{
	if (!initialized || !playing)
		return;
	paused = true;
	ndspChnSetPaused (MUSIC_CHANNEL, true);
}

void CDAudio_Resume (void)
{
	if (!initialized || !playing)
		return;
	paused = false;
	ndspChnSetPaused (MUSIC_CHANNEL, false);
}

static void CD_f (void)
{
	char	*command;

	if (Cmd_Argc () < 2)
	{
		Con_Printf ("cd <play|loop|stop|pause|resume|on|off|info> [track]\n");
		return;
	}
	command = Cmd_Argv (1);

	if (!Q_strcasecmp (command, "on"))
		enabled = true;
	else if (!Q_strcasecmp (command, "off"))
	{
		CDAudio_Stop ();
		enabled = false;
	}
	else if (!Q_strcasecmp (command, "play"))
		CDAudio_Play ((byte)Q_atoi (Cmd_Argv (2)), false);
	else if (!Q_strcasecmp (command, "loop"))
		CDAudio_Play ((byte)Q_atoi (Cmd_Argv (2)), true);
	else if (!Q_strcasecmp (command, "stop"))
		CDAudio_Stop ();
	else if (!Q_strcasecmp (command, "pause"))
		CDAudio_Pause ();
	else if (!Q_strcasecmp (command, "resume"))
		CDAudio_Resume ();
	else if (!Q_strcasecmp (command, "info"))
	{
		if (playing)
			Con_Printf ("%s track %u%s\n", paused ? "Paused" : "Playing",
				playTrack, looping ? " (looping)" : "");
		else
			Con_Printf ("No music playing\n");
		Con_Printf ("Volume is %f\n", bgmvolume.value);
	}
}

void CDAudio_Update (void)
{
	float	mix[12];

	if (!initialized)
		return;
	if (bgmvolume.value != cdvolume)
	{
		cdvolume = bgmvolume.value;
		if (cdvolume < 0)
			cdvolume = 0;
		memset (mix, 0, sizeof(mix));
		mix[0] = mix[1] = cdvolume;
		ndspChnSetMix (MUSIC_CHANNEL, mix);
	}
}

int CDAudio_Init (void)
{
	s32	prio = 0x30;
	int	i;

	if (cls.state == ca_dedicated)
		return -1;
	if (COM_CheckParm ("-nocdaudio"))
		return -1;
	if (ndspInit () != 0)	// reference counted; snd_ctr.c already did it
	{
		Con_Printf ("CDAudio: no DSP, music disabled\n");
		return -1;
	}

	bufdata = linearAlloc (NUM_BUFS * BUF_FRAMES * 4);
	if (!bufdata)
	{
		ndspExit ();
		return -1;
	}
	memset (wavebuf, 0, sizeof(wavebuf));
	for (i = 0; i < NUM_BUFS; i++)
		wavebuf[i].data_pcm16 = bufdata + i * BUF_FRAMES * 2;

	LightLock_Init (&lock);
	svcGetThreadPriority (&prio, CUR_THREAD_HANDLE);
	thread_run = true;
	// New 3DS: decode on core 2; elsewhere share the app core at a higher
	// priority (the thread sleeps between fills)
	thread = threadCreate (CD_Thread, NULL, 32 * 1024, prio - 1, 2, false);
	if (!thread)
		thread = threadCreate (CD_Thread, NULL, 32 * 1024, prio - 1, -2, false);
	if (!thread)
	{
		Con_Printf ("CDAudio: thread creation failed, music disabled\n");
		linearFree (bufdata);
		ndspExit ();
		return -1;
	}

	Cmd_AddCommand ("cd", CD_f);
	initialized = true;
	Con_Printf ("CD Audio Initialized (Ogg Vorbis music)\n");
	return 0;
}

void CDAudio_Shutdown (void)
{
	if (!initialized)
		return;
	CDAudio_Stop ();
	thread_run = false;
	threadJoin (thread, U64_MAX);
	threadFree (thread);
	linearFree (bufdata);
	ndspExit ();
	initialized = false;
}
