#ifndef __PSX_PLATFORM_H
#define __PSX_PLATFORM_H

#include <dirent.h>

#define SCRATCH_u32 (*(volatile uint32_t *)0x1F800000)

#define regCD0 (*(volatile uint8_t *)0x1F801800)
#define regCD1 (*(volatile uint8_t *)0x1F801801)
#define regCD2 (*(volatile uint8_t *)0x1F801802)
#define regCD3 (*(volatile uint8_t *)0x1F801803)

#define regDMA_GPU_MADR (*(volatile uint32_t *)(0x1F801080 + 0x10*2))
#define regDMA_GPU_BCR  (*(volatile uint32_t *)(0x1F801084 + 0x10*2))
#define regDMA_GPU_CHCR (*(volatile uint32_t *)(0x1F801088 + 0x10*2))
#define regDPCR (*(volatile uint32_t *)0x1F8010F0)
#define regDICR (*(volatile uint32_t *)0x1F8010F4)

#define regGP0 (*(volatile uint32_t *)0x1F801810)
#define regGP1 (*(volatile uint32_t *)0x1F801814)
#define regGPUREAD (*(volatile const uint32_t *)0x1F801810)
#define regGPUSTAT (*(volatile const uint32_t *)0x1F801814)


// Need to make space for stderr
// Map stderr over stdout
#define PSX_TO_LIBC_FD(fd) ((fd) >= 2 ? (fd)+1 : (fd))
#define LIBC_TO_PSX_FD(fd) ((fd) >= 2 ? (fd)-1 : (fd))
//define PSX_TO_LIBC_FD(fd) fd
//define LIBC_TO_PSX_FD(fd) fd

// TODO: work out if making these all varargs also makes them safe to call
int PSX_A00_FileOpen(const char *filename, int accessmode);
#define PSX_FileOpen_Read      0x0001
#define PSX_FileOpen_Write     0x0002
#define PSX_FileOpen_NonBlock  0x0004
#define PSX_FileOpen_Create    0x0200
#define PSX_FileOpen_Async     0x8000
int PSX_A01_FileSeek(int fildes, int offset, int seektype);
int PSX_A02_FileRead(int fildes, void *buf, int length);
int PSX_A03_FileWrite(int fildes, const void *buf, int length);
int PSX_A04_FileClose(int fildes);
int PSX_A3C_std_out_putchar(char c);
int PSX_A3F_Printf(const char *txt, ...);
int PSX_A54_CdInit(void);
int PSX_AA6_CdGetStatus(void);

int PSX_B40_chdir(const char *name);
struct psx_dirent *PSX_B42_firstfile(const char *filename, struct psx_dirent *direntry);
struct psx_dirent *PSX_B43_nextfile(struct psx_dirent *direntry);
int PSX_B54_GetLastError(void);
int PSX_SYS02_ExitCriticalSection(void);

#endif

