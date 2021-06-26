/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#define get_ticks n64_get_ticks

#include <libdragon.h>

#undef get_ticks

#include "../../src/event.h"
#include "../../src/platform.h"
#include "../../src/graphics.h"
#include "../../src/audio/audio.h"

#ifdef CONFIG_AUDIO

static void n64_audio_callback(short *buffer, size_t numsamples)
{
  audio_callback(buffer, numsamples * 4);
}


void init_audio_platform(struct config_info *conf)
{
  audio_init(audio.output_frequency, 2);
  audio_write_silence();
  audio_write_silence();

  audio.buffer_samples = audio_get_buffer_length();

  int buffer_size = sizeof(Sint16) * 2 * audio.buffer_samples;
  audio.mix_buffer = cmalloc(buffer_size * 2);

  audio_set_buffer_callback(n64_audio_callback);
}

void quit_audio_platform(void)
{
  audio_close();
}

#endif // CONFIG_AUDIO
