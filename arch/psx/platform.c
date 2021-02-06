/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018 Ben Russell
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "platform.h"
#include "event.h"

#ifdef CONFIG_AUDIO
#include "audio/audio.h"
#endif

#include <sys/time.h>

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <psx_platform.h>

// Don't use MZX's fopen wrapper.
#ifdef fopen
#undef fopen
#endif

void delay(Uint32 ms)
{
  usleep(1000 * ms);
}

Uint32 get_ticks(void)
{
  struct timeval tv;

  if(gettimeofday(&tv, NULL) < 0)
  {
    perror("gettimeofday");
    return 0;
  }

  return (Uint32)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

bool platform_init(void)
{
  struct psx_dirent *dfollow;
  struct psx_dirent dfollow_buf;

  // Enable interrupts
  PSX_SYS02_ExitCriticalSection();

  // TEST: firstfile/nextfile
  printf("Doing a firstfile...\n");
  for(dfollow = PSX_B42_firstfile("cdrom:\\A\\*", &dfollow_buf); dfollow != NULL;
      dfollow = PSX_B43_nextfile(dfollow))
  {
    // Skip the ".." entry
    if(dfollow->filename[0] == 1) { continue; }

    printf("File: \"%s\" %08X %08X %08X\n",
      dfollow->filename,
      (unsigned int)dfollow->fileoffs,
      (unsigned int)dfollow->filesize,
      (unsigned int)dfollow->fileattr);
  }

  return true;
}

void platform_quit(void)
{
  // stub
  _exit(0);
}

void initialize_joysticks(void)
{
  // stub
}

bool __update_event_status(void)
{
  // stub
  return false;
}

void real_warp_mouse(int x, int y)
{
  // stub
  // Supporting the PS1 mouse might be fun...
  // What's worse, the mouse, or the G-Con?
}

void __wait_event(int timeout)
{
  // stub
}

#ifdef CONFIG_AUDIO

void init_audio_platform(struct config_info *conf)
{
  // stub
}

void quit_audio_platform(void)
{
  // stub
}

#endif // CONFIG_AUDIO
