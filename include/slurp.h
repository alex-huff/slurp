#ifndef _SLURP_H
#define _SLURP_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <cairo/cairo.h>

#include "box.h"
#include "cursor-shape-v1-client-protocol.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"

#define TOUCH_ID_EMPTY -1
#define M_PI 3.14159265358979323846
#define DEFAULT_BYTE_ORDER 0b11100100

struct slurp_selection {
  struct slurp_output *current_output;
  int32_t x, y;
  int32_t anchor_x, anchor_y;
  struct slurp_box selection;
  bool has_selection;
};

struct slurp_state {
  bool running;
  bool edit_anchor;
  bool selection_started;
  bool freeze_outputs;
  bool paint_cursors;
  const char *save_path;

  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct wl_compositor *compositor;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct zxdg_output_manager_v1 *xdg_output_manager;
  struct ext_output_image_capture_source_manager_v1 *ext_output_image_capture_source_manager;
  struct ext_image_copy_capture_manager_v1 *ext_image_copy_capture_manager;
  struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
  struct wl_list outputs;  // slurp_output::link
  struct wl_list seats;    // slurp_seat::link

  struct xkb_context *xkb_context;

  struct {
    uint32_t background;
    uint32_t border;
    uint32_t selection;
    uint32_t choice;
  } colors;

  const char *font_family;

  uint32_t border_weight;
  bool display_dimensions;
  bool single_point;
  bool restrict_selection;
  bool resizing_selection;
  struct wl_list boxes; // slurp_box::link
  bool fixed_aspect_ratio;
  double aspect_ratio; // h / w

  struct slurp_box result;
};

struct slurp_capture {
  struct slurp_state *state;
  struct slurp_output *output;
  struct wl_list link;

  enum wl_output_transform transform;

  struct pool_buffer buffer;

  struct ext_image_copy_capture_session_v1 *ext_image_copy_capture_session;
  struct ext_image_copy_capture_frame_v1 *ext_image_copy_capture_frame;
  uint32_t buffer_width, buffer_height;
  enum wl_shm_format shm_format;
  cairo_format_t cairo_format;
  bool has_shm_format;
  uint8_t byte_order;
  bool ready;
};

struct slurp_output {
  struct wl_output *wl_output;
  struct slurp_state *state;
  struct slurp_capture capture;
  struct wl_list link; // slurp_state::outputs

  struct slurp_box geometry;
  struct slurp_box logical_geometry;
  int32_t scale;

  struct wl_surface *surface;
  struct zwlr_layer_surface_v1 *layer_surface;

  struct zxdg_output_v1 *xdg_output;

  struct wl_callback *frame_callback;
  bool configured;
  bool dirty;
  int32_t width, height;
  struct pool_buffer buffers[2];
  struct pool_buffer *current_buffer;

  struct wl_cursor_theme *cursor_theme;
  struct wl_cursor_image *cursor_image;

  cairo_surface_t *drawing_surface;
  cairo_t *drawing_surface_cairo;
  void *drawing_surface_data;
};

struct slurp_seat {
  struct wl_surface *cursor_surface;
  struct slurp_state *state;
  struct wl_seat *wl_seat;
  struct wl_list link; // slurp_state::seats

  // keyboard:
  struct wl_keyboard *wl_keyboard;

  // selection (pointer/touch):

  struct slurp_selection pointer_selection;
  struct slurp_selection touch_selection;

  // pointer:
  struct wl_pointer *wl_pointer;
  enum wl_pointer_button_state button_state;
  uint32_t last_button;

  // keymap:
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  // touch:
  struct wl_touch *wl_touch;
  int32_t touch_id;

  uint8_t draw_width;
};

void set_source_u32(cairo_t *cairo, uint32_t color);

bool box_intersect(const struct slurp_box *a, const struct slurp_box *b);

cairo_format_t wl_shm_format_to_cairo(enum wl_shm_format shm_format, uint8_t *byte_order);

static inline struct slurp_selection *
slurp_seat_current_selection(struct slurp_seat *seat) {
  return seat->touch_selection.has_selection ? &seat->touch_selection
                                             : &seat->pointer_selection;
}
#endif
