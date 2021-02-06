/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018 Ben Russell
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

#include <string.h>

#include <psx_platform.h>

#include "graphics.h"
#include "render.h"
#include "render_psx.h"
#include "renderers.h"
#include "util.h"

//static uint32_t psx_flip_mask = 0x00FFFFFF;
static uint32_t psx_flip_mask = 0x00000000;

static uint32_t *gpu_dma_write_ptr = NULL;
// With a bit of padding...
static uint32_t gpu_dma_buffers[2][80*25*sizeof(struct gpu_dma_layer_cell)/sizeof(uint32_t)+256];
static uint32_t gpu_dma_index = 0;
static bool gpu_dma_is_seeded[2];

#define GWAIT() {}
#define GPTR() gpu_dma_write_ptr
#define GADD(x) \
  *(gpu_dma_write_ptr++) = (x)
#define GFULLSTART() \
  { \
    gpu_dma_write_ptr = ( \
      gpu_dma_buffers[gpu_dma_index]); \
  }
#define GFULLEND() \
  { \
    while((regDMA_GPU_CHCR & (1<<24)) != 0) \
      {} \
    regDMA_GPU_BCR = 0; \
    regDMA_GPU_MADR = 0xFFFFFF&(uint32_t)&gpu_dma_buffers[gpu_dma_index];\
    regDICR &= ~(1<<(16+2)); \
    regDICR |= (1<<(24+2)); \
    regDPCR |= (8<<(2*4)); \
    regDMA_GPU_CHCR = 0x01000401; \
    gpu_dma_index ^= 1; \
  }


static bool psxgpu_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool psxgpu_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  //struct psx_render_data *render_data = graphics->render_data;

  // TODO: actually read the info fed in.

  // NOTE: Dimensions defined in terms of 640x240p mode!
  // 640 wide = 4 clocks per pixel
  // Vertically, there's 240p and 480i.

  const int disp_width = 640;
  const int disp_height = 175;
  const int disp_half_width = disp_width/2;
  const int disp_half_height = disp_height/2;

  // Set up screen
  regGP1 = 0x00000000; // Reset GPU
  regGP1 = 0x08000027; // Video mode: 640x480i 15bpp NTSC
  regGP1 = (0x05<<24)
    | (0<<10) | (0); // Display start address
  regGP1 = (0x06<<24)
    | ((0x260+4*(320-disp_half_width+disp_width))<<12)
    | ((0x260+4*(320-disp_half_width))); // Horizontal
  regGP1 = (0x07<<24)
    | ((0x88-disp_half_height+disp_height)<<10)
    | ((0x88-disp_half_height)); // Vertical
  regGP0 = 0xE1000600; // Texpage: Enable dither + draw-on-front-buffer
  regGP0 = 0xE2000000; // Texwindow: Enable full area
  regGP0 = (0xE3<<24)
    | ((0)<<10) | ((0)); // Drawing top-left corner
  regGP0 = (0xE4<<24)
    | ((350-1)<<10) | ((640-1)); // Drawing bottom-right corner
  regGP0 = (0xE5<<24)
    | ((0)<<10) | ((0)); // Drawing offset

  // Set up a base palette
  regGP0 = 0xA0000000;
  regGP0 = ((0)<<16)|(640);
  regGP0 = ((1)<<16)|(16);
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;
  regGP0 = 0x7FFF0000;

  // FILL IT WITH MAGIC PINK (toned down edition)
  regGP0 = 0x02550055;
  regGP0 = ((0)<<16) | ((0));
  regGP0 = ((350)<<16) | ((640));

  regGP1 = 0x03000000; // Display enable

  return true;
}

static bool psxgpu_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct psx_render_data render_data;

  memset(&render_data, 0, sizeof(struct psx_render_data));
  graphics->render_data = &render_data;

  // We only support 640x350 right now
  graphics->allow_resize = 0;
  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  // Ignored.
  //
  // Could be used for allowing 24bpp as well as 15bpp rendering?
  //
  // But 24bpp can really only be done in software OR with weird hacks,
  // and we'll have very little VRAM left.
  graphics->bits_per_pixel = 16;

  return set_video_mode();
}

static void psxgpu_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct psx_render_data *render_data = graphics->render_data;
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    render_data->pal[i].r = palette[i].r;
    render_data->pal[i].g = palette[i].g;
    render_data->pal[i].b = palette[i].b;
    //render_data->pal[i].flags |= PSX_PALETTE_FLAG_DIRTY;
    render_data->pal[i].zero0 = 0;
  }
}

static void psxgpu_render_graph(struct graphics_data *graphics)
{
  struct psx_render_data *render_data = graphics->render_data;
  int cx, cy;
  int tx, ty;
  struct char_element *cp = graphics->text_video;
  struct psx_palette_entry *pal = render_data->pal;
  struct psx_palette_entry *cobg, *cofg;
  struct gpu_dma_layer_cell *gptr;
  uint32_t chain_ptr_value;
  uint32_t cmd_bgrect_0;
  uint32_t cmd_rect_pos;
  uint32_t cmd_rect_siz;
  uint32_t cmd_texpage_0;
  uint32_t cmd_fgrect_0;
  uint32_t cmd_fgrect_2;

  //printf("PSX: Render graph\n");

  GFULLSTART();

  if(!gpu_dma_is_seeded[gpu_dma_index])
  {
    // Preseed our rendering layer with stuff that isn't going to change if we haven't done this already.
    gptr = (struct gpu_dma_layer_cell *)gpu_dma_write_ptr;
    chain_ptr_value = (((sizeof(*gptr)/4)-1)<<24)
      |(0xFFFFFF&(
        ((uint32_t)(GPTR()))
        +sizeof(*gptr)));
    cmd_rect_siz = ((14<<16)|8);

    for(cy = 0; cy < 25; cy++)
    {
      cmd_rect_pos = (((cy*14)<<16)|(0*8));
      for(cx = 0; cx < 80; cx++, gptr++, chain_ptr_value += sizeof(*gptr), cmd_rect_pos += 8)
      {
        gptr->chain = chain_ptr_value;
        gptr->texpage_cmd = 0x00000000;
        gptr->texpage_nop = 0x00000000;
        gptr->bgrect_cmd = 0x60000000;
        gptr->bgrect_pos = cmd_rect_pos;
        gptr->bgrect_siz = cmd_rect_siz;
        gptr->fgrect_cmd = 0x64000000;
        gptr->fgrect_pos = cmd_rect_pos;
        gptr->fgrect_tex = 0x00000000;
        gptr->fgrect_siz = cmd_rect_siz;
      }
    }

    gpu_dma_is_seeded[gpu_dma_index] = true;
  }

  // Now fill with this frame's data
  gptr = (struct gpu_dma_layer_cell *)gpu_dma_write_ptr;
  for(cy = 0; cy < 25; cy++)
  {
    for(cx = 0; cx < 80; cx++, cp++, gptr++)
    {
      tx = ((cp->char_value&0x1FF));
      ty = ((cp->char_value&0xFFF)>>9)*14+350-0x100;

      // Draw a rectangle for the background
      // Draw a texture for the foreground
      cobg = &pal[cp->bg_color];
      cofg = &pal[cp->fg_color];
      cmd_bgrect_0 = ((0x60<<24)|*(uint32_t *)cobg) ^ psx_flip_mask;
      cmd_fgrect_0 = (((0x64<<24)|*(uint32_t *)cofg) ^ psx_flip_mask);

      // Texpage:
      // + Enable dither
      // + Enable draw-on-front-buffer
      // + 4bpp
      // + Set texture base X + Y
      cmd_texpage_0 = (0xE1000610
        | ((tx>>5)&0xF));

      // Foreground rectangle texcoords
      cmd_fgrect_2 = ((0<<22)
        |(40<<16)
        |((ty&0xFF)<<8)
        |((tx&0x1F)*8));

      gptr->texpage_cmd = cmd_texpage_0;
      gptr->bgrect_cmd = cmd_bgrect_0;
      gptr->fgrect_cmd = cmd_fgrect_0;
      gptr->fgrect_tex = cmd_fgrect_2;
    }
  }

  // Mark end and send
  gptr[-1].chain |= 0x00FFFFFF;
  GFULLEND();

  // Speed test, EPILEPSY WARNING
  //psx_flip_mask += 0x00020202;
  //psx_flip_mask &= 0x00FEFEFF;
}

static void psxgpu_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
}

static void psxgpu_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
}

static void psxgpu_sync_screen(struct graphics_data *graphics)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
  //psxgpu_render_graph(graphics);
}


static void psxgpu_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
}

static void psxgpu_remap_charsets(struct graphics_data *graphics)
{
  int i;
  int x, y, timeout;
  int cy;
  uint32_t char_data;
  uint32_t init_char_data;
  uint8_t *cptr = graphics->charset;
  char ch;

  //struct psx_render_data *render_data = graphics->render_data;
  // TODO: DMA it (sums up the whole renderer really)
  printf("PSX: Remap charsets\n");
  for(i = 0; i < CHARSET_SIZE * NUM_CHARSETS; i++)
  //for(i = 0; i < CHARSET_SIZE; i++)
  {
    x = (i&((1<<9)-1))*2; y = (i>>9)*14+350;
    //x = (i%320)*2; y = (i/320)*14;

    init_char_data = 0x00000000;
    //init_char_data = 0x80008000;
    //if((((x>>1)^(y/14))&1) != 0) { init_char_data ^= 0x3C003C00; }
    //if((i&0x100) != 0) { init_char_data ^= 0x00070007; }
    //if((i&0x800) != 0) { init_char_data ^= 0x00E000E0; }

    timeout = 100;
    while((regGPUSTAT & (1<<28)) == 0 && (timeout-- > 0))
    {
      asm volatile ("" : : :"memory"); // barrier
    }

    // 4bpp mapping idea:
    // Suppose we have, from left to right:
    // ABCDEFGH - 1bpp version
    // abcdefgh - 2bpp (SMZX) version
    // Now suppose Y is 0 for even-numbered Y positions,
    // and 1 for odd-numbered positions...
    // Then we can map the pixels like so:
    // YabA YabB YcdC YcdD YefE YefF YghG YghH
    regGP0 = 0xA0000000;
    regGP0 = ((y)<<16)|(x); // Dest pos
    regGP0 = (14<<16)|(2); // Size in halfwords

    for(cy = 0; cy < 14; cy++)
    {
      char_data = init_char_data;
      ch = *cptr;
      //ch ^= cy|(1<<cy); // TEST! FIXME: get file I/O working so the charset can load
      //ch ^= i>>(cy*8);
      if((ch&0x80) != 0) { char_data ^= 0x1<<0; }
      if((ch&0x40) != 0) { char_data ^= 0x1<<4; }
      if((ch&0x20) != 0) { char_data ^= 0x1<<8; }
      if((ch&0x10) != 0) { char_data ^= 0x1<<12; }
      if((ch&0x08) != 0) { char_data ^= 0x1<<16; }
      if((ch&0x04) != 0) { char_data ^= 0x1<<20; }
      if((ch&0x02) != 0) { char_data ^= 0x1<<24; }
      if((ch&0x01) != 0) { char_data ^= 0x1<<28; }
      regGP0 = char_data;
      cptr++;
    }
  }
}

static void psxgpu_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
  printf("PSX: Remap char %04X\n", chr);
}

static void psxgpu_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  //struct psx_render_data *render_data = graphics->render_data;
  // TODO!
  printf("PSX: Remap char %04X byte %02X\n", chr, byte);
}

void render_psx_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = psxgpu_init_video;
  renderer->check_video_mode = psxgpu_check_video_mode;
  renderer->set_video_mode = psxgpu_set_video_mode;
  renderer->update_colors = psxgpu_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = psxgpu_remap_charsets;
  renderer->remap_char = psxgpu_remap_char;
  renderer->remap_charbyte = psxgpu_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = psxgpu_render_graph;
  renderer->render_layer = psxgpu_render_layer;
  renderer->render_cursor = psxgpu_render_cursor;
  renderer->render_mouse = psxgpu_render_mouse;
  renderer->sync_screen = psxgpu_sync_screen;
}
