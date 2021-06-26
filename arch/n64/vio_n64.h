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

#ifndef __IO_VIO_N64_H
#define __IO_VIO_N64_H

#include "../../src/compat.h"

__M_BEGIN_DECLS

/**
 * Standard path function wrappers.
 * On most relevant platforms these already support UTF-8.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static inline void adjust_path(char *p, const char *path)
{
  if (path[0] == 0 || !strcmp(path, "rom:/") || !strcmp(path, ".") || !strcmp(path, "./")) {
    strncpy(p, "rom://", 256);
  } else if (strstr(path, ":/") != NULL) {
    strncpy(p, path, 256);
  } else {
    sniprintf(p, 256, "rom://%s", path);
  }
}

static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
  return fopen_unsafe(path, mode);
}

static inline FILE *platform_tmpfile(void)
{
  return NULL;
}

static inline char *platform_getcwd(char *buf, size_t size)
{
  strncpy(buf, "rom:/", size);
  return buf;
}

static inline int platform_chdir(const char *path)
{
  return 0;
}

static inline int platform_mkdir(const char *path, int mode)
{
  return -1;
}

static inline int platform_rename(const char *oldpath, const char *newpath)
{
  return -1;
}

static inline int platform_unlink(const char *path)
{
  return -1;
}

static inline int platform_rmdir(const char *path)
{
  return -1;
}

static inline int platform_access(const char *path, int mode)
{
  return 0;
}

static inline int platform_stat(const char *path, struct stat *buf)
{
  char p[256];
  adjust_path(p, path);
  return stat(p, buf);
}

__M_END_DECLS

#endif /* __IO_VIO_N64_H */
