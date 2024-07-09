/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "Unit.hpp"
#include "UnitIO.hpp"

#include "../src/io/path.h"

extern "C" {
#include "../src/render.c"
}
#include "../src/render_layer.cpp"

//#define GENERATE_FILES

#define DIR ".." DIR_SEPARATOR "render" DIR_SEPARATOR

enum smzx_type
{
  MZX = 0,
  SMZX = 1
};

enum flat_bpp
{
  FLAT16,
  FLAT32,
  FLAT_YUY2,
  FLAT_UYVY,
  FLAT_YVYU
};

static const uint8_t palette[][3] =
{
  {  0,  0,  0 },
  {  0,  0, 42 },
  {  0, 42,  0 },
  {  0, 42, 42 },
  { 42,  0,  0 },
  { 42,  0, 42 },
  { 42, 21,  0 },
  { 42, 42, 42 },
  { 21, 21, 21 },
  { 21, 21, 63 },
  { 21, 63, 21 },
  { 21, 63, 63 },
  { 63, 21, 21 },
  { 63, 21, 63 },
  { 63, 63, 21 },
  { 63, 63, 63 }
};

static size_t calc_offset(size_t frame_w, size_t frame_h, size_t full_w, size_t full_h)
{
  return (full_h - frame_h) * full_w / 2 + (full_w - frame_w) / 2;
}

template<typename T, int MISALIGN = 0, int GENERATE = 1>
class render_frame
{
  size_t frame_w;
  size_t frame_h;
  size_t full_width;
  size_t full_height;
  size_t start_offset;
  std::vector<T> buf;

public:
  static constexpr T fill = static_cast<T>(0xbabababababababa);

  render_frame(size_t w, size_t h):
   frame_w(w), frame_h(h), full_width(w + 16), full_height(h + 16),
   buf(full_width * full_height, T(fill))
  {
    start_offset = calc_offset(frame_w, frame_h, full_width, full_height);
  }

  T *pixels()
  {
    return buf.data() + start_offset;
  }

  const T *pixels() const
  {
    return buf.data() + start_offset;
  }

  size_t pitch()
  {
    return sizeof(T) * (full_width - MISALIGN);
  }

  size_t bpp()
  {
    return 8 * sizeof(T);
  }

  void check_out_of_frame() const
  {
    size_t skip = full_width - frame_w;
    size_t i;
    size_t j;
    size_t y;

    // misalign rows if requested by reducing the skip size.
    skip -= MISALIGN;

    for(i = 0; i < start_offset; i++)
      ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);

    for(y = 0; y < frame_h; y++)
    {
      i += frame_w;
      for(j = 0; j < skip; j++, i++)
        ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);
    }
    size_t full_size = buf.size();
    for(; i < full_size; i++)
      ASSERTEQ(buf[i], fill, "letterbox mismatch @ %zu", i);
  }

  void check_in_frame(const graphics_data &graphics, const char *file) const
  {
    size_t frame_w_bytes = frame_w * sizeof(T);
    size_t y;
    const T *buf_pos;

#ifdef GENERATE_FILES
    if(GENERATE)
    {
      std::vector<T> out(frame_w * frame_h);
      T *pix_pos = out.data();
      buf_pos = pixels();
      for(y = 0; y < frame_h; y++)
      {
        memcpy(pix_pos, buf_pos, frame_w_bytes);
        buf_pos += full_width - MISALIGN;
        pix_pos += frame_w;
      }
      unit::io::save_tga(out, frame_w, frame_h,
       graphics.flat_intensity_palette, graphics.screen_mode ? 256 : 32, file);
    }
#endif

    std::vector<T> expected = unit::io::load_tga<T>(file);
    const T *exp_pos = expected.data();
    buf_pos = buf.data() + start_offset;

    for(y = 0; y < frame_h; y++)
    {
      ASSERTMEM(buf_pos, exp_pos, frame_w_bytes,
       "frame mismatch on line %zu: %s", y, file);
      buf_pos += full_width - MISALIGN;
      exp_pos += frame_w;
    }

#ifdef GENERATE_FILES
    FAIL("generation code enabled");
#endif
  }

  void check(const graphics_data &graphics, const char *file) const
  {
    check_in_frame(graphics, file);
    check_out_of_frame();
  }
};

static inline unsigned comp6to5(unsigned c)
{
  return c >> 1;
}

static inline unsigned comp6to8(unsigned c)
{
  return ((c * 255u) + 32u) / 63u;
}

template<flat_bpp FLAT>
static inline uint32_t flat_color(unsigned r, unsigned g, unsigned b);

// TGA 16bpp
template<>
inline uint32_t flat_color<FLAT16>(unsigned r, unsigned g, unsigned b)
{
  r = comp6to5(r);
  g = comp6to5(g);
  b = comp6to5(b);

  uint32_t val = 0x8000 | (r << 10u) | (g << 5u) | b;
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  val = ((val & 0xff) << 8) | (val >> 8);
#endif
  return val;
}

// TGA 32bpp
template<>
inline uint32_t flat_color<FLAT32>(unsigned r, unsigned g, unsigned b)
{
  r = comp6to8(r);
  g = comp6to8(g);
  b = comp6to8(b);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  return (b << 24u) | (g << 16u) | (r << 8u) | 0xffu;
#else
  return 0xff000000u | (r << 16u) | (g << 8u) | b;
#endif
}

template<>
inline uint32_t flat_color<FLAT_YUY2>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_yuy2(comp6to8(r), comp6to8(g), comp6to8(b));
}

template<>
inline uint32_t flat_color<FLAT_UYVY>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_uyvy(comp6to8(r), comp6to8(g), comp6to8(b));
}

template<>
inline uint32_t flat_color<FLAT_YVYU>(unsigned r, unsigned g, unsigned b)
{
  return rgb_to_yvyu(comp6to8(r), comp6to8(g), comp6to8(b));
}

/* Preset enough of the graphics data for these tests to work.
 * Currently this requires:
 *
 *   charset (all renderers)
 *   flat_intensity_palette (16/32-bit renderers)
 *   smzx_indices (SMZX renderers)
 *   protected_pal_position (MZX renderers)
 *
 * Additionally, the render_graph* functions require layer data
 * in text_video and render_layer requires an initialized
 * video_layer struct (it does not need to be in the graphics data).
 *
 * The cursor and mouse render functions don't use the graphics data.
 */
static void init_graphics_base(struct graphics_data &graphics)
{
  static std::vector<uint8_t> default_chr;
  static std::vector<uint8_t> edit_chr;

  if(!default_chr.size())
    default_chr = unit::io::load("../../assets/default.chr");
  if(!edit_chr.size())
    edit_chr = unit::io::load("../../assets/edit.chr");

  memcpy(graphics.charset, default_chr.data(), CHARSET_SIZE * CHAR_H);
  memcpy(graphics.charset + PRO_CH * CHAR_H, edit_chr.data(), CHARSET_SIZE * CHAR_H);
}

template<flat_bpp FLAT>
static void init_graphics_mzx(struct graphics_data &graphics)
{
  graphics.screen_mode = 0;
  graphics.protected_pal_position = 16;
  for(size_t i = 0; i < 16; i++)
  {
    uint32_t flat = flat_color<FLAT>(palette[i][0], palette[i][1], palette[i][2]);
    graphics.flat_intensity_palette[i] = flat;
    graphics.flat_intensity_palette[i + 16] = flat;
    graphics.flat_intensity_palette[i + 256] = flat;
  }
}

template<flat_bpp FLAT>
static void init_graphics_smzx(struct graphics_data &graphics)
{
  init_graphics_mzx<FLAT>(graphics);
  graphics.screen_mode = 3;
  graphics.protected_pal_position = 256;

  for(size_t i = 0; i < 256; i++)
  {
    size_t bg = i >> 4;
    size_t fg = i & 0x0f;
    size_t r = fg << 2;
    size_t b = bg << 2;
    graphics.flat_intensity_palette[i] = flat_color<FLAT>(r, b, b);

    graphics.smzx_indices[i * 4 + 0] = (bg << 4) | bg;
    graphics.smzx_indices[i * 4 + 1] = (bg << 4) | fg;
    graphics.smzx_indices[i * 4 + 2] = (fg << 4) | bg;
    graphics.smzx_indices[i * 4 + 3] = (fg << 4) | fg;
  }
}

#define out(ch,bg,fg) do { \
  pos->char_value = (ch) + offset; \
  pos->bg_color = (bg); \
  pos->fg_color = (fg); \
  pos++; \
} while(0)

static void init_layer_data(struct char_element *dest, int w, int h,
 int offset, int c_offset, int col)
{
  struct char_element *pos;
  int x, y, ch;
  int bg = (col >> 4) + c_offset;
  int fg = (col & 0x0f) + c_offset;

  // Border in given color containing all characters:
  pos = dest;
  ch = 0;
  for(y = 0; y < h; y++)
  {
    if(y < 2 || h - y <= 2)
    {
      for(x = 0; x < w; x++, ch++)
        out(ch & 255, bg, fg);
    }
    else
    {
      for(x = 0; x < 4; x++, ch++)
        out(ch & 255, bg, fg);
      pos += w - 8;
      for(x = w - 4; x < w; x++, ch++)
        out(ch & 255, bg, fg);
    }
  }

  // Centered palette display.
  if(w >= 64 && h >= 22)
  {
    pos = dest;
    pos += w * ((h - 16) / 2);
    pos += (w - 32) / 2;

    for(y = 0; y < 16; y++)
    {
      for(x = 0; x < 16; x++)
      {
        out(176, y, x);
        out(176, y, x);
      }
      pos += w - 32;
    }
  }
}

static void init_layer(struct video_layer &layer, int w, int h,
 int mode, struct char_element *data)
{
  layer.w = w;
  layer.h = h;
  layer.transparent_col = -1;
  layer.mode = mode;
  layer.data = data;
}

template<smzx_type MODE, flat_bpp FLAT>
struct render_graph_init
{
  render_graph_init(struct graphics_data &graphics)
  {
    init_graphics_base(graphics);
    if(MODE == SMZX)
      init_graphics_smzx<FLAT>(graphics);
    else
      init_graphics_mzx<FLAT>(graphics);
  }
};

template<smzx_type MODE, flat_bpp FLAT>
struct render_layer_init : public render_graph_init<MODE, FLAT>
{
  static constexpr int w = 3;
  static constexpr int h = 2;

  struct video_layer layer_screen{};
  struct video_layer layer_small{};
  struct video_layer layer_ui{};
  struct char_element big_data[132 * 43]{};
  struct char_element small_data[w * h]{};
  struct char_element ui_data[w * h]{};

  render_layer_init(struct graphics_data &graphics):
   render_graph_init<MODE, FLAT>(graphics)
  {
    init_layer(layer_screen, SCREEN_W, SCREEN_H,
     graphics.screen_mode, big_data);
    init_layer(layer_small, w, h, graphics.screen_mode, small_data);
    init_layer(layer_ui, w, h, 0, ui_data);

    init_layer_data(small_data, w, h, 0, 0, 0x1f);
    init_layer_data(ui_data, w, h, PRO_CH, 16, 0x2f);
  }
};


/* pix=8-bit align=32-bit */
UNITTEST(render_graph8)
{
  struct graphics_data graphics{};
  render_frame<uint8_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint8_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph8(pixels, pitch, &graphics, set_colors8_mzx);
    frame.check(graphics, DIR "8.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph8(pixels, pitch, &graphics, set_colors8_mzx);
    frame.check(graphics, DIR "8p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph8(pixels, pitch, &graphics, set_colors8_smzx);
    frame.check(graphics, DIR "8s.tga.gz");
  }
}

/* pix=16-bit align=32-bit */
UNITTEST(render_graph16)
{
  struct graphics_data graphics{};
  render_frame<uint16_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint16_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, set_colors16_mzx);
    frame.check(graphics, DIR "16.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph16(pixels, pitch, &graphics, set_colors16_mzx);
    frame.check(graphics, DIR "16p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT16> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, set_colors16_smzx);
    frame.check(graphics, DIR "16s.tga.gz");
  }
}

UNITTEST(render_graph16_yuv)
{
  struct graphics_data graphics{};
  render_frame<uint32_t> frame(SCREEN_PIX_W / 2, SCREEN_PIX_H);

  uint16_t *pixels = reinterpret_cast<uint16_t *>(frame.pixels());
  size_t pitch = frame.pitch();

  SECTION(YUY2)
  {
    render_graph_init<MZX, FLAT_YUY2> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, yuy2_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16yuy2.tga.gz");
  }

  SECTION(UYVY)
  {
    render_graph_init<MZX, FLAT_UYVY> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, uyvy_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16uyvy.tga.gz");
  }

  SECTION(YVYU)
  {
    render_graph_init<MZX, FLAT_YVYU> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph16(pixels, pitch, &graphics, yvyu_subsample_set_colors_mzx);
    frame.check(graphics, DIR "16yvyu.tga.gz");
  }
}

/* pix=32-bit align=32-bit */
UNITTEST(render_graph32)
{
  struct graphics_data graphics{};
  render_frame<uint32_t> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  uint32_t *pixels = frame.pixels();
  size_t pitch = frame.pitch();

  SECTION(MZX)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph32(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32.tga.gz");
  }

  SECTION(MZXProtected)
  {
    render_graph_init<MZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, PRO_CH, 16, 0x2f);

    render_graph32(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32p.tga.gz");
  }

  SECTION(SMZX)
  {
    render_graph_init<SMZX, FLAT32> d(graphics);
    init_layer_data(graphics.text_video, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

    render_graph32s(pixels, pitch, &graphics);
    frame.check(graphics, DIR "32s.tga.gz");
  }
}


// Test that render_cursor overwrites the fill color completely with the
// cursor color at  the specified location and char lines.
template<typename T,
 unsigned width_ch = 4, unsigned height_ch = 3, uint32_t fill = 0xbabababau>
static void test_render_cursor(unsigned x_ch, unsigned y_ch, uint32_t flatcolor,
 uint8_t lines, uint8_t offset)
{
  static constexpr unsigned width_px = width_ch * CHAR_W;
  static constexpr unsigned height_px = height_ch * CHAR_H;
  static constexpr unsigned buf_sz = width_px * height_px * sizeof(T) / sizeof(uint32_t);
  static constexpr unsigned pitch = width_px * sizeof(T);
  static constexpr unsigned bpp = 8 * sizeof(T);

  T fill_px = static_cast<T>(fill);
  T flat_px = static_cast<T>(flatcolor);

  ASSERT(x_ch < width_ch, "bad x");
  ASSERT(y_ch < height_ch, "bad y");

  uint32_t buffer[buf_sz];
  for(size_t i = 0; i < buf_sz; i++)
    buffer[i] = fill;

  render_cursor(buffer, pitch, bpp, x_ch, y_ch, flatcolor, lines, offset);

  T *pos = reinterpret_cast<T *>(buffer);
  for(unsigned y = 0; y < height_px; y++)
  {
    for(unsigned x = 0; x < width_px; x++, pos++)
    {
      if(x >= x_ch * CHAR_W && x < (x_ch + 1) * CHAR_W &&
       y >= y_ch * CHAR_H + offset && y < y_ch * CHAR_H + offset + lines)
      {
        ASSERTEQ(*pos, flat_px, "%u,%u", x, y);
      }
      else
        ASSERTEQ(*pos, fill_px, "%u,%u", x, y);
    }
  }
}

UNITTEST(render_cursor)
{
  SECTION(8bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint8_t>(x, y, 0x0f0f0f0f, 2, 12);
        test_render_cursor<uint8_t>(x, y, 0x0f0f0f0f, 14, 0);
      }
    }
  }

  SECTION(16bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint16_t>(x, y, 0xabcfabcf, 2, 12);
        test_render_cursor<uint16_t>(x, y, 0xabcfabcf, 14, 0);
      }
    }
  }

  SECTION(32bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_cursor<uint32_t>(x, y, 0x12345678, 2, 12);
        test_render_cursor<uint32_t>(x, y, 0x12345678, 14, 0);
      }
    }
  }
}


// Test that render_mouse inverts the data at the given (pixel) rectangle
// according to the invert mask and sets pixels according to the alpha mask.
template<typename T,
 unsigned width_ch = 4, unsigned height_ch = 3>
static void test_render_mouse(unsigned x_px, unsigned y_px,
 uint32_t mask, uint32_t amask, uint8_t width, uint8_t height)
{
  static constexpr unsigned width_px = width_ch * CHAR_W;
  static constexpr unsigned height_px = height_ch * CHAR_H;
  static constexpr unsigned buf_sz = width_px * height_px * sizeof(T) / sizeof(uint32_t);
  static constexpr unsigned pitch = width_px * sizeof(T);
  static constexpr unsigned bpp = 8 * sizeof(T);

  T mask_px = static_cast<T>(mask);
  T amask_px = static_cast<T>(amask);

  ASSERT(x_px < width_px, "bad x");
  ASSERT(y_px < height_px, "bad y");

  uint32_t buffer[buf_sz];
  T *pos = reinterpret_cast<T *>(buffer);
  size_t i;
  for(i = 0; i < width_px * height_px; i++)
    *(pos++) = (i & 0xff) * 0x01010101;

  render_mouse(buffer, pitch, bpp, x_px, y_px, mask, amask, width, height);

  pos = reinterpret_cast<T *>(buffer);
  i = 0;
  for(unsigned y = 0; y < height_px; y++)
  {
    for(unsigned x = 0; x < width_px; x++, pos++, i++)
    {
      T expected = static_cast<T>((i & 0xff) * 0x01010101);
      if(x >= x_px && x < x_px + width && y >= y_px && y < y_px + height)
        expected = amask_px | (expected ^ mask_px);

      ASSERTEQ(*pos, expected, "%u,%u", x, y);
    }
  }
}

UNITTEST(render_mouse)
{
  SECTION(8bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0xffffffff, 0, 8, 14);
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0xffffffff, 0, 8, 7);
        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H + 7, 0xffffffff, 0, 8, 7);

        test_render_mouse<uint8_t>(x * CHAR_W, y * CHAR_H, 0x3f3f3f3f, 0xc0c0c0c0, 8, 14);
      }
    }
  }

  SECTION(16bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H, 0x7fff7fff, 0x80008000, 8, 14);
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H, 0x7fff7fff, 0x80008000, 8, 7);
        test_render_mouse<uint16_t>(x * CHAR_W, y * CHAR_H + 7, 0x7fff7fff, 0x80008000, 8, 7);
      }
    }
  }

  SECTION(32bpp)
  {
    for(size_t y = 0; y < 3; y++)
    {
      for(size_t x = 0; x < 4; x++)
      {
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H, 0x00ffffff, 0xff000000, 8, 14);
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H, 0x00ffffff, 0xff000000, 8, 7);
        test_render_mouse<uint32_t>(x * CHAR_W, y * CHAR_H + 7, 0x00ffffff, 0xff000000, 8, 7);
      }
    }
  }
}


static constexpr int SM_W = 32;
static constexpr int SM_H = 20;
static constexpr int XL_W = 132;
static constexpr int XL_H = 43;

/* Test rendering text_video data.
 * Output should be indistinguishable from render_graph with the same input. */
template<typename T, smzx_type MODE, flat_bpp FLAT>
static void render_layer_graphic(const char *path)
{
  struct graphics_data graphics{};
  render_frame<T, 0, 0> frame(SCREEN_PIX_W, SCREEN_PIX_H);

  T *pixels = frame.pixels();
  size_t pitch = frame.pitch();
  size_t bpp = frame.bpp();

  render_layer_init<MODE, FLAT> d(graphics);
  init_layer_data(d.big_data, SCREEN_W, SCREEN_H, 0, 0, 0x1f);

  render_layer(pixels, SCREEN_PIX_W, SCREEN_PIX_H,
   pitch, bpp, &graphics, &d.layer_screen);

  frame.check(graphics, path);
}

/* Test rendering layers at various alignments with or without clipping.
 * If TR=1, the layers will use transparent colors. */
template<typename T, smzx_type MODE, flat_bpp FLAT,
 int TR = 0, int CLIP = 0, int MISALIGN = 0>
static void render_layer_common(unsigned screen_w, unsigned screen_h,
 unsigned fill_color, const char *path)
{
  struct graphics_data graphics{};
  size_t i;
  int width_px = screen_w * CHAR_W;
  int height_px = screen_h * CHAR_H;

  render_frame<T, MISALIGN, !MISALIGN> frame(width_px, height_px);

  T *pixels = frame.pixels();
  size_t pitch = frame.pitch();
  size_t bpp = frame.bpp();

  render_layer_init<MODE, FLAT> d(graphics);

  // Fill black
  d.layer_screen.w = screen_w;
  d.layer_screen.h = screen_h;
  init_layer_data(d.big_data, screen_w, screen_h, 0, 0, fill_color);
  render_layer(pixels, width_px, height_px,
   pitch, bpp, &graphics, &d.layer_screen);

  for(i = 0; i < 8; i++)
  {
    // no-clip: gradually increase misalignment for each of the 8 renders.
    int x1 = (i * 25) + CHAR_W * 3;
    int x2 = x1;
    int x3 = x1;
    int x4 = x1;
    int y1 = (i * 1) + CHAR_H * 2;
    int y2 = y1 + CHAR_H * 4;
    int y3 = y1 + CHAR_H * 8;
    int y4 = y1 + CHAR_H * 12;

    // clip: same, except relocate each set to one of the four edges.
    if(CLIP)
    {
      // top edge
      y1 = (i * 1) - CHAR_H;
      // left edge
      x2 = (i * 1) - CHAR_W;
      y2 = (i * 29) + CHAR_H;
      // bottom edge
      y3 = (i * 1) + (screen_h - 1) * CHAR_H - 7;
      // right edge
      x4 = (i * 1) + (screen_w - 2) * CHAR_W;
      y4 = y2;
    }

    d.layer_small.x = x1;
    d.layer_small.y = y1;
    d.layer_small.transparent_col = TR ? (MODE ? 0x11 : 1) : -1;
    render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_small);

    d.layer_small.x = x2;
    d.layer_small.y = y2;
    d.layer_small.transparent_col = TR ? (MODE ? 0xff : 15) : -1;
    render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_small);

    d.layer_ui.x = x3;
    d.layer_ui.y = y3;
    d.layer_ui.transparent_col = TR ? graphics.protected_pal_position + 2 : -1;
    render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_ui);

    d.layer_ui.x = x4;
    d.layer_ui.y = y4;
    d.layer_ui.transparent_col = TR ? graphics.protected_pal_position + 15 : -1;
    render_layer(pixels, width_px, height_px,
     pitch, bpp, &graphics, &d.layer_ui);
  }

  frame.check(graphics, path);
}

/* Test rendering layers at various alignments without clipping.
 * If TR=1, the layers will use transparent colors. */
template<typename T, smzx_type MODE, flat_bpp FLAT, int TR = 0>
static void render_layer_align(const char *path)
{
  render_layer_common<T, MODE, FLAT, TR, 0>(SM_W, SM_H, 0, path);
}
/* Same, but with clipping. */
template<typename T, smzx_type MODE, flat_bpp FLAT, int TR = 0>
static void render_layer_clip(const char *path)
{
  render_layer_common<T, MODE, FLAT, TR, 1>(SM_W, SM_H, 0, path);
}

/* Test rendering layers at various alignments without clipping.
 * The buffer will be misaligned by one pixel to test that
 * the layer renderer selects an appropriately misaligned (1PPW) renderer.
 * If TR=1, the layers will use transparent colors. */
template<typename T, smzx_type MODE, flat_bpp FLAT, int TR = 0>
static void render_layer_misalign(const char *path)
{
  render_layer_common<T, MODE, FLAT, TR, 0, 1>(SM_W, SM_H, 0, path);
}
/* Same, but with clipping. */
template<typename T, smzx_type MODE, flat_bpp FLAT, int TR = 0>
static void render_layer_misclip(const char *path)
{
  render_layer_common<T, MODE, FLAT, TR, 1, 1>(SM_W, SM_H, 0, path);
}

/* Test rendering layers to a large destination at various alignments
 * with clipping. If TR=1, the layers will use transparent colors. */
template<typename T, smzx_type MODE, flat_bpp FLAT, int TR = 0>
static void render_layer_large(const char *path)
{
  render_layer_common<T, MODE, FLAT, TR, 1>(XL_W, XL_H, 0x5f, path);
}

UNITTEST(render_layer_mzx8)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  SECTION(graphic)  render_layer_graphic<uint8_t, MZX, FLAT32>(DIR "8.tga.gz");
  SECTION(align)    render_layer_align<uint8_t, MZX, FLAT32>(DIR "8a.tga.gz");
  SECTION(align_tr) render_layer_align<uint8_t, MZX, FLAT32, 1>(DIR "8ta.tga.gz");
  SECTION(clip)     render_layer_clip<uint8_t, MZX, FLAT32>(DIR "8c.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint8_t, MZX, FLAT32, 1>(DIR "8tc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint8_t, MZX, FLAT32>(DIR "8a.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint8_t, MZX, FLAT32>(DIR "8c.tga.gz");
  SECTION(large)    render_layer_large<uint8_t, MZX, FLAT32>(DIR "8xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx8)
{
#ifdef SKIP_8BPP
  SKIP();
#else
  SECTION(graphic)  render_layer_graphic<uint8_t, SMZX, FLAT32>(DIR "8s.tga.gz");
  SECTION(align)    render_layer_align<uint8_t, SMZX, FLAT32>(DIR "8sa.tga.gz");
  SECTION(align_tr) render_layer_align<uint8_t, SMZX, FLAT32, 1>(DIR "8sta.tga.gz");
  SECTION(clip)     render_layer_clip<uint8_t, SMZX, FLAT32>(DIR "8sc.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint8_t, SMZX, FLAT32, 1>(DIR "8stc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint8_t, SMZX, FLAT32>(DIR "8sa.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint8_t, SMZX, FLAT32>(DIR "8sc.tga.gz");
  SECTION(large)    render_layer_large<uint8_t, SMZX, FLAT32>(DIR "8sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx16)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  SECTION(graphic)  render_layer_graphic<uint16_t, MZX, FLAT16>(DIR "16.tga.gz");
  SECTION(align)    render_layer_align<uint16_t, MZX, FLAT16>(DIR "16a.tga.gz");
  SECTION(align_tr) render_layer_align<uint16_t, MZX, FLAT16, 1>(DIR "16ta.tga.gz");
  SECTION(clip)     render_layer_clip<uint16_t, MZX, FLAT16>(DIR "16c.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint16_t, MZX, FLAT16, 1>(DIR "16tc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint16_t, MZX, FLAT16>(DIR "16a.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint16_t, MZX, FLAT16>(DIR "16c.tga.gz");
  SECTION(large)    render_layer_large<uint16_t, MZX, FLAT16>(DIR "16xl.tga.gz");
#endif
}

UNITTEST(render_layer_smzx16)
{
#ifdef SKIP_16BPP
  SKIP();
#else
  SECTION(graphic)  render_layer_graphic<uint16_t, SMZX, FLAT16>(DIR "16s.tga.gz");
  SECTION(align)    render_layer_align<uint16_t, SMZX, FLAT16>(DIR "16sa.tga.gz");
  SECTION(align_tr) render_layer_align<uint16_t, SMZX, FLAT16, 1>(DIR "16sta.tga.gz");
  SECTION(clip)     render_layer_clip<uint16_t, SMZX, FLAT16>(DIR "16sc.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint16_t, SMZX, FLAT16, 1>(DIR "16stc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint16_t, SMZX, FLAT16>(DIR "16sa.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint16_t, SMZX, FLAT16>(DIR "16sc.tga.gz");
  SECTION(large)    render_layer_large<uint16_t, SMZX, FLAT16>(DIR "16sxl.tga.gz");
#endif
}

UNITTEST(render_layer_mzx32)
{
  SECTION(graphic)  render_layer_graphic<uint32_t, MZX, FLAT32>(DIR "32.tga.gz");
  SECTION(align)    render_layer_align<uint32_t, MZX, FLAT32>(DIR "32a.tga.gz");
  SECTION(align_tr) render_layer_align<uint32_t, MZX, FLAT32, 1>(DIR "32ta.tga.gz");
  SECTION(clip)     render_layer_clip<uint32_t, MZX, FLAT32>(DIR "32c.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint32_t, MZX, FLAT32, 1>(DIR "32tc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint32_t, MZX, FLAT32>(DIR "32a.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint32_t, MZX, FLAT32>(DIR "32c.tga.gz");
  SECTION(large)    render_layer_large<uint32_t, MZX, FLAT32>(DIR "32xl.tga.gz");
}

UNITTEST(render_layer_smzx32)
{
  SECTION(graphic)  render_layer_graphic<uint32_t, SMZX, FLAT32>(DIR "32s.tga.gz");
  SECTION(align)    render_layer_align<uint32_t, SMZX, FLAT32>(DIR "32sa.tga.gz");
  SECTION(align_tr) render_layer_align<uint32_t, SMZX, FLAT32, 1>(DIR "32sta.tga.gz");
  SECTION(clip)     render_layer_clip<uint32_t, SMZX, FLAT32>(DIR "32sc.tga.gz");
  SECTION(clip_tr)  render_layer_clip<uint32_t, SMZX, FLAT32, 1>(DIR "32stc.tga.gz");
  SECTION(misalign) render_layer_misalign<uint32_t, SMZX, FLAT32>(DIR "32sa.tga.gz");
  SECTION(misclip)  render_layer_misclip<uint32_t, SMZX, FLAT32>(DIR "32sc.tga.gz");
  SECTION(large)    render_layer_large<uint32_t, SMZX, FLAT32>(DIR "32sxl.tga.gz");
}
