/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

// Split from compat.h because it's only needed in one place and it was making
// the build process much slower.

#ifndef __COMPAT_SDL_H
#define __COMPAT_SDL_H

#include "compat.h"

__M_BEGIN_DECLS

#if defined(CONFIG_SDL)

#include <SDL.h>
#if defined(_WIN32) || defined(CONFIG_X11)
#include <SDL_syswm.h>
#endif

#if !SDL_VERSION_ATLEAST(2,0,0)

// This block remaps anything that is EXACTLY equivalent to its new SDL 2.0
// counterpart. More complex changes are handled with #ifdefs "in situ".

// Data types

typedef SDLKey SDL_Keycode;
typedef int (*SDL_ThreadFunction)(void *);
typedef Uint32 SDL_threadID;
// Use a macro because sdl1.2-compat typedefs SDL_Window...
#define SDL_Window void

// Macros / enumerants

#define SDLK_KP_0         SDLK_KP0
#define SDLK_KP_1         SDLK_KP1
#define SDLK_KP_2         SDLK_KP2
#define SDLK_KP_3         SDLK_KP3
#define SDLK_KP_4         SDLK_KP4
#define SDLK_KP_5         SDLK_KP5
#define SDLK_KP_6         SDLK_KP6
#define SDLK_KP_7         SDLK_KP7
#define SDLK_KP_8         SDLK_KP8
#define SDLK_KP_9         SDLK_KP9
#define SDLK_NUMLOCKCLEAR SDLK_NUMLOCK
#define SDLK_SCROLLLOCK   SDLK_SCROLLOCK
#define SDLK_PAUSE        SDLK_BREAK

#define SDL_WINDOW_FULLSCREEN  SDL_FULLSCREEN
#define SDL_WINDOW_RESIZABLE   SDL_RESIZABLE
#define SDL_WINDOW_OPENGL      SDL_OPENGL

// API functions

#define SDL_SetEventFilter(filter, userdata) SDL_SetEventFilter(filter)

#if defined(_WIN32) || defined(CONFIG_X11)
static inline SDL_bool SDL_GetWindowWMInfo(SDL_Window *window,
                                           SDL_SysWMinfo *info)
{
  return SDL_GetWMInfo(info) == 1 ? SDL_TRUE : SDL_FALSE;
}
#endif

static inline SDL_Window *SDL_GetWindowFromID(Uint32 id)
{
  return NULL;
}

static inline void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y)
{
  SDL_WarpMouse(x, y);
}

static inline void SDL_SetWindowTitle(SDL_Window *window, const char *title)
{
  SDL_WM_SetCaption(title, "");
}

static inline void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon)
{
   SDL_WM_SetIcon(icon, NULL);
}

static inline char *SDL_GetCurrentVideoDriver(void)
{
  static char namebuf[16];
  return SDL_VideoDriverName(namebuf, 16);
}

static inline int SDL_JoystickInstanceID(SDL_Joystick *joystick)
{
  return SDL_JoystickIndex(joystick);
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)
static inline int SDL_GL_SetSwapInterval(int interval)
{
#if SDL_VERSION_ATLEAST(1,2,10)
  return SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
#endif
  return 0;
}
#endif

#ifdef _WIN32
static inline HWND SDL_GetWindowProperty_HWND(SDL_Window *window)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);
  return info.window;
}
#endif /* _WIN32 */

#else /* SDL_VERSION_ATLEAST(2,0,0) */

#ifdef _WIN32
static inline HWND SDL_GetWindowProperty_HWND(SDL_Window *window)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);
  return info.info.win.window;
}
#endif /* _WIN32 */

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

#endif /* CONFIG_SDL */

__M_END_DECLS

#endif /* __COMPAT_SDL_H */
