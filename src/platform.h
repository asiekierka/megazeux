/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#ifndef __PLATFORM_H
#define __PLATFORM_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform_endian.h"

#ifdef CONFIG_SDL

#include <SDL_stdinc.h>

#else // !CONFIG_SDL

#include <inttypes.h>

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

#if defined(CONFIG_WII) || defined(CONFIG_NDS) || defined(CONFIG_3DS)
int real_main(int argc, char *argv[]);
#define main real_main
#endif // CONFIG_WII || CONFIG_NDS || CONFIG_3DS

#endif // CONFIG_SDL

/**
 * Audio, networking, and other misc. optional features require threading
 * support. Include a platform-specific thread header here if it's available.
 * If not, a dummy implementation will be included.
 *
 * Most of the dummy functions will emit errors when used.
 * If this happens, implement proper threading functions for that platform.
 */
#ifdef CONFIG_PTHREAD
#include "thread_pthread.h"
#elif defined(CONFIG_WII)
#include "../arch/wii/thread.h"
#elif defined(CONFIG_3DS)
#include "../arch/3ds/thread.h"
#elif defined(CONFIG_SDL)
#include "thread_sdl.h"
#else
#if defined(CONFIG_NDS)
#define THREAD_DUMMY_ALLOW_MUTEX
#endif
#include "thread_dummy.h"
#endif

CORE_LIBSPEC void delay(Uint32 ms);
CORE_LIBSPEC Uint32 get_ticks(void);
CORE_LIBSPEC boolean platform_init(void);
CORE_LIBSPEC void platform_quit(void);

__M_END_DECLS

#endif // __PLATFORM_H
