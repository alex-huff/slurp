#include <stdio.h>
#include <stdlib.h>

#include "pool-buffer.h"
#include "render.h"
#include "slurp.h"

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
	uint32_t offset = (line_width + 1) / 2;
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

	if (state->background_surface) {
		cairo_set_operator(cairo, CAIRO_OPERATOR_DEST_OVER);
		cairo_set_source_surface(cairo, state->background_surface, 0, 0);
		cairo_paint(cairo);
	}

	if (output->drawing_surface) {
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(cairo, output->drawing_surface, 0, 0);
		cairo_paint(cairo);
	}

	cairo_restore(cairo);
}
