/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

#include <stdarg.h>

#include "../../src/platform.h"
#undef main

#include "../../src/util.h"
#include "../../src/event.h"
#include "../../src/graphics.h"

#include <unistd.h>
#include <stdio.h>

#include <3ds.h>
#include <citro3d.h>

#include "keyboard.h"

static u8 isNot2DS;

void delay(Uint32 ms)
{
  if(ms > 0)
  {
    svcSleepThread(1e6 * ms);
  }
}

bool ctr_is_2d(void)
{
  return isNot2DS == 0;
}

Uint32 get_ticks(void)
{
  return (Uint32) osGetTime();
}

bool platform_init(void)
{
  cfguInit();
  romfsInit();
  osSetSpeedupEnable(1);
  APT_SetAppCpuTimeLimit(80);

  gfxInitDefault();
  gfxSet3D(false);
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  CFGU_GetModelNintendo2DS(&isNot2DS);
  return true;
}

void platform_quit(void)
{
  C3D_Fini();
  gfxExit();

  romfsExit();
  cfguExit();
}

void initialize_joysticks(void)
{
  // stub - hardcoded
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
  // Since we can't warp a touchscreen stylus, focus there instead.
  focus_pixel(x, y);
}

int main(int argc, char *argv[])
{
  static char *_argv[] = {"/"};
  int retval;
  chdir("/");

  retval = real_main(1, _argv);
  return 0;
}