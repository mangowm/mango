#include "mango/workspace_overview/workspace_overview_internal.h"

float wsov_premultiplied(float component, float alpha) {
	return component * alpha;
}

void wsov_set_rect_color(struct wlr_scene_rect *rect, const float color[4]) {
	float premultiplied[4] = {
		wsov_premultiplied(color[0], color[3]),
		wsov_premultiplied(color[1], color[3]),
		wsov_premultiplied(color[2], color[3]),
		color[3],
	};
	wlr_scene_rect_set_color(rect, premultiplied);
}

void wsov_update_styles_tags(uint32_t tags) {
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!(tags & wsov_tag_bit(cell->tag)))
			continue;
		bool focused = cell->tag == wsov.target_tag;
		if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL) {
			cell->dim_enabled = false;
			mango_dim_node_set_enabled(cell->dim, false);
			cell->borders_enabled = false;
			for (int j = 0; j < 4; j++)
				wlr_scene_node_set_enabled(&cell->border[j]->node, false);
		} else {
			float dim_opacity =
				focused ? config.workspace_overview_focused_dim_opacity
						: config.workspace_overview_unfocused_dim_opacity;
			float dim_color[4] = {0.0f, 0.0f, 0.0f, dim_opacity};
			mango_dim_node_set_color(cell->dim, dim_color);
			mango_dim_node_set_box(cell->dim, cell->box.x + wsov_border_width(),
								   cell->box.y + wsov_border_width(),
								   cell->buffer_w, cell->buffer_h);
			cell->dim_enabled = dim_opacity > 0.0f;
			mango_dim_node_set_enabled(cell->dim, cell->dim_enabled);
			wlr_scene_node_raise_to_top(&cell->dim->scene_buffer->node);
			for (int j = 0; j < 4; j++) {
				wsov_set_rect_color(cell->border[j], focused
														 ? config.focuscolor
														 : config.bordercolor);
			}
		}
	}
	if (wsov.drop_fill) {
		wsov_set_rect_color(wsov.drop_fill, config.dropcolor);
		if (wsov.drop_indicator_visible)
			wlr_scene_node_raise_to_top(&wsov.drop_fill->node);
	}
	if (wsov.drag_buffer)
		wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
}

void wsov_update_styles(void) {
	uint32_t tags = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		tags |= wsov_tag_bit(wsov.cells[i].tag);
	wsov_update_styles_tags(tags);
}

void wsov_set_dim_enabled(bool enabled) {
	for (int i = 0; i < wsov.cell_count; i++) {
		wsov.cells[i].dim_enabled = enabled;
		mango_dim_node_set_enabled(wsov.cells[i].dim, enabled);
	}
}

void wsov_set_borders_enabled(bool enabled) {
	if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL)
		enabled = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		wsov.cells[i].borders_enabled = enabled;
		for (int j = 0; j < 4; j++)
			wlr_scene_node_set_enabled(&wsov.cells[i].border[j]->node, enabled);
	}
}
