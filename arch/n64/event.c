/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#include "../../src/event.h"
#include "../../src/platform.h"
#include "../../src/graphics.h"
#include "../../src/util.h"

#include "event.h"

extern struct input_status input;

void initialize_joysticks(void)
{
  controller_init();
}

boolean __peek_exit_input(void)
{
  return false;
}

void __warp_mouse(int x, int y)
{
}

boolean n64_update_input(void);

boolean __update_event_status(void)
{
  boolean retval = false;
  retval |= n64_update_input();
  return retval;
}

void __wait_event(void)
{
  while(!__update_event_status())
    {}
}

static inline boolean check_hat(struct buffered_status *status,
 Uint32 down, Uint32 up, enum joystick_hat dir)
{
  if(down)
  {
    joystick_hat_update(status, 0, dir, true);
    return true;
  }
  else

  if(up)
  {
    joystick_hat_update(status, 0, dir, false);
    return true;
  }

  return false;
}

static inline boolean check_joy(struct buffered_status *status,
  Uint32 down, Uint32 up, Uint32 code)
{
  if(down)
  {
    joystick_button_press(status, 0, code);
    return true;
  }
  else

  if(up)
  {
    joystick_button_release(status, 0, code);
    return true;
  }

  else
  {
    return false;
  }
}

boolean n64_update_input(void)
{
  struct buffered_status *status = store_status();
  struct controller_data keys_down, keys_up;

  controller_scan();
  keys_down = get_keys_down();
  keys_up = get_keys_up();

  boolean retval = false;

  retval |= check_hat(status, keys_down.c[0].up, keys_up.c[0].up, JOYHAT_UP);
  retval |= check_hat(status, keys_down.c[0].down, keys_up.c[0].down, JOYHAT_DOWN);
  retval |= check_hat(status, keys_down.c[0].left, keys_up.c[0].left, JOYHAT_LEFT);
  retval |= check_hat(status, keys_down.c[0].right, keys_up.c[0].right, JOYHAT_RIGHT);

  retval |= check_joy(status, keys_down.c[0].A, keys_up.c[0].A, 0);
  retval |= check_joy(status, keys_down.c[0].B, keys_up.c[0].B, 1);

  retval |= check_joy(status, keys_down.c[0].L, keys_up.c[0].L, 4);
  retval |= check_joy(status, keys_down.c[0].R, keys_up.c[0].R, 5);
  retval |= check_joy(status, keys_down.c[0].Z, keys_up.c[0].Z, 6);
  retval |= check_joy(status, keys_down.c[0].start, keys_up.c[0].start, 7);

  retval |= check_joy(status, keys_down.c[0].C_up, keys_up.c[0].C_up, 8);
  retval |= check_joy(status, keys_down.c[0].C_down, keys_up.c[0].C_down, 9);
  retval |= check_joy(status, keys_down.c[0].C_left, keys_up.c[0].C_left, 10);
  retval |= check_joy(status, keys_down.c[0].C_right, keys_up.c[0].C_right, 11);

  return retval;
}
