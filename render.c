#include <stdio.h>
#include <stdlib.h>

#include "pool-buffer.h"
#include "render.h"
#include "slurp.h"

cairo_format_t wl_shm_format_to_cairo(enum wl_shm_format shm_format, uint8_t *byte_order) {
	*byte_order = DEFAULT_BYTE_ORDER;
	switch(shm_format) {
#ifdef SLURP_LITTLE_ENDIAN
	case WL_SHM_FORMAT_XRGB8888:
		*byte_order = 0b11100100;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_XBGR8888:
		*byte_order = 0b11000110;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_RGBX8888:
		*byte_order = 0b10010011;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_BGRX8888:
		*byte_order = 0b00011011;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_ARGB8888:
		*byte_order = 0b11100100;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_ABGR8888:
		*byte_order = 0b11000110;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_RGBA8888:
		*byte_order = 0b10010011;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_BGRA8888:
		*byte_order = 0b00011011;
		return CAIRO_FORMAT_ARGB32;
#else
	case WL_SHM_FORMAT_XRGB8888:
		*byte_order = 0b00011011;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_XBGR8888:
		*byte_order = 0b10010011;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_RGBX8888:
		*byte_order = 0b11000110;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_BGRX8888:
		*byte_order = 0b11100100;
		return CAIRO_FORMAT_RGB24;
	case WL_SHM_FORMAT_ARGB8888:
		*byte_order = 0b00011011;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_ABGR8888:
		*byte_order = 0b10010011;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_RGBA8888:
		*byte_order = 0b11000110;
		return CAIRO_FORMAT_ARGB32;
	case WL_SHM_FORMAT_BGRA8888:
		*byte_order = 0b11100100;
		return CAIRO_FORMAT_ARGB32;
#endif
	default:
		return CAIRO_FORMAT_INVALID;
	}
}

void set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo, (color >> (3 * 8) & 0xFF) / 255.0,
		(color >> (2 * 8) & 0xFF) / 255.0,
		(color >> (1 * 8) & 0xFF) / 255.0,
		(color >> (0 * 8) & 0xFF) / 255.0);
}

static void draw_rect(cairo_t *cairo, struct slurp_box *box, uint32_t color) {
	set_source_u32(cairo, color);
	cairo_rectangle(cairo, box->x, box->y,
			box->width, box->height);
}

static void draw_rect_border(cairo_t *cairo, struct slurp_box *box, uint32_t color, uint32_t line_width) {
	set_source_u32(cairo, color);
	cairo_set_line_width(cairo, line_width);
	int32_t offset = (line_width + 1) / 2;
	cairo_rectangle(cairo, box->x - offset, box->y - offset,
			box->width + 2 * offset, box->height + 2 * offset);
}

void render(struct slurp_output *output) {
	struct slurp_state *state = output->state;
	struct pool_buffer *buffer = output->current_buffer;
	cairo_t *cairo = buffer->cairo;

	cairo_save(cairo);

	// Clear
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	set_source_u32(cairo, state->selection_started ? state->colors.background : 0x00000000);
	cairo_paint(cairo);

	// Draw option boxes from input
	struct slurp_box *choice_box;
	wl_list_for_each(choice_box, &state->boxes, link) {
		if (box_intersect(&output->logical_geometry,
					choice_box)) {
			draw_rect(cairo, choice_box, state->colors.choice);
			cairo_fill(cairo);
		}
	}

	struct slurp_seat *seat;
	wl_list_for_each(seat, &state->seats, link) {
		struct slurp_selection *current_selection =
			slurp_seat_current_selection(seat);

		if (!current_selection->has_selection) {
			continue;
		}

		if (!box_intersect(&output->logical_geometry,
			&current_selection->selection)) {
			continue;
		}
		struct slurp_box *sel_box = &current_selection->selection;

		draw_rect(cairo, sel_box, state->colors.selection);
		cairo_fill(cairo);

		// Draw border
		draw_rect_border(cairo, sel_box, state->colors.border, state->border_weight);
		cairo_stroke(cairo);

		if (state->display_dimensions) {
			cairo_select_font_face(cairo, state->font_family,
					       CAIRO_FONT_SLANT_NORMAL,
					       CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cairo, 14);
			set_source_u32(cairo, state->colors.border);
			// buffer of 12 can hold selections up to 99999x99999
			char dimensions[12];
			snprintf(dimensions, sizeof(dimensions), "%ix%i",
				 sel_box->width, sel_box->height);
			cairo_move_to(cairo, sel_box->x + sel_box->width + 10,
				      sel_box->y + sel_box->height + 20);
			cairo_show_text(cairo, dimensions);
		}
	}

	cairo_identity_matrix(cairo);

	if (output->capture.ready) {
		enum wl_output_transform transform = output->capture.transform;
		switch (transform) {
			case WL_OUTPUT_TRANSFORM_FLIPPED:
				cairo_scale(cairo, -1, 1);
				cairo_translate(cairo, -(int32_t) output->capture.buffer_width, 0);
				break;
			case WL_OUTPUT_TRANSFORM_NORMAL:
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_90:
				cairo_rotate(cairo, M_PI / 2.0);
				cairo_scale(cairo, 1, -1);
				break;
			case WL_OUTPUT_TRANSFORM_90:
				cairo_rotate(cairo, M_PI / 2.0);
				cairo_translate(cairo, 0, -(int32_t) output->capture.buffer_height);
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_180:
				cairo_rotate(cairo, M_PI);
				cairo_scale(cairo, -1, 1);
				cairo_translate(cairo, 0, -(int32_t) output->capture.buffer_height);
				break;
			case WL_OUTPUT_TRANSFORM_180:
				cairo_rotate(cairo, M_PI);
				cairo_translate(cairo, -(int32_t) output->capture.buffer_width, -(int32_t) output->capture.buffer_height);
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_270:
				cairo_rotate(cairo, M_PI / -2.0);
				cairo_scale(cairo, 1, -1);
				cairo_translate(cairo, -(int32_t) output->capture.buffer_width, -(int32_t) output->capture.buffer_height);
				break;
			case WL_OUTPUT_TRANSFORM_270:
				cairo_rotate(cairo, M_PI / -2.0);
				cairo_translate(cairo, -(int32_t) output->capture.buffer_width, 0);
				break;
		}
		cairo_set_operator(cairo, CAIRO_OPERATOR_DEST_OVER);
		cairo_set_source_surface(cairo, output->capture.buffer.surface, 0, 0);
		cairo_paint(cairo);
		if (transform != WL_OUTPUT_TRANSFORM_NORMAL) {
			cairo_identity_matrix(cairo);
		}
	}

	if (output->drawing_surface) {
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cairo, output->drawing_surface, 0, 0);
		cairo_paint(cairo);
	}

	cairo_restore(cairo);
}
