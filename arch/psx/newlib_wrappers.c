// Wrappers for making newlib work on a PlayStation 1

// Fun fact: The PS1 contains a POSIX subset in its operating system!
// It's buggy, but it works!
// Except when it doesn't, because it's buggy.
//
// Either way it's a good starting point for the port.

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

#include <psx_platform.h>

void nextfile_to_stat(const struct psx_dirent *dfollow,
  struct stat *restrict buf);

int __base_errno = 0;

// Party like it's 1970
// As usual, overflows every 49.7 days
uint32_t __clock_ticks = 0;

char active_cwd[DIR_NAME_MAX+1] = "/";
char temp_rel_path[DIR_NAME_MAX+1] = "";
char temp_psx_path[DIR_NAME_MAX+64+1] = "";
char temp_filename[DIR_NAME_MAX+1] = "";
extern char end[];
char *__brk_ptr = (char *)0;

// PSX supports up to 16 file handles.
uint8_t readbuf_data[16][2048];
int readbuf_fppos[16] = {
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
};
int readbuf_pos[16] = {
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
};
char readbuf_fname[16][DIR_NAME_MAX+1] = {
  "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "",
};
struct stat readbuf_stat[16];
struct stat readdir_stat;
char readdir_fname[DIR_NAME_MAX+1];

//
// Helpers
//

// NOT REENTRANT!
static const char *path_to_relative_format(const char *path) 
{
  char *d, *s;

  // FIXME: active_cwd is fucked
  strcpy(active_cwd, "/");

  temp_rel_path[0] = 0;

  // Absolute or relative?
  if(path[0] == '/')
  {
    // Absolute. This is our new cwd.
    strncpy(temp_rel_path, path, DIR_NAME_MAX);
    temp_rel_path[DIR_NAME_MAX] = 0;
  }
  else
  {
    // Relative. Copy old cwd and concatenate.
    strncpy(temp_rel_path, active_cwd, DIR_NAME_MAX);
    temp_rel_path[DIR_NAME_MAX] = 0;
    strncat(temp_rel_path, "/",
      DIR_NAME_MAX-strlen(temp_rel_path));
    strncat(temp_rel_path, path,
      DIR_NAME_MAX-strlen(temp_rel_path));
  }

  // Perform a few path strips.
  for(s = d = (temp_rel_path+1); *s != 0;)
  {
    if(d == (temp_rel_path+1) && s[0] == '.' &&
        (s[1] == '/' || s[1] == 0))
    {
      s += 2;
    }
    else if(d == (temp_rel_path+1) && s[0] == '/')
    {
      s += 1;
    }
    else if(s[0] == '/' && s[1] == '.' &&
        (s[2] == '/' || s[2] == 0))
    {
      s += 2;
    }
    else if(s[0] == '/' &&
        (s[1] == '/' || s[1] == 0))
    {
      s += 1;
    }
    else
    {
      *(d++) = *(s++);
    }
  }
  *d = 0;

  return temp_rel_path;
}

// NOT REENTRANT!
static const char *path_to_psx_format(const char *path) 
{
  char *p;

  //
  // Convert the path to a PSX `cdrom:\foo\bar` path.
  //
  path_to_relative_format(path);

  // Convert new cwd to what the PS1 expects.
  //strncpy(temp_psx_path, "bu00:", DIR_NAME_MAX);
  strncpy(temp_psx_path, "cdrom:", DIR_NAME_MAX);
  temp_psx_path[DIR_NAME_MAX] = 0;
  strncat(temp_psx_path, temp_rel_path,
    DIR_NAME_MAX-strlen(temp_psx_path));

  // TEST: mutilate the device name so it fails fast
  //temp_psx_path[4]++;

  for(p = temp_psx_path; *p != ':' && *p != 0; p++)
  {
    // skip
  }

  for(; *p != 0; p++)
  {
    *p = toupper(*p);
    if(*p == '/')
    {
      *p = '\\';
    }
  }

  return temp_psx_path;
}

//
//
//

int *__errno(void)
{
  return &__base_errno;
}

void _exit(int status)
{
  printf("EXIT %d - REBOOTING!\n", status);

  // DEBUG: keep it here
  for(;;) {}

  // Reset the PS1
  for(;;) {
    (*(void (*)(void))0xBFC00000)();
  }
}

pid_t getpid(void)
{
  return 1;
}

int gettimeofday(struct timeval *restrict tp, void *restrict tzp)
{
  (void)tzp;

  tp->tv_sec = __clock_ticks/1000;
  tp->tv_usec = (__clock_ticks%1000)*1000;

  // FIXME: actually get actual clock ticks from an ISR
  __clock_ticks += 1;

  return 0;
}

// NOT A POSIX FUNCTION. Last known sighting: SUSv2.
void *sbrk(intptr_t increment)
{
  char *old_brk;
  char *new_brk;
  char *sp;

  if((uintptr_t)__brk_ptr == (uintptr_t)0)
  {
    __brk_ptr = &end[0];
  }

  old_brk = __brk_ptr;
  new_brk = __brk_ptr + increment;

  // Read stack pointer
  asm ("move %0, $sp\n" :"=r"(sp) : : );
  sp -= 4*256; // Give a little bit of breathing room

  *(volatile uint32_t *)0x801FFFF4 = (uint32_t)new_brk;
  if(new_brk <= &end[0])
  {
    __base_errno = ENOMEM;
    *(volatile uint32_t *)0x801FFFEC = (uint32_t)sp;
    *(volatile uint32_t *)0x801FFFF0 = (uint32_t)old_brk;
    *(volatile uint32_t *)0x801FFFF4 = (uint32_t)new_brk;
    *(volatile uint32_t *)0x801FFFF8 = (uint32_t)&end[0];
    *(volatile uint32_t *)0x801FFFFC = (uint32_t)increment;
    for(;;) {}
    return (void *)-1;
  }
  else if(new_brk >= (char *)sp)
  {
    __base_errno = ENOMEM;
    for(;;) {}
    return (void *)-1;
  }
  else
  {
    __brk_ptr = new_brk;
    return old_brk;
  }
}

clock_t times(struct tms *buffer)
{
  clock_t ref_ticks = __clock_ticks;

  buffer->tms_utime = ref_ticks;
  buffer->tms_stime = 0;
  buffer->tms_cutime = ref_ticks;
  buffer->tms_cstime = 0;

  return ref_ticks;
}

// POSIX has sleep() but lacks usleep().
// It DOES have nanosleep() though.
// Using the Linux definition here.
int usleep(useconds_t usec)
{
  // TODO!
  return 0;
}

//
// File I/O
//

int close(int fildes)
{
  int result;

  fildes = LIBC_TO_PSX_FD(fildes);
  readbuf_pos[fildes] = -1;
  result = PSX_A04_FileClose(fildes);
  printf("close result %d: %d\n", fildes, result);

  if(result == -1)
  {
    // Guess an errno.
    result = PSX_B54_GetLastError();
    printf("close error %d\n", result);
    __base_errno = EIO;
    return -1;
  }
  else
  {
    // Success!
    readbuf_fname[fildes][0] = 0;
    readbuf_pos[fildes] = -1;
    return 0;
  }
}

int isatty(int fildes)
{
  if(fildes >= 0 && fildes <= 2)
  {
    return 1;
  }
  else
  {
    __base_errno = ENOTTY;
    return 0;
  }
}

off_t lseek(int fildes, off_t offset, int whence)
{
  int psx_whence;
  int result;
  int read_result;

  if(readbuf_fname[fildes][0] == 0)
  {
    __base_errno = EBADF;
    return (off_t)-1;
  }

  switch(whence)
  {
    case SEEK_SET:
      psx_whence = 0;
      break;

    // This is a pain to track.
    case SEEK_CUR:
      psx_whence = 1;
      printf("libcdbg: Seek cur hack %d %d\n", fildes, (int)offset);
      offset += readbuf_fppos[fildes];
      break;

    // According to nocash, SEEK_END is buggy.
    case SEEK_END:
      psx_whence = 0;
      printf("libcdbg: Seek end hack %d %d\n", fildes, (int)offset);
      offset = readbuf_stat[fildes].st_size - offset;
      break;

    default:
      __base_errno = EINVAL;
      return -1;
  }

  fildes = LIBC_TO_PSX_FD(fildes);
  printf("libcdbg: Seek %d %d %d\n", fildes, (int)offset, psx_whence);
  // Check if aligned
  if((offset & 2047) == 0)
  {
    // Seek is aligned. Use directly.
    result = PSX_A01_FileSeek(fildes,
      offset, psx_whence);
    printf("libcdbg: ^-- result: %d\n", result);

    readbuf_pos[fildes] = 2048;
  }
  else
  {
    // Seek is not aligned. Go back a bit, then skip forward.
    // TODO!
    result = PSX_A01_FileSeek(fildes,
      offset&~2047, psx_whence);
    printf("libcdbg: ^-- result: %d\n", result);

    // Read a sector
    printf("libcdbg: Seek Read %d\n", fildes);
    read_result = PSX_A02_FileRead(fildes, readbuf_data[fildes], 2048);
    printf("libcdbg: ^-- result: %d\n", read_result);
    readbuf_pos[fildes] = offset&2047;
    readbuf_fppos[fildes] = (result&~2047) + readbuf_pos[fildes];
    printf("libcdbg: ^-- new result: %d\n", readbuf_fppos[fildes]);
  }

  if(result == -1)
  {
    // Not defined in POSIX,
    // but none of the defined errors are generic enough.
    __base_errno = EIO;
    return -1;
  }
  else
  {
    return readbuf_fppos[fildes];
  }
}

int open(const char *path, int oflag, ...)
{
  int fildes;
  int result;
  int accessmode = 0;
  struct stat temp_stat;

  if(path == NULL)
  {
    __base_errno = EINVAL;
    return -1;
  }

  // Hardcoded stdio support.
  if(!strcmp(path, "/dev/stdin"))
  {
    return 0;
  }
  else if(!strcmp(path, "/dev/stdout"))
  {
    return 1;
  }
  else if(!strcmp(path, "/dev/stderr"))
  {
    return 2;
  }

  // File name limit check.
  if(strlen(path)+1 > DIR_NAME_MAX)
  {
    __base_errno = ENAMETOOLONG;
    return -1;
  }

  // Permissions check.
  if((oflag & O_WRONLY) != 0
    || (oflag & O_RDWR) != 0
    || (oflag & O_CREAT) != 0
    || (oflag & O_TRUNC) != 0)
  {
    __base_errno = EROFS;
    return -1;
  }

  // Convert flags.
  switch((oflag & O_ACCMODE))
  {
    case O_RDONLY:
      accessmode |= PSX_FileOpen_Read;
      break;
    case O_WRONLY:
      accessmode |= PSX_FileOpen_Write;
      break;
    case O_RDWR:
      accessmode |= PSX_FileOpen_Read;
      accessmode |= PSX_FileOpen_Write;
      break;
    default:
      // not valid
      __base_errno = EINVAL;
      return -1;
  }

  if((oflag & O_NONBLOCK) != 0)
  {
    accessmode |= PSX_FileOpen_NonBlock;
  }
  if((oflag & O_CREAT) != 0)
  {
    accessmode |= PSX_FileOpen_Create;
  }

  // Load the stat before opening.
  stat(path, &temp_stat);

  // Convert filename to PSX format.
  path_to_psx_format(path);

  // Also shove a version onto the end.
  strcat(temp_psx_path, ";1");

  // Do the syscall for real.
  printf("libcdbg: Open \"%s\" -> \"%s\"\n", path, temp_psx_path);
  fildes = PSX_A00_FileOpen(temp_psx_path, accessmode);
  printf("libcdbg: ^-- result: %d\n", fildes);
  result = PSX_B54_GetLastError();
  printf("libcdbg: open error: %d\n", result);

  // If it fails we have to give a plausible errno.
  if(fildes == -1)
  {
    __base_errno = ENOENT;
    return -1;
  }
  else
  {
    // Reset the read buffers.
    readbuf_pos[fildes] = 2048;
    readbuf_fppos[fildes] = 0;
    readbuf_fname[fildes][DIR_NAME_MAX] = 0;
    fildes = PSX_TO_LIBC_FD(fildes);
    strncpy(readbuf_fname[fildes], temp_rel_path, DIR_NAME_MAX);
    memcpy(&readbuf_stat[fildes], &temp_stat, sizeof(struct stat));
    printf("libcdbg: ^-- altered result: %d\n", fildes);
    return fildes;
  }
}

ssize_t read(int fildes, void *buf, size_t nbyte)
{
  int result;
  int bytes_to_read;
  int bytes_read = 0;
  int is_end = 0;
  char *dp = (char *)buf;

  //nbyte = 2048;
  fildes = LIBC_TO_PSX_FD(fildes);

  // Ensure the buffer is actually open!
  if(readbuf_pos[fildes] == -1)
  {
    __base_errno = EIO;
    return -1;
  }

  while((int)nbyte > 0)
  {
    if(readbuf_pos[fildes] < 2048)
    {
      bytes_to_read = 2048-readbuf_pos[fildes];
      if(bytes_to_read <= (int)nbyte)
      {
        bytes_to_read = (int)nbyte;
      }
      memcpy(dp, &readbuf_data[fildes][readbuf_pos[fildes]], bytes_to_read);
      readbuf_pos[fildes] += bytes_to_read;
      readbuf_fppos[fildes] += bytes_to_read;
      nbyte -= bytes_to_read;
      bytes_read += bytes_to_read;
      dp += bytes_to_read;
    }

    if(nbyte == 0 || is_end)
    {
      return bytes_read;
    }

    printf("libcdbg: Read %d\n", fildes);
    result = PSX_A02_FileRead(fildes, readbuf_data[fildes], 2048);
    printf("libcdbg: ^-- result: %d\n", result);

    if(result == 0)
    {
      // Probably EOF.
      readbuf_pos[fildes] = 2048;
      return bytes_read;
    }
    else if(result == -1)
    {
      readbuf_pos[fildes] = 2048;

      // If we've grabbed some things, return.
      if(bytes_read > 0)
      {
        return bytes_read;
      }

      // EIO can be set for "implementation-defined reasons".
      // So let's use that!
      result = PSX_B54_GetLastError();
      printf("libcdbg: failed - error: %d\n", result);
      __base_errno = EIO;
      return -1;
    }
    else if(result < 2048)
    {
      // Seems to be the end of the stream.
      // Move it all along and go for one last round.
      readbuf_pos[fildes] = 2048 - result;
      memmove(
        &readbuf_data[fildes][readbuf_pos[fildes]],
        &readbuf_data[fildes][0],
        result);
      is_end = 1;
    }
    else
    {
      readbuf_pos[fildes] = 0;
    }

  }

  return bytes_read;
}

ssize_t write(int fildes, const void *buf, size_t nbyte)
{
  int result;
  size_t i;

  if(fildes == 1 || fildes == 2)
  {
    // Intercept for stdout
    for(i = 0; i < nbyte; i++)
    {
      //PSX_A3C_std_out_putchar(((char *)buf)[i]);
      PSX_A3F_Printf("%c", ((char *)buf)[i]);
    }

    return nbyte;
  }

  result = PSX_A03_FileWrite(LIBC_TO_PSX_FD(fildes), buf, nbyte);

  if(result == -1)
  {
    // This should never happen.
    // Oh well, let's set an errno anyway!
    __base_errno = EIO;
    return -1;
  }
  else
  {
    return result;
  }
}


//
// File stubs
// TODO: make these work
//

void nextfile_to_stat(const struct psx_dirent *dfollow, struct stat *restrict buf)
{
  char *p;

  p = strchr(dfollow->filename, ';');

  if(p == NULL)
  {
    buf->st_mode = _IFDIR;
  }
  else
  {
    buf->st_mode = _IFREG;
    buf->st_size = 0;
  }
}

int stat(const char *restrict path, struct stat *restrict buf)
{
  struct psx_dirent dfollow_base;
  struct psx_dirent *dfollow;
  int i;

  // game requires this so pretend we have it
  printf("libcdbg: stat \"%s\"\n", path);

  path_to_psx_format(path);

  // Check if we have a stat against this
  for(i = 0; i < 16; i++)
  {
    if(!strcasecmp(readbuf_fname[i], temp_rel_path))
    {
      printf("stat cached as fd\n");
      memcpy(buf, &readbuf_stat[i], sizeof(struct stat));
      return 0;
    }
  }

  if(!strcasecmp(readdir_fname, path))
  {
    printf("stat cached as readdir\n");
    memcpy(buf, &readdir_stat, sizeof(struct stat));
    return 0;
  }

  strcat(temp_psx_path, "*"); // FIXME UNSAFE
  printf("stat attempt: \"%s\"\n", temp_psx_path);

  dfollow = &dfollow_base;
  dfollow = PSX_B42_firstfile(temp_psx_path, dfollow);
  printf("stat attempt result: %p\n", (void *)dfollow);

  if(dfollow == NULL)
  {
    __base_errno = EIO;
    return -1;
  }
  else
  {
    nextfile_to_stat(dfollow, buf);
    return 0;
  }
}

int fstat(int fildes, struct stat *buf)
{
  int result;

  if(readbuf_fname[fildes][0] == 0)
  {
    __base_errno = EBADF;
    return -1;
  }
  else
  {
    result = stat(readbuf_fname[fildes], buf);
    if(result != -1)
    {
      memcpy(&readbuf_stat[fildes], buf, sizeof(struct stat));
    }
    return result;
  }
}

//
// Directory I/O
//

int chdir(const char *path)
{
  int syscall_result;

  // Convert the path.
  path_to_psx_format(path);

  //if(temp_psx_path[strlen(temp_psx_path)-1] == '\\') { temp_psx_path[strlen(temp_psx_path)-1] = 0; }
  //strcpy(temp_psx_path, "cdrom:\\assets");

  // Call the syscall for real.
  printf("libcdbg: chdir \"%s\" -> \"%s\" -> \"%s\"\n",
    path, temp_rel_path, temp_psx_path);
  syscall_result = 0; // FIXME: make this work eventually
  //syscall_result = 1; // FIXME: make this work eventually
  //syscall_result = PSX_B40_chdir(temp_psx_path);
  printf("libcdbg: ^-- result: %d\n", syscall_result);

  if(syscall_result == 0)
  {
    // 0 indicates failure.
    // We don't know why it fails.
    // So let's guess the most plausible case.
    syscall_result = PSX_B54_GetLastError();
    printf("libcdbg: failed - error: %d\n", syscall_result);
    printf("libcdbg: failed - sanity check \"%s\"\n", active_cwd);
    __base_errno = ENOENT;
    return -1;
  }
  else
  {
    // OK, it worked.
    // Copy our new cwd across.
    printf("libcdbg: before: sanity check \"%s\"\n", active_cwd);
    strncpy(active_cwd, temp_rel_path, DIR_NAME_MAX);
    active_cwd[DIR_NAME_MAX] = 0;
    printf("libcdbg: after:  sanity check \"%s\"\n", active_cwd);
    return 0;
  }
}

char *getcwd(char *buf, size_t size)
{
  char *p;
  size_t cwd_len;

  cwd_len = strlen(active_cwd);

  if(size == 0)
  {
    __base_errno = EINVAL;
    return NULL;
  }
  
  if(size < cwd_len+1)
  {
    __base_errno = ERANGE;
    return NULL;
  }

  p = stpncpy(buf, active_cwd, size-1);
  *p = 0;

  return buf;
}

//
// Directory open stubs
// TODO: make these work
//

int closedir(DIR *dirp)
{
  if(dirp == NULL)
  {
    __base_errno = EBADF;
    return -1;
  }
  else
  {
    free(dirp);
    return 0;
  }
}

DIR *opendir(const char *dirname)
{
  DIR *dirp = malloc(sizeof(DIR));

  path_to_psx_format(dirname);
  strcpy(dirp->path, temp_rel_path); // FIXME UNSAFE
  strcat(temp_psx_path, "*"); // FIXME UNSAFE
  //printf("opendir attempt: \"%s\"\n", temp_psx_path);

  dirp->dfollow = &dirp->dfollow_base;
  dirp->dfollow = PSX_B42_firstfile(temp_psx_path, dirp->dfollow);
  printf("opendir \"%s\" -> %p\n", temp_psx_path, (void *)dirp->dfollow);

  if(dirp->dfollow == NULL)
  {
    free(dirp);
    __base_errno = EIO;
    return NULL;
  }

  return dirp;
}

struct dirent *readdir(DIR *dirp)
{
  char *p;

  if(dirp == NULL)
  {
    __base_errno = EBADF;
    return NULL;
  }
  if(dirp->dfollow == NULL)
  {
    __base_errno = ENOENT;
    return NULL;
  }

  strncpy(dirp->dirent_base.d_name,
    dirp->dfollow->filename,
    DIR_NAME_MAX);
  dirp->dirent_base.d_off = (off_t)dirp->dfollow->fileoffs;
  dirp->dirent_base.d_size = (size_t)dirp->dfollow->filesize;
  dirp->dirent_base.d_name[DIR_NAME_MAX] = 0;

  // Convert the self entry or something
  if(dirp->dirent_base.d_name[0] == 1)
  {
    strncpy(dirp->dirent_base.d_name,
      "./",
      DIR_NAME_MAX);
  }

  // Strip the ISO file version
  p = strchr(dirp->dirent_base.d_name, ';');
  if(p != NULL)
  {
    *p = 0;
  }

  //for(p = dirp->dirent_base.d_name; *p != 0; p++) { *p = tolower(*p); }

  // FIXME: handle abs and rel paths
  nextfile_to_stat(dirp->dfollow, &readdir_stat);
  strncpy(readdir_fname, dirp->dirent_base.d_name, DIR_NAME_MAX);
  readdir_fname[DIR_NAME_MAX] = 0;

  //memcpy(&last_psx_dirent, dirp->dfollow, sizeof(last_psx_dirent));
  dirp->dfollow = PSX_B43_nextfile(dirp->dfollow);
  printf("file -> \"%s\"\n", dirp->dirent_base.d_name);
  printf("readdir -> %p\n", (void *)dirp->dfollow);

  return &dirp->dirent_base;
}

void rewinddir(DIR *dirp)
{
  if(dirp == NULL)
  {
    return;
  }

  dirp->dfollow = &dirp->dfollow_base;
  dirp->dfollow = PSX_B42_firstfile(temp_psx_path, dirp->dfollow);
}

//
// Stuff that has to be a stub
//

int kill(pid_t pid, int sig)
{
  (void)pid; (void)sig;
  __base_errno = EPERM;
  return -1;
}

int link(const char *path1, const char *path2)
{
  (void)path1; (void)path2;
  __base_errno = EROFS;
  return -1;
}

int mkdir(const char *path, mode_t mode)
{
  (void)path; (void)mode;
  __base_errno = EROFS;
  return -1;
}

int rmdir(const char *path)
{
  (void)path;
  __base_errno = EROFS;
  return -1;
}

int unlink(const char *path)
{
  (void)path;
  __base_errno = EROFS;
  return -1;
}

