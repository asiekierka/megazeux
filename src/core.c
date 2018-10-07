/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

/* MegaZeux main loop and context handling code. */

#include <stdlib.h>
#include <string.h>

#include "caption.h"
#include "counter.h"
#include "configure.h"
#include "core.h"
#include "error.h"
#include "event.h"
#include "graphics.h"
#include "helpsys.h"
#include "settings.h"
#include "world.h"
#include "world_struct.h"

struct context_stack
{
  context **contents;
  int alloc;
  int size;
  int pos;
};

/**
 * Used as a context to help create the first actual context.
 * Stores the context stack.
 */

struct core_context
{
  context ctx;
  boolean full_exit;
  boolean context_changed;
  struct context_stack stack;
};

/**
 * Contains internal context data used to run contexts.
 */

struct context_data
{
  context *parent;
  boolean is_subcontext;
  enum context_type context_type;
  enum framerate_type framerate;
  struct context_stack stack;
  struct context_spec functions;
};

#ifdef CONFIG_FPS
#define FPS_HISTORY_SIZE 5
#define FPS_INTERVAL 1000

static double average_fps;

/**
 * Track the current speed MegaZeux is running in frames.
 */

static void update_fps(int current_ticks)
{
  static int fps_previous_ticks = -1;
  static int fps_history[FPS_HISTORY_SIZE];
  static int fps_history_count;
  static int frames_counted;
  int total_fps;
  int min_fps;
  int max_fps;
  int i;

  int delta_ticks = current_ticks - fps_previous_ticks;

  if(fps_previous_ticks == -1)
  {
    fps_previous_ticks = current_ticks;
    frames_counted = 0;
    for(i = 0; i < FPS_HISTORY_SIZE; i++)
      fps_history[i] = -1;
  }
  else

  if(delta_ticks >= FPS_INTERVAL)
  {
    for(i = FPS_HISTORY_SIZE - 1; i >= 1; i--)
    {
      fps_history[i] = fps_history[i - 1];
    }

    fps_history[0] = frames_counted;
    min_fps = fps_history[0];
    max_fps = fps_history[0];
    total_fps = 0;
    fps_history_count = 0;

    for(i = 0; i < FPS_HISTORY_SIZE; i++)
    {
      if(fps_history[i] > -1)
      {
        if(fps_history[i] > max_fps)
          max_fps = fps_history[i];

        if(fps_history[i] < min_fps)
          min_fps = fps_history[i];

        total_fps += fps_history[i];
        fps_history_count++;
      }
    }
    // Subtract off highest and lowest scores (outliers)
    total_fps -= max_fps;
    total_fps -= min_fps;
    if(fps_history_count > 2)
    {
      average_fps =
        1.0 * total_fps / (fps_history_count - 2) * (1000.0 / FPS_INTERVAL);

      caption_set_fps(average_fps);
    }
    fps_previous_ticks += FPS_INTERVAL;

    frames_counted = 0;
  }

  else
  {
    frames_counted++;
  }
}
#endif

#define CTX_NAME_MAX_SIZE 16

/**
 * Get the name of a context from its ID (see core.h)
 */
static const char *get_ctx_name(enum context_type id)
{
  switch(id)
  {
    // Core contexts.
    case CTX_TITLE_SCREEN:      return "Title screen";
    case CTX_MAIN:              return "(help main page)";
    case CTX_PLAY_GAME:         return "Gameplay";
    case CTX_CONFIGURE:         return "Settings ed.";
    case CTX_DIALOG_BOX:        return "Dialog";
    case CTX_HELP_SYSTEM:       return "Help system";
    case CTX_MAIN_MENU:         return "Main menu";
    case CTX_GAME_MENU:         return "Game menu";
    case CTX_INTAKE:            return "(intake string)";
    case CTX_INTAKE_NUM:        return "(intake number)";

    // Network contexts.
    case CTX_UPDATER:           return "Updater";

    // Editor contexts.
    case CTX_EDITOR:            return "Editor";
    case CTX_EDITOR_VIEW_BOARD: return "(view board)";
    case CTX_BLOCK_CMD:         return "Block command";
    case CTX_BLOCK_TYPE:        return "Block type";
    case CTX_CHOOSE_CHARSET:    return "Select charset";
    case CTX_IMPORTEXPORT_TYPE: return "Import/export";
    case CTX_CHAR_EDIT:         return "Char editor";
    case CTX_STATUS_COUNTERS:   return "Status counters";
    case CTX_BOARD_EXITS:       return "Board exits";
    case CTX_BOARD_SIZES:       return "Board sizes";
    case CTX_BOARD_INFO:        return "Board info";
    case CTX_CHANGE_CHAR_IDS:   return "Char ID table";
    case CTX_CHANGE_DAMAGE:     return "Damage table";
    case CTX_GLOBAL_SETTINGS:   return "Global settings1";
    case CTX_GLOBAL_SETTINGS_2: return "Global settings2";
    case CTX_ROBO_ED:           return "Robot editor";
    case CTX_PALETTE_EDITOR:    return "Palette editor";
    case CTX_SENSOR_EDITOR:     return "Sensor param";
    case CTX_SUPER_MEGAZEUX:    return "Select SMZX mode";
    case CTX_SFX_EDITOR:        return "SFX editor";
    case CTX_COUNTER_DEBUG:     return "Counter debugger";
    case CTX_ROBOT_DEBUG:       return "Robot debugger";
    case CTX_BREAKPOINT_EDITOR: return "Robot dbg. conf.";
    case CTX_VLAYER_SIZES:      return "Vlayer sizes";
  }
  return "?????";
}

/**
 * Debug: print a line of the context stack.
 */
static void print_ctx_line(context_data *ctx_data)
{
  char name[CTX_NAME_MAX_SIZE + 1] = "  -> subcontext";
  const char *framerate_str = "-";
  boolean click_drag_same = false;

  if(!ctx_data->is_subcontext)
  {
    snprintf(name, CTX_NAME_MAX_SIZE, "%s",
     get_ctx_name(ctx_data->context_type));

    switch(ctx_data->framerate)
    {
      case FRAMERATE_UI:            framerate_str = "UI "; break;
      case FRAMERATE_UI_INTERRUPT:  framerate_str = "Int"; break;
      case FRAMERATE_MZX_SPEED:     framerate_str = "MZX"; break;
      default:                      framerate_str = "???"; break;
    }
  }

  if(ctx_data->functions.click == ctx_data->functions.drag)
    click_drag_same = true;

  fprintf(stderr, "%-*.*s | %3s %3s %3s %3s %3s %3s %3s | %-3s \n",
    16, 16, name,
    ctx_data->functions.resume  ? "Yes" : "",
    ctx_data->functions.draw    ? "Yes" : "",
    ctx_data->functions.idle    ? "Yes" : "",
    ctx_data->functions.key     ? "Yes" : "",
    ctx_data->functions.click   ? "Yes" : "",
    ctx_data->functions.drag    ? click_drag_same ? "<- " : "Yes" : "",
    ctx_data->functions.destroy ? "Yes" : "",
    framerate_str
  );
}

/**
 * Debug: print the entire context stack to the terminal.
 */

#define print_core_stack(ctx) __print_core_stack(ctx, __FILE__, __LINE__)

static void __print_core_stack(context *_ctx, const char *file, int line)
{
  core_context *root;
  context_data *ctx_data;
  context_data *sub_data;
  int i;
  int i2;

  if(file)
    fprintf(stderr, "%s line %d:\n", file, line);

  if(!_ctx)
  {
    fprintf(stderr, "Context is NULL!\n");
    fflush(stderr);
    return;
  }
  else

  if(!_ctx->root)
  {
    fprintf(stderr, "Context root is NULL!\n");
    fflush(stderr);
    return;
  }

  root = _ctx->root;

  fprintf(stderr, "CONTEXT STACK    | Res Drw Idl Key Clk Drg Dst | Fr. \n");
  fprintf(stderr, "-----------------|-----------------------------|-----\n");

  for(i = root->stack.size - 1; i >= 0; i--)
  {
    ctx_data = root->stack.contents[i]->internal_data;

    for(i2 = ctx_data->stack.size - 1; i2 >= 0; i2--)
    {
      sub_data = ctx_data->stack.contents[i2]->internal_data;
      print_ctx_line(sub_data);
    }

    print_ctx_line(ctx_data);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}

/**
 * Add an element to a stack
 */
static void add_stack(struct context_stack *stack, context *add)
{
  if(stack->size >= stack->alloc)
  {
    if(stack->alloc == 0)
      stack->alloc = 8;

    while(stack->size >= stack->alloc)
      stack->alloc *= 2;

    stack->contents = crealloc(stack->contents, stack->alloc * sizeof(void *));
  }

  stack->contents[stack->size] = add;
  stack->size++;
}

/**
 * Remove an element from a stack.
 */
static int remove_stack(struct context_stack *stack, context *del)
{
  int i;

  for(i = stack->size - 1; i >= 0; i--)
  {
    if(stack->contents[i] == del)
    {
      if(i < stack->size - 1)
        memmove(stack->contents + i, stack->contents + i + 1,
         stack->size - i - 1);

      stack->size--;
      return i;
    }
  }

  error_message(E_CORE_FATAL_BUG, 6, NULL);
  return -1;
}

/**
 * Configures a provided context and adds it to the context stack to be run
 * by the main loop. The assumption here is that ctx will be some other local
 * struct casted to a context.
 *
 * At least one update function needs to be provided capable of destroying the
 * context; otherwise, MegaZeux would hang up trying to execute this context.
 */

void create_context(context *ctx, context *parent,
 struct context_spec *ctx_spec, enum context_type context_type)
{
  core_context *root;
  context_data *ctx_data;

  if(parent == NULL || !ctx_spec ||
   (ctx_spec->key == NULL && ctx_spec->click == NULL &&
    ctx_spec->drag == NULL && ctx_spec->idle == NULL))
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 1, NULL);
    return;
  }

  // If the parent is a subcontext, try to find the real parent context.
  while(parent->internal_data && parent->internal_data->parent &&
   parent->internal_data->is_subcontext)
    parent = parent->internal_data->parent;

  // Root needs to exist so this context can be added to the stack.
  if(!parent->root)
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 7, NULL);
    return;
  }

  if(!ctx) ctx = cmalloc(sizeof(struct context));
  ctx_data = cmalloc(sizeof(struct context_data));

  ctx->root = parent->root;
  ctx->internal_data = ctx_data;
  ctx->world = parent->world;
  ctx_data->parent = NULL;
  ctx_data->context_type = context_type;
  ctx_data->framerate = ctx_spec->framerate_mode;
  ctx_data->is_subcontext = false;
  memset(&(ctx_data->stack), 0, sizeof(struct context_stack));
  memcpy(&(ctx_data->functions), ctx_spec, sizeof(struct context_spec));

  // Add the new context to the stack.
  root = parent->root;
  add_stack(&(root->stack), ctx);

  root->context_changed = true;
}

/**
 * Creates a subcontext and adds it to the parent context.
 */

CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 struct context_spec *sub_spec)
{
  core_context *root;
  context_data *parent_data;
  context_data *sub_data;

  // If the parent is a subcontext, try to find the real parent context.
  while(parent && parent->internal_data && parent->internal_data->is_subcontext)
    parent = parent->internal_data->parent;

  // Root context must exit, parent must not be the root context, and make sure
  // this isn't some weird glitched context.
  if(!parent || !parent->root || parent == (context *)(parent->root) ||
   !parent->internal_data || parent->internal_data->parent || !sub_spec)
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 8, NULL);
    return;
  }

  root = parent->root;
  parent_data = parent->internal_data;

  if(!sub) sub = cmalloc(sizeof(struct context));
  sub_data = cmalloc(sizeof(struct context_data));

  sub->root = root;
  sub->internal_data = sub_data;
  sub->world = parent->world;
  sub_data->parent = parent;
  sub_data->is_subcontext = true;
  memcpy(&(sub_data->functions), sub_spec, sizeof(struct context_spec));

  // Add the subcontext to the parent's stack.
  add_stack(&(parent_data->stack), sub);
}

/**
 * Destroy the target context or subcontext from its parent stack.
 * Flag the core_context to abort further execution of the cycle if the root
 * context stack is changed.
 */

void destroy_context(context *ctx)
{
  core_context *root = ctx->root;
  context_data *ctx_data = ctx->internal_data;

  if(!ctx_data)
    return;

  if(!ctx_data->parent || !ctx_data->is_subcontext)
  {
    // This is a root-level context, so remove it from the context stack.

    // If the context isn't on the stack, this will error.
    remove_stack(&(root->stack), ctx);
    root->context_changed = true;

    // Also, destroy all children.
    if(ctx_data->stack.size)
    {
      int i;
      for(i = ctx_data->stack.size - 1; i >= 0; i--)
        destroy_context(ctx_data->stack.contents[i]);
    }

    free(ctx_data->stack.contents);
  }
  else
  {
    int removed;
    // This is a subcontext, so remove it from its parent context.
    context_data *parent_data = ctx_data->parent->internal_data;

    // If the subcontext isn't on the stack, this will error.
    removed = remove_stack(&(parent_data->stack), ctx);

    // Adjust the current position in case this is mid-iteration.
    if(removed <= parent_data->stack.pos)
      parent_data->stack.pos--;
  }

  if(ctx_data->functions.destroy)
    ctx_data->functions.destroy(ctx);

  free(ctx_data);
  free(ctx);
}

/**
 * Find out if a context change has occurred.
 */

boolean has_context_changed(context *ctx)
{
  if(!ctx || !ctx->root)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 9, NULL);
    return true;
  }

  return (ctx->root->context_changed);
}

/**
 * Find out if the given context is a particular context.
 */

boolean is_context(context *ctx, enum context_type context_type)
{
  if(!ctx || !ctx->internal_data)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 2, NULL);
    return false;
  }

  if(ctx->internal_data->is_subcontext)
    return false;

  return (ctx->internal_data->context_type == context_type);
}

/**
 * Set the framerate mode for the current context.
 */

void set_context_framerate_mode(context *ctx, enum framerate_type framerate)
{
  if(!ctx || !ctx->internal_data || ctx->internal_data->is_subcontext)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 3, NULL);
    return;
  }

  ctx->internal_data->framerate = framerate;
}

/**
 * Generate the core_context struct. This struct imitates a regular context so
 * it can be used as a parent, but it should not be started like a context.
 */

core_context *core_init(struct world *mzx_world)
{
  core_context *root = cmalloc(sizeof(core_context));
  context *ctx = (context *)root;

  ctx->root = root;
  ctx->world = mzx_world;
  ctx->internal_data = NULL;

  memset(&(root->stack), 0, sizeof(struct context_stack));
  root->full_exit = false;
  root->context_changed = false;

  return root;
}

/**
 * Execute the resume function for the current context after a context change.
 */

static void core_resume(core_context *root)
{
  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;

  if(ctx_data->functions.resume)
    ctx_data->functions.resume(ctx);

  if(root->context_changed || root->full_exit)
    return;

  ctx_data->stack.pos = 0;

  while(ctx_data->stack.pos < ctx_data->stack.size)
  {
    sub = ctx_data->stack.contents[ctx_data->stack.pos];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.resume)
      sub_data->functions.resume((context *)sub);

    if(root->context_changed || root->full_exit)
      return;

    ctx_data->stack.pos++;
  }
}

/**
 * Draw the current context.
 */

static void core_draw(core_context *root)
{
  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;

  if(ctx_data->functions.draw)
    ctx_data->functions.draw(ctx);

  if(root->context_changed || root->full_exit)
    return;

  ctx_data->stack.pos = 0;

  while(ctx_data->stack.pos < ctx_data->stack.size)
  {
    sub = ctx_data->stack.contents[ctx_data->stack.pos];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.draw)
    {
      select_layer(UI_LAYER);
      sub_data->functions.draw((context *)sub);
    }

    if(root->context_changed || root->full_exit)
      return;

    ctx_data->stack.pos++;
  }

#ifdef CONFIG_FPS
  if(is_fullscreen() && ctx_data->context_type != CTX_EDITOR)
  {
    // If we're in fullscreen mode, draw an onscreen FPS display.
    char fpsbuf[32];
    snprintf(fpsbuf, 32, "  %.2f  ", average_fps);

    select_layer(UI_LAYER);
    write_string(fpsbuf, 0, 0, 0x0f, false);
  }
#endif
}

/**
 * Determine if a given context exists on the stack.
 */
static boolean is_on_stack(core_context *root, enum context_type type)
{
  int i = root->stack.size - 1;
  context *ctx;

  for(; i >= 0; i--)
  {
    ctx = root->stack.contents[i];
    if(ctx->internal_data->context_type == type)
      return true;
  }

  return false;
}

#ifdef CONFIG_HELPSYS
/**
 * Determine if the help system is currently allowed.
 */
static boolean allow_help_system(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  struct config_info *conf = get_config();

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  if(mzx_world->active && mzx_world->version >= V260)
  {
    if(is_on_stack(root, CTX_PLAY_GAME) ||
     (is_on_stack(root, CTX_TITLE_SCREEN) && conf->standalone_mode))
    {
      if(!get_counter(mzx_world, "HELP_MENU", 0))
        return false;
    }
  }

  return true;
}
#endif

/**
 * Determine if the configure menu is currently allowed.
 */
static boolean allow_configure(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  struct config_info *conf = get_config();

  if(is_on_stack(root, CTX_CONFIGURE))
    return false;

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  // Bypass F2_MENU counter.
  if(get_ctrl_status(keycode_internal) && !conf->standalone_mode)
    return true;

  if(mzx_world->active && mzx_world->version >= V260)
  {
    if(is_on_stack(root, CTX_PLAY_GAME) ||
     (is_on_stack(root, CTX_TITLE_SCREEN) && conf->standalone_mode))
    {
      if(!get_counter(mzx_world, "F2_MENU", 0))
        return false;
    }
  }

  return true;
}

/**
 * Update the current context, handling input and idle activity.
 * If any subcontexts are attached to the context, they will be handled from
 * most recent to oldest, and the context will be handled last. Notes:
 *
 * 1) Execution order: Idle -> Click OR Drag -> Key
 * 2) A true return value of idle cancels both key and mouse handling.
 * 3) A true return value of key cancels key handling.
 * 4) A true return value of click or drag cancels mouse handling.
 */

static void core_update(core_context *root)
{
  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *cur_data;
  context *cur;

  boolean mouse_handled = false;
  boolean key_handled = false;

  boolean exit_status = get_exit_status();
  int key = get_key(keycode_internal_wrt_numlock);
  int mouse_press = get_mouse_press_ext();
  int mouse_drag_state = get_mouse_drag();
  int mouse_x;
  int mouse_y;

  get_mouse_position(&mouse_x, &mouse_y);

  ctx_data->stack.pos = ctx_data->stack.size - 1;

  do
  {
    // Count down to -1 and break when cur equals ctx.
    // Slightly convoluted, but better than duplicating this loop's logic.
    if(ctx_data->stack.pos < 0)
    {
      cur = ctx;
      cur_data = ctx_data;
    }
    else
    {
      cur = (context *)ctx_data->stack.contents[ctx_data->stack.pos];
      cur_data = cur->internal_data;
    }

    if(cur_data->functions.idle)
    {
      if(cur_data->functions.idle(cur))
      {
        mouse_handled = true;
        key_handled = true;
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!mouse_handled)
    {
      if(mouse_drag_state && cur_data->functions.drag)
      {
        mouse_handled |=
         cur_data->functions.drag(cur, &key, mouse_press, mouse_x, mouse_y);
      }
      else

      if(mouse_press && cur_data->functions.click)
      {
        mouse_handled |=
         cur_data->functions.click(cur, &key, mouse_press, mouse_x, mouse_y);
      }

      if(root->context_changed || root->full_exit)
        return;
    }

    if(!key_handled)
    {
      if((key || exit_status) && cur_data->functions.key)
        key_handled |= cur_data->functions.key(cur, &key);

      if(root->context_changed || root->full_exit)
        return;
    }

    // Move to the next subcontext if we haven't already been advanced.
    if(ctx_data->stack.pos >= 0)
      if(cur == ctx_data->stack.contents[ctx_data->stack.pos])
        ctx_data->stack.pos--;
  }
  while(cur != ctx);

  // Global key handler.
  if(!key_handled)
  {
    switch(key)
    {
#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        // Display help.
        if(allow_help_system(root))
          help_system(ctx, ctx->world);

        break;
      }
#endif

      case IKEY_F2:
      {
        // Display settings menu.
        if(allow_configure(root))
          game_settings(ctx->world);

        break;
      }

      case IKEY_F12:
      {
#ifdef DEBUG
        if(get_alt_status(keycode_internal_wrt_numlock))
        {
          print_core_stack(ctx);
          break;
        }
#endif

#ifdef CONFIG_ENABLE_SCREENSHOTS
        // Take screenshot.
        if(get_config()->allow_screenshots)
          dump_screen();
#endif
        break;
      }
    }
  }
}

/**
 * Run the main game loop.
 */

void core_run(core_context *root)
{
  struct config_info *conf = get_config();
  context *ctx;
  // FIXME: Because not everything can be converted to use the main loop at
  // once, this function needs to be used from places other than main(). So
  // this doesn't break MZX, this function stops once the number of contexts
  // on the stack has dropped below the initial value.
  int initial_stack_size = root->stack.size;
  int start_ticks = get_ticks();
  int delta_ticks;
  int total_ticks;

  // If there aren't any contexts on the stack, there's no reason to be here.
  if(initial_stack_size <= 0)
    return;

  // FIXME hack
  enable_f12_hack = conf->allow_screenshots;

  do
  {
    // Resume might trigger additional context changes.
    while(root->context_changed)
    {
      root->context_changed = false;
      force_release_all_keys();
      core_resume(root);

      // Resume might also trigger an exit.
      if(root->full_exit)
        return;
    }

    core_draw(root);

    // Context changed or an exit occurred? Skip the screen update and delay
    if(root->context_changed || root->full_exit)
      continue;

    update_screen();

    // Delay and then handle events.
    ctx = root->stack.contents[root->stack.size - 1];

    // FIXME hack
    enable_f12_hack = false;

    switch(ctx->internal_data->framerate)
    {
      case FRAMERATE_UI:
      {
        // Delay for a standard (fixed) amount of time.
        update_event_status_delay();
        break;
      }

      case FRAMERATE_UI_INTERRUPT:
      {
        // Delay for the standard amount of time or until an event is detected.
        // Use for interfaces that need precise keypress detection, like typing.
        update_event_status_intake();
        break;
      }

      case FRAMERATE_MZX_SPEED:
      {
        // Delay according to mzx_speed and execution time.
        if(ctx->world->mzx_speed > 1)
        {
          // Number of ms the update cycle took
          delta_ticks = get_ticks() - start_ticks;
          total_ticks = (16 * (ctx->world->mzx_speed - 1)) - delta_ticks;
          if(total_ticks < 0)
            total_ticks = 0;

          // Delay for 16 * (speed - 1) since the beginning of the update
          delay(total_ticks);
        }

        update_event_status();
        break;
      }

      default:
      {
        print_core_stack(ctx);
        error_message(E_CORE_FATAL_BUG, 5, NULL);
        break;
      }
    }

    // FIXME hack
    enable_f12_hack = conf->allow_screenshots;

    start_ticks = get_ticks();

#ifdef CONFIG_FPS
    update_fps(start_ticks);
#endif

    core_update(root);
  }
  while(!root->full_exit && root->stack.size >= initial_stack_size);
}

/**
 * Indicate that the core loop should exit.
 */

void core_exit(context *ctx)
{
  if(!ctx || !ctx->root)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 4, NULL);
    return;
  }

  ctx->root->full_exit = true;
}

/**
 * Free all context data that still exists.
 */

void core_free(core_context *root)
{
  int i;

  // Destroy all contexts on the stack.
  for(i = root->stack.size - 1; i >= 0; i--)
    destroy_context(root->stack.contents[i]);

  free(root->stack.contents);
  free(root);
}

// Deprecated.
static enum context_type indices[128] = { CTX_MAIN };
static int curr_index = 0;

/**
 * Get the most recent context ID associated with the help system.
 * TODO rename after the deprecated features are no longer necessary.
 */

enum context_type get_context(context *ctx)
{
  if(!curr_index && ctx)
  {
    core_context *root = ctx->root;
    enum context_type ctx_type;
    int i;

    for(i = root->stack.size - 1; i >= 0; i--)
    {
      ctx_type = root->stack.contents[i]->internal_data->context_type;

      // Help system only cares about positive context values.
      if(ctx_type > 0)
        return ctx_type;
    }

    return CTX_MAIN;
  }

  return indices[curr_index];
}

// Deprecated.
void set_context(enum context_type idx)
{
  curr_index++;
  indices[curr_index] = idx;
}

// Deprecated.
void pop_context(void)
{
  if(curr_index > 0)
    curr_index--;
}

// Editor external function pointers (NULL by default).

void (*edit_world)(context *parent, boolean reload_curr_file);
void (*debug_counters)(context *ctx);
void (*draw_debug_box)(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys);
int (*debug_robot_break)(context *ctx, struct robot *cur_robot,
 int id, int lines_run);
int (*debug_robot_watch)(context *ctx, struct robot *cur_robot,
 int id, int lines_run);
void (*debug_robot_config)(struct world *mzx_world);

// Network external function pointers (NULL by default).

void (*check_for_updates)(struct world *mzx_world, boolean is_automatic);