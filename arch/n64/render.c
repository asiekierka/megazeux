/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
 * Copyright (C) 2021 Adrian Siekierka <kontakt@asie.pl>
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

#include "../../src/graphics.h"
#include "../../src/render.h"
#include "../../src/render_layer.h"
#include "../../src/renderers.h"

#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

// #define HIDE_RENDERER_FOR_TESTING
#define RENDERER_ACCELERATED

extern void *__safe_buffer[];
#define __get_buffer(x) __safe_buffer[(x) - 1]

struct n64_render_data
{
#ifdef RENDERER_ACCELERATED
  uint32_t fg_texture[64 * 128 / 8];
#endif
  display_context_t ctx;
  uint32_t pitch;
#ifdef RENDERER_ACCELERATED
  bool rdp_attached;
  uint32_t palette32[FULL_PAL_SIZE];
  uint32_t palette32_blend[FULL_PAL_SIZE];
  bool char_empty[256];
  bool fg_texture_changed;
  bool tlut_loaded;
#endif
};

#define DISP_Y_OFFSET ((480 - 350) >> 1)

static void *n64_get_buffer(struct n64_render_data *render_data)
{
  return ((uint8_t*) __get_buffer(render_data->ctx)) + (DISP_Y_OFFSET * (render_data->pitch));
}

#ifdef RENDERER_ACCELERATED
static const uint32_t lut_expand_8_32[256] = {
#include "lut_expand_8_32.inc"
};

static const uint16_t fg_tlut_map[80] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF,
  0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0xFFFF, 0xFFFF,
  0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
};

void __rdp_ringbuffer_queue( uint32_t data );
void __rdp_ringbuffer_send( void );
#endif

static boolean n64_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct n64_render_data *render_data;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  render_data = cmalloc(sizeof(struct n64_render_data));

  graphics->render_data = render_data;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 16;

  render_data->pitch = 640 * (graphics->bits_per_pixel >> 3);

#ifndef HIDE_RENDERER_FOR_TESTING
  display_init(RESOLUTION_640x480, graphics->bits_per_pixel == 32 ? DEPTH_32_BPP : DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_OFF);
#endif

  return set_video_mode();
}

static void n64_free_video(struct graphics_data *graphics)
{
#ifndef HIDE_RENDERER_FOR_TESTING
  display_close();
#endif

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean n64_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  return true;
}

static void n64_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct n64_render_data *render_data = graphics->render_data;
  unsigned int i;

  if(graphics->bits_per_pixel == 16)
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       ((palette[i].b >> 3) << 1) | ((palette[i].g >> 3) << 6) | ((palette[i].r >> 3) << 11) | 0x1;
#ifdef RENDERER_ACCELERATED
      render_data->palette32[i] = graphics->flat_intensity_palette[i] * 0x10001;
      render_data->palette32_blend[i] = 0xFF
        | (((palette[i].b & 0xF8) | 7) << 8)
        | (((palette[i].g & 0xF8) | 7) << 16)
        | (((palette[i].r & 0xF8) | 7) << 24);
#endif
    }
  }
  else
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] = (palette[i].b << 8) | (palette[i].g << 16) | (palette[i].r << 24) | 0xFF;
#ifdef RENDERER_ACCELERATED
      render_data->palette32[i] = graphics->flat_intensity_palette[i];
      render_data->palette32_blend[i] = graphics->flat_intensity_palette[i];
#endif
    }
  }
}

static void n64_start_frame(struct n64_render_data *render_data)
{
  if (!render_data->ctx) while (!(render_data->ctx = display_lock()));
}

#ifdef RENDERER_ACCELERATED
static void n64_start_rdp(struct n64_render_data *render_data)
{
  if (!render_data->rdp_attached)
  {
    rdp_sync(SYNC_PIPE);
    rdp_set_default_clipping();
    rdp_attach_display(render_data->ctx);
    render_data->rdp_attached = true;
  }
}

static inline void n64_rdp_set_combine_mode_2(int a_clr, int b_clr, int c_clr, int d_clr, int a_alpha, int b_alpha, int c_alpha, int d_alpha, int a_clr2, int b_clr2, int c_clr2, int d_clr2, int a_alpha2, int b_alpha2, int c_alpha2, int d_alpha2) {
  __rdp_ringbuffer_queue( 0xFC000000 | (a_clr << 20) | (c_clr << 15) | (a_alpha << 12) | (c_alpha << 9) | (a_clr2 << 5) | c_clr2);
  __rdp_ringbuffer_queue( (b_clr << 28) | (b_clr2 << 24) | (a_alpha2 << 21) | (c_alpha2 << 18) | (d_clr << 15) | (b_alpha << 12) | (d_alpha << 9) | (d_clr2 << 6) | (b_alpha2 << 3) | d_alpha2);
  __rdp_ringbuffer_send();
}

static inline void n64_rdp_set_combine_mode_1(int a_clr, int b_clr, int c_clr, int d_clr, int a_alpha, int b_alpha, int c_alpha, int d_alpha) {
  n64_rdp_set_combine_mode_2(
    a_clr, b_clr, c_clr, d_clr, a_alpha, b_alpha, c_alpha, d_alpha,
    a_clr, b_clr, c_clr, d_clr, a_alpha, b_alpha, c_alpha, d_alpha
  );
}

static inline void n64_draw_fg_accel(struct graphics_data *graphics, struct n64_render_data *render_data, uint8_t offset)
{
  rdp_sync(SYNC_TILE);

  if(!render_data->tlut_loaded)
  {
    for (int i = 0; i < 4; i++) {
      __rdp_ringbuffer_queue( 0xFD100000 );
      __rdp_ringbuffer_queue( (uint32_t) fg_tlut_map );
      __rdp_ringbuffer_send();

      __rdp_ringbuffer_queue( 0xF5000000 | 0x100 );
      __rdp_ringbuffer_queue( 0x00000000 );
      __rdp_ringbuffer_send();
    }

    __rdp_ringbuffer_queue( 0xF0000000 );
    __rdp_ringbuffer_queue( 79 << 14 );
    __rdp_ringbuffer_send();

    render_data->tlut_loaded = true;
  }

  __rdp_ringbuffer_queue( 0xFD100000 | 15 );
  __rdp_ringbuffer_queue( (uint32_t) render_data->fg_texture + (offset << 6));
  __rdp_ringbuffer_send();

  __rdp_ringbuffer_queue(0xF5100000 | (4 << 9) | 0);
  __rdp_ringbuffer_queue(0x00000000);
  __rdp_ringbuffer_send();

  __rdp_ringbuffer_queue( 0xF4000000 );
  __rdp_ringbuffer_queue( (63 << 14) | (61 << 2) );
  __rdp_ringbuffer_send();

  // Configure tile descriptors
  for(int i = 0; i < 4; i++)
  {
    __rdp_ringbuffer_queue(0xF5400000 | (4 << 9) | 0);
    __rdp_ringbuffer_queue( ((i + 1) << 20) | (i << 24) );
    __rdp_ringbuffer_send();

    if(i > 0)
    {
      __rdp_ringbuffer_queue( 0xF2000000 );
      __rdp_ringbuffer_queue( (63 << 14) | (63 << 2) | (i << 24) );
      __rdp_ringbuffer_send();
    }
  }

  // Draw foreground textures
  {
    struct char_element *text_cell = graphics->text_video;
    uint8_t curr_fg_color = 16;

    for (uint32_t y = 0; y < 25; y++) {
      int ty = y * 14 + DISP_Y_OFFSET;
      int by = ty + 14;

      for (uint32_t x = 0; x < 80; x++, text_cell++) {
        uint8_t chr = (*text_cell).char_value;
        if ((chr & 0x20) != offset) continue;
        uint8_t bg_color = (*text_cell).bg_color;
        uint8_t fg_color = (*text_cell).fg_color;
        if (bg_color == fg_color) continue;
        if (render_data->char_empty[chr]) continue;

        uint8_t pal_offset = (chr >> 6);

        if (fg_color != curr_fg_color)
        {
          uint32_t new_color = render_data->palette32_blend[fg_color];
          rdp_set_blend_color(new_color);
          curr_fg_color = fg_color;
        }

        int s = (chr & 0x07) << 3;
        int t = (chr & 0x18) << 1;

        int tx = x * 8;
        int bx = tx + 8;

        __rdp_ringbuffer_queue( 0xE4000000 | (bx << 14) | (by << 2) );
        __rdp_ringbuffer_queue( (tx << 14) | (ty << 2) | (pal_offset << 24) );

        __rdp_ringbuffer_queue( (s << 21) | (t << 5) );
        __rdp_ringbuffer_queue( (1024 << 16) | 1024 );

        __rdp_ringbuffer_send();
      }
    }
  }
}
#endif

static void n64_render_graph(struct graphics_data *graphics)
{
  struct n64_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  int mode = graphics->screen_mode;
  uint32_t *pixels;

  n64_start_frame(render_data);

#ifndef HIDE_RENDERER_FOR_TESTING
   pixels = n64_get_buffer(render_data);

#ifdef RENDERER_ACCELERATED
  if(mode)
  {
    if(graphics->bits_per_pixel == 16)
      render_graph16((uint16_t *)pixels, pitch, graphics, set_colors16[mode]);
    else
      render_graph32s((uint32_t *)pixels, pitch, graphics);
    return;
  }

  // Accelerated codepath for mode 0
  n64_start_rdp(render_data);
  __rdp_ringbuffer_queue( 0xEF3000FF );
  __rdp_ringbuffer_queue( 0x00004000 );
  __rdp_ringbuffer_send();

  // Queue the filled BG rectangles to draw
  {
    struct char_element *text_cell = graphics->text_video;
    uint8_t curr_bg_color = 255;
    for (uint32_t y = 0; y < 25; y++) {
      uint32_t prev_x = 0;
      int ty = y * 14 + DISP_Y_OFFSET;
      int by = ty + 14;
      for (uint32_t x = 0; x < 80; x++, text_cell++) {
        uint8_t bg_color = (*text_cell).bg_color;
        if (curr_bg_color != bg_color)
        {
          if (prev_x != x)
          {
            int tx = prev_x * 8;
            int bx = x * 8;
            __rdp_ringbuffer_queue( 0xF6000000 | ( bx << 14 ) | ( by << 2 ) );
            __rdp_ringbuffer_queue( ( tx << 14 ) | ( ty << 2 ) );
            __rdp_ringbuffer_send();
            prev_x = x;
          }
          uint32_t new_color = render_data->palette32[(*text_cell).bg_color];
          rdp_set_primitive_color(new_color);
          curr_bg_color = bg_color;
        }
      }
      int tx = prev_x * 8;
      int bx = 80 * 8;
      __rdp_ringbuffer_queue( 0xF6000000 | ( bx << 14 ) | ( by << 2 ) );
      __rdp_ringbuffer_queue( ( tx << 14 ) | ( ty << 2 ) );
      __rdp_ringbuffer_send();
    }
  }

  // Set up the foreground texture

  if(render_data->fg_texture_changed)
  {
    data_cache_hit_writeback_invalidate(render_data->fg_texture, sizeof(render_data->fg_texture));
    render_data->fg_texture_changed = false;
  }

  __rdp_ringbuffer_queue( 0xEF0008F0 | 0xC000 );
  __rdp_ringbuffer_queue( 0xA0400000 | 0x4000 | 0x40 );
  __rdp_ringbuffer_send();

  n64_rdp_set_combine_mode_1(6, 0xF, 0x1F, 6, 1, 7, 0, 7);

  n64_draw_fg_accel(graphics, render_data, 0x00);

  n64_draw_fg_accel(graphics, render_data, 0x20);
#else
  if(graphics->bits_per_pixel == 16)
    render_graph16((uint16_t *)pixels, pitch, graphics, set_colors16[mode]);
  else
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);
  }
#endif
#endif
}

static void n64_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct n64_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  uint32_t *pixels;

#ifndef HIDE_RENDERER_FOR_TESTING
  n64_start_frame(render_data);

  pixels = n64_get_buffer(render_data);

  render_layer(pixels, graphics->bits_per_pixel, pitch, graphics, layer);
#endif
}

static void n64_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct n64_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  unsigned int bpp = graphics->bits_per_pixel;
  uint32_t *pixels;

  uint32_t flatcolor = graphics->flat_intensity_palette[color] * (bpp == 32 ? 1 : 0x10001);

#ifndef HIDE_RENDERER_FOR_TESTING
  n64_start_frame(render_data);

  pixels = n64_get_buffer(render_data);

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
#endif
}

static void n64_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct n64_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  unsigned int bpp = graphics->bits_per_pixel;
  uint32_t *pixels;

  uint32_t mask = 0xFFFFFFFF;
  uint32_t alpha_mask = 0x0;

#ifndef HIDE_RENDERER_FOR_TESTING
  n64_start_frame(render_data);

  pixels = n64_get_buffer(render_data);

  render_mouse(pixels, pitch, bpp, x, y, mask, alpha_mask, w, h);
#endif
}

static void n64_sync_screen(struct graphics_data *graphics)
{
  struct n64_render_data *render_data = graphics->render_data;

#ifndef HIDE_RENDERER_FOR_TESTING
#ifdef RENDERER_ACCELERATED
  if (render_data->rdp_attached)
  {
    rdp_detach_display();
    render_data->rdp_attached = false;
  }
#endif

  if (render_data->ctx)
  {
    display_show(render_data->ctx);
    render_data->ctx = 0;
  }
#endif
}

#ifdef RENDERER_ACCELERATED
static void n64_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  if (chr > 0xFF) return;
  chr &= 0x3F;

  int i;
  struct n64_render_data *render_data = graphics->render_data;
  uint8_t *c = graphics->charset + (chr * CHAR_SIZE);
  uint32_t *fg_tex = render_data->fg_texture + ((chr >> 3) * 128) + (chr & 0x07);

  bool ce[4] = {true, true, true, true};
  for (i = 0; i < 14; i++, c++, fg_tex += 8)
  {
    ce[0] &= (c[0] == 0);
    ce[1] &= (c[0x40 * CHAR_SIZE] == 0);
    ce[2] &= (c[0x80 * CHAR_SIZE] == 0);
    ce[3] &= (c[0xC0 * CHAR_SIZE] == 0);
    *fg_tex = (lut_expand_8_32[c[0]])
      | (lut_expand_8_32[c[0x40 * CHAR_SIZE]] << 1)
      | (lut_expand_8_32[c[0x80 * CHAR_SIZE]] << 2)
      | (lut_expand_8_32[c[0xC0 * CHAR_SIZE]] << 3);
  }
  render_data->char_empty[chr] = ce[0];
  render_data->char_empty[chr + 0x40] = ce[1];
  render_data->char_empty[chr + 0x80] = ce[2];
  render_data->char_empty[chr + 0xC0] = ce[3];
  render_data->fg_texture_changed = true;
}

static void n64_remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  n64_remap_char(graphics, chr);
}


static void n64_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  if (count > 64) count = 64; // handle overlap

  for(uint16_t i = 0; i < count; i++)
    n64_remap_char(graphics, first + i);
}
#endif

void render_n64_register(struct renderer *renderer) {
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = n64_init_video;
  renderer->free_video = n64_free_video;
  renderer->set_video_mode = n64_set_video_mode;
  renderer->update_colors = n64_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = n64_render_graph;
  renderer->render_layer = n64_render_layer;
  renderer->render_cursor = n64_render_cursor;
  renderer->render_mouse = n64_render_mouse;
  renderer->sync_screen = n64_sync_screen;
#ifdef RENDERER_ACCELERATED
  renderer->remap_charbyte = n64_remap_charbyte;
  renderer->remap_char = n64_remap_char;
  renderer->remap_char_range = n64_remap_char_range;
#endif
}
