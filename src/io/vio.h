/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __IO_VIO_H
#define __IO_VIO_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "memfile.h"
#include "vfile.h"

#include <stdio.h>
#include <sys/stat.h>

enum vfileflags
{
  V_SMALL_BUFFER = (1<<29), // setvbuf <= 256 for real files in binary mode.
  V_LARGE_BUFFER = (1<<30), // setvbuf >= 8192 for real files in binary mode.
};

UTILS_LIBSPEC vfile *vfopen_unsafe_ext(const char *filename, const char *mode,
 int user_flags);
UTILS_LIBSPEC vfile *vfopen_unsafe(const char *filename, const char *mode);
vfile *vfile_init_fp(FILE *fp, const char *mode);
vfile *vfile_init_mem(void *buffer, size_t size, const char *mode);
vfile *vfile_init_mem_ext(void **external_buffer, size_t *external_buffer_size,
 const char *mode);
UTILS_LIBSPEC vfile *vtempfile(size_t initial_size);
UTILS_LIBSPEC int vfclose(vfile *vf);

struct memfile *vfile_get_memfile(vfile *vf);

UTILS_LIBSPEC int vchdir(const char *path);
UTILS_LIBSPEC char *vgetcwd(char *buf, size_t size);
UTILS_LIBSPEC int vmkdir(const char *path, int mode);
UTILS_LIBSPEC int vrename(const char *oldpath, const char *newpath);
UTILS_LIBSPEC int vunlink(const char *path);
UTILS_LIBSPEC int vrmdir(const char *path);
UTILS_LIBSPEC int vaccess(const char *path, int mode);
UTILS_LIBSPEC int vstat(const char *path, struct stat *buf);

UTILS_LIBSPEC int vfgetc(vfile *vf);
UTILS_LIBSPEC int vfgetw(vfile *vf);
UTILS_LIBSPEC int vfgetd(vfile *vf);
UTILS_LIBSPEC int vfputc(int character, vfile *vf);
UTILS_LIBSPEC int vfputw(int character, vfile *vf);
UTILS_LIBSPEC int vfputd(int character, vfile *vf);
UTILS_LIBSPEC int vfread(void *dest, size_t size, size_t count, vfile *vf);
UTILS_LIBSPEC int vfwrite(const void *src, size_t size, size_t count, vfile *vf);
UTILS_LIBSPEC char *vfsafegets(char *dest, int size, vfile *vf);
UTILS_LIBSPEC int vfputs(const char *src, vfile *vf);
UTILS_LIBSPEC int vungetc(int ch, vfile *vf);
UTILS_LIBSPEC int vfseek(vfile *vf, long int offset, int whence);
UTILS_LIBSPEC long int vftell(vfile *vf);
UTILS_LIBSPEC void vrewind(vfile *vf);
UTILS_LIBSPEC long vfilelength(vfile *vf, boolean rewind);

__M_END_DECLS

#endif /* __IO_VIO_H */
