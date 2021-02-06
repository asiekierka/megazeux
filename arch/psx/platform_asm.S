  .text
  .global _start
  .extern _gp
  .extern _init
  .extern _fini
_start:
  // Set GP + SP
  la $gp, _gp
  la $sp, 0x801FFF00

  // Initialise
  jal _init
  nop

  // Main!
  li $a0, 0
  li $a1, 0
  jal main
  nop

  // Finalise
  jal _fini
  nop

  // Reset
  la $t0, 0xBFC00000
  jr $t0
  nop

  //
  // int PSX_A00_FileOpen(const char *filename, int accessmode)
  //
  .global PSX_A00_FileOpen
PSX_A00_FileOpen:
  li $v0, -2
  li $t0, 0x00A0
  li $t1, 0x00
  jr $t0
  nop

  //
  // int PSX_A01_FileSeek(int fildes, int offset, int seektype);
  //
  .global PSX_A01_FileSeek
PSX_A01_FileSeek:
  li $v0, -2
  li $t0, 0x00A0
  li $t1, 0x01
  jr $t0
  nop

  //
  // int PSX_A02_FileRead(int fildes, void *buf, size_t length);
  //
  .global PSX_A02_FileRead
PSX_A02_FileRead:
  li $v0, -2
  li $t0, 0x00A0
  li $t1, 0x02
  jr $t0
  nop

  //
  // int PSX_A03_FileWrite(int fildes, void *buf, size_t length);
  //
  .global PSX_A03_FileWrite
PSX_A03_FileWrite:
  li $v0, -2
  li $t0, 0x00A0
  li $t1, 0x03
  jr $t0
  nop

  //
  // int PSX_A04_FileClose(int fildes);
  //
  .global PSX_A04_FileClose
PSX_A04_FileClose:
  li $v0, -2
  li $t0, 0x00A0
  li $t1, 0x04
  jr $t0
  nop

  //
  // int PSX_A3C_std_out_putchar(char c);
  //
  .global PSX_A3C_std_out_putchar
PSX_A3C_std_out_putchar:
  li $t0, 0x00A0
  li $t1, 0x3C
  jr $t0
  nop

  //
  // int PSX_A3F_Printf(const char *txt, ...);
  //
  .global PSX_A3F_Printf
PSX_A3F_Printf:
  li $t0, 0x00A0
  li $t1, 0x3F
  jr $t0
  nop

  //
  // int PSX_A54_CdInit(void);
  //
  .global PSX_A54_CdInit
PSX_A54_CdInit:
  li $t0, 0x00A0
  li $t1, 0x54
  jr $t0
  nop

  //
  // int PSX_AA6_CdGetStatus(...);
  //
  .global PSX_AA6_CdGetStatus
PSX_AA6_CdGetStatus:
  li $t0, 0x00A0
  li $t1, 0xA6
  jr $t0
  nop

  //
  // int PSX_B40_chdir(const char *name);
  //
  .global PSX_B40_chdir
PSX_B40_chdir:
  li $t0, 0x00B0
  li $t1, 0x40
  jr $t0
  nop

  //
  // struct psx_dirent *PSX_B42_firstfile(const char *filename, struct psx_dirent *direntry);
  //
  .global PSX_B42_firstfile
PSX_B42_firstfile:
  li $t0, 0x00B0
  li $t1, 0x42
  jr $t0
  nop

  //
  // struct psx_dirent *PSX_B43_nextfile(struct psx_dirent *direntry);
  //
  .global PSX_B43_nextfile
PSX_B43_nextfile:
  li $t0, 0x00B0
  li $t1, 0x43
  jr $t0
  nop

  //
  // int PSX_B54_GetLastError(void);
  //
  .global PSX_B54_GetLastError
PSX_B54_GetLastError:
  li $t0, 0x00B0
  li $t1, 0x54
  jr $t0
  nop

  //
  // int PSX_SYS02_ExitCriticalSection(void);
  //
  .global PSX_SYS02_ExitCriticalSection
PSX_SYS02_ExitCriticalSection:
  li $a0, 0x0002
  syscall
  nop
  jr $ra
  nop

