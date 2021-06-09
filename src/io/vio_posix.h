/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __IO_VIO_POSIX_H
#define __IO_VIO_POSIX_H

#include "../compat.h"

__M_BEGIN_DECLS

/**
 * Standard path function wrappers.
 * On most relevant platforms these already support UTF-8.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
  return fopen_unsafe(path, mode);
}

static inline FILE *platform_tmpfile(void)
{
#if defined(CONFIG_PSX) || defined(CONFIG_NDS)
  // tmpfile() pulls in the non-integer-only variant of sprintf from newlib,
  // while the only functions currently using tmpfile() have a fallback.
  return NULL;
#else
  return tmpfile();
#endif /* CONFIG_PSX || CONFIG_NDS */
}

static inline char *platform_getcwd(char *buf, size_t size)
{
  return getcwd(buf, size);
}

static inline int platform_chdir(const char *path)
{
  return chdir(path);
}

static inline int platform_mkdir(const char *path, int mode)
{
  return mkdir(path, mode);
}

static inline int platform_rename(const char *oldpath, const char *newpath)
{
  return rename(oldpath, newpath);
}

static inline int platform_unlink(const char *path)
{
  return unlink(path);
}

static inline int platform_rmdir(const char *path)
{
  return rmdir(path);
}

static inline int platform_access(const char *path, int mode)
{
#ifdef CONFIG_PSX
  return 0;
#else
  return access(path, mode);
#endif
}

static inline int platform_stat(const char *path, struct stat *buf)
{
  return stat(path, buf);
}

__M_END_DECLS

#endif /* __IO_VIO_POSIX_H */
