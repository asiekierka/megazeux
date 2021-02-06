#ifndef __RENDER_PSX_H
#define __RENDER_PSX_H
#include <stdint.h>

#include "graphics.h"

#define PSX_PALETTE_FLAG_DIRTY 0x01
struct psx_palette_entry {
  uint8_t r, g, b;
  //uint8_t flags;
  uint8_t zero0;
} psxpal[SMZX_PAL_SIZE];

struct psx_render_data {
  struct psx_palette_entry pal[FULL_PAL_SIZE];
};

struct gpu_dma_layer_cell {
  // 0xLLNNNNNN, where:
  // LL = length of chain (min 0x00, max 0x10)
  // NNNNNN = lower 24 bits of next chain pointer,
  //  or 0xFFFFFF to denote the end of the chain
  uint32_t chain;

  // Texpage command (0xE1)
  uint32_t texpage_cmd;

  // A post-texpage NOP for safety (0x00)
  // Mednafen won't complain. Nor will any other emulator.
  // Real hardware is likely to decide not to draw the rectangle.
  uint32_t texpage_nop;

  // Solid background rectangle (0x60)
  uint32_t bgrect_cmd;
  uint32_t bgrect_pos;
  uint32_t bgrect_siz;

  // Textured coloured foreground rectangle (0x64)
  uint32_t fgrect_cmd;
  uint32_t fgrect_pos;
  uint32_t fgrect_tex;
  uint32_t fgrect_siz;
};

#endif
