#ifndef __DIRENT_H
#define __DIRENT_H

#include <stdint.h>
#include <sys/types.h>

#define DIR_NAME_MAX 63

struct dirent {
  off_t d_off;
  size_t d_size;
  //int d_type;
  char d_name[DIR_NAME_MAX+1];
};

struct psx_dirent {
  char filename[0x14];
  uint32_t fileattr;
  uint32_t filesize;
  uint32_t _reserved1;
  uint32_t fileoffs;
  uint32_t _reserved2;
};

struct DIR_s {
  char base_drive[16];
  char path[DIR_NAME_MAX];
  struct psx_dirent dfollow_base;
  struct psx_dirent *dfollow;
  struct dirent dirent_base;
};
typedef struct DIR_s DIR;


// POSIX definitions
int closedir(DIR *dirp);
DIR *opendir(const char *dirname);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);

//int readdir_r(DIR *restrict dirp, struct dirent *restrict entry,
//   struct dirent **restrict result);

#endif
