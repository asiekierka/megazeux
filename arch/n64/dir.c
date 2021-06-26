/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

/**
 * MZX dirent wrapper with several platform-specific hacks.
 *
 * These are a bit more involved than the stdio wrappers and require fields in
 * the mzx_dir struct, so the platform hacks are here instead of in the vfile
 * platform function headers.
 */

#include <stdlib.h>
#include <dir.h>

#include "../../src/io/dir.h"
#include "../../src/util.h"
#include "vio_n64.h"

static inline boolean platform_opendir(struct mzx_dir *dir, const char *path)
{
  char p[256];
  adjust_path(p, path);

  dir_t *d = malloc(sizeof(dir_t));
  if(!d)
    return false;

  sniprintf(dir->path, PATH_BUF_LEN, "%s", p);
  dir->path[PATH_BUF_LEN - 1] = 0;

  dir->opaque = d;
  return true;
}

static inline void platform_closedir(struct mzx_dir *dir)
{
  free(dir->opaque);
}

static inline boolean platform_readdir(struct mzx_dir *dir,
 char *dest, size_t dest_len, int *type)
{
  dir_t *d = dir->opaque;
  int ret;

  // TODO: hack (like many in the N64 port)
  int tgt_pos = dir->pos;
  dir->pos = 0;
  for(; dir->pos <= tgt_pos; dir->pos++)
  {
    if(dir->pos > 0)
      ret = dir_findnext(dir->path, d);
    else
      ret = dir_findfirst(dir->path, d);
    if (ret < 0)
    {
      if(dest)
        dest[0] = 0;
      return false;
    }
  }
  if(type)
    *type = d->d_type == DT_DIR ? DIR_TYPE_DIR : DIR_TYPE_FILE;

  if(dest)
  {
    sniprintf(dest, dest_len, "%s", d->d_name);
#ifdef DEBUG
    iprintf("dir test: %s %d (%d/%d)\n", d->d_name, d->d_type, dir->pos, dir->entries);
#endif
  }

  return true;
}

static inline boolean platform_rewinddir(struct mzx_dir *dir)
{
  dir->pos = 0;
  return true;
}

long dir_tell(struct mzx_dir *dir)
{
  return dir->pos;
}

boolean dir_open(struct mzx_dir *dir, const char *path)
{
  if(!platform_opendir(dir, path))
    return false;

  dir->pos = 0;
  dir->entries = 0;
  if(dir_findfirst(dir->path, dir->opaque) >= 0)
  {
    dir->entries++;
    while(dir_findnext(dir->path, dir->opaque) >= 0)
      dir->entries++;
  }

  dir->pos = 0;
  return true;
}

void dir_close(struct mzx_dir *dir)
{
  if(dir->opaque)
  {
    platform_closedir(dir);
    dir->opaque = NULL;
    dir->entries = 0;
    dir->pos = 0;
  }
}

void dir_seek(struct mzx_dir *dir, long offset)
{
  long i;

  if(!dir->opaque)
    return;

  dir->pos = 0;
  for(i = 0; i < offset; i++)
    platform_readdir(dir, NULL, 0, NULL);
}

boolean dir_get_next_entry(struct mzx_dir *dir, char *entry, int *type)
{
  if(!dir->opaque)
    return false;

  return platform_readdir(dir, entry, PATH_BUF_LEN, type);
}
