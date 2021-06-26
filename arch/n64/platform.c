/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#define get_ticks n64_get_ticks

#include <stdint.h>
#include <libdragon.h>

#undef get_ticks

#include "../../src/platform.h"
#include "../../src/event.h"

#ifdef CONFIG_AUDIO
#include "../../src/audio/audio.h"
#endif

#include <sys/time.h>

#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "vio_n64.h"

FILE *fopen_unsafe(const char *path, const char *mode)
{
  char p[256];
  if(mode[0] == 'w' || mode[0] == 'a')
    return NULL;
  adjust_path(p, path);
  return fopen_unsafe_unwrapped(p, mode);
}

void delay(Uint32 ms)
{
  wait_ms(ms);
}

static Uint32 last_ticks_ms = 0;
static Uint32 ticks_ms_offset = 0;

Uint32 get_ticks(void)
{
  Uint32 curr_ticks_ms = get_ticks_ms();
  if(curr_ticks_ms < last_ticks_ms)
    ticks_ms_offset += 91625;
  last_ticks_ms = curr_ticks_ms;
  return ticks_ms_offset + curr_ticks_ms;
}

boolean platform_init(void)
{
  init_interrupts();
#ifdef DEBUG
  console_init();
  console_set_render_mode(RENDER_AUTOMATIC);
#endif
  rdp_init();
  timer_init();

  dfs_init(DFS_DEFAULT_LOCATION);

  return true;
}

void platform_quit(void)
{
  while(1);
}
