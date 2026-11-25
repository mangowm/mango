#include "mango/workspace_overview/workspace_overview_internal.h"

struct wsov_surface_render_data {
	struct wlr_render_pass *pass;
	struct wsov_cell *cell;
	struct wsov_item *item;
	struct wsov_render_resources *resources;
	double scale_x;
	double scale_y;
	double cell_scale;
	pixman_region32_t clip;
};

bool wsov_retain_texture(struct wsov_render_resources *resources,
						 struct wlr_texture *texture, bool owned) {
	if (!resources || !texture)
		return false;
	struct wsov_texture_ref *ref = malloc(sizeof(*ref));
	if (!ref)
		return false;
	ref->texture = texture;
	ref->owned = owned;
	ref->next = resources->textures;
	resources->textures = ref;
	return true;
}

void wsov_release_textures(struct wsov_render_resources *resources) {
	if (!resources)
		return;
	struct wsov_texture_ref *ref = resources->textures;
	while (ref) {
		struct wsov_texture_ref *next = ref->next;
		if (ref->owned)
			wlr_texture_destroy(ref->texture);
		free(ref);
		ref = next;
	}
	resources->textures = NULL;
}

void wsov_rebind_pass(struct wsov_render_resources *resources) {
	if (!resources || !resources->target)
		return;
	GLuint fbo = fx_renderer_get_buffer_fbo(server.renderer, resources->target);
	if (fbo)
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void wsov_item_add(struct wsov_cell *cell, Client *c) {
	if (cell->item_count == cell->item_capacity) {
		int capacity = cell->item_capacity ? cell->item_capacity * 2 : 8;
		struct wsov_item *items =
			realloc(cell->items, capacity * sizeof(*items));
		if (!items)
			return;
		cell->items = items;
		cell->item_capacity = capacity;
	}
	cell->items[cell->item_count++] = (struct wsov_item){
		.client = c,
		.scene = c->scene,
		.border_width = c->bw,
	};
	cell->items[cell->item_count - 1].layer = client_target_layer(c);
}

void wsov_snapshot_item_add(struct wsov_cell *cell, Client *snapshot) {
	if (!snapshot || !snapshot->scene ||
		(snapshot->isglobal ? false
							: !(snapshot->tags & wsov_tag_bit(cell->tag))))
		return;
	if (cell->item_count == cell->item_capacity) {
		int capacity = cell->item_capacity ? cell->item_capacity * 2 : 8;
		struct wsov_item *items =
			realloc(cell->items, capacity * sizeof(*items));
		if (!items)
			return;
		cell->items = items;
		cell->item_capacity = capacity;
	}
	cell->items[cell->item_count++] = (struct wsov_item){
		.client = snapshot,
		.scene = snapshot->scene,
		/*
		 * Close snapshots are rendered from the off-screen cell buffer in
		 * wsov. Keep them on the dedicated fadeout layer so a closing tile
		 * remains visible above the workspace preview it was captured from.
		 */
		.layer = LyrFadeOut,
		.border_width = snapshot->bw,
		.is_snapshot = true,
		.workspace_geom = snapshot->animation.current,
	};
}

struct wsov_item *wsov_cell_item_for_client(struct wsov_cell *cell, Client *c) {
	if (!cell || !c)
		return NULL;
	for (int i = 0; i < cell->item_count; i++) {
		if (!cell->items[i].is_snapshot && cell->items[i].client == c)
			return &cell->items[i];
	}
	return NULL;
}

int wsov_scene_node_sibling_index(struct wlr_scene_node *node) {
	if (!node || !node->parent)
		return INT_MAX;
	int index = 0;
	struct wlr_scene_node *sibling;
	wl_list_for_each(sibling, &node->parent->children, link) {
		if (sibling == node)
			return index;
		index++;
	}
	return INT_MAX;
}

int wsov_item_stack_compare(const void *left, const void *right) {
	const struct wsov_item *a = left;
	const struct wsov_item *b = right;
	if (a->layer != b->layer)
		return a->layer < b->layer ? -1 : 1;

	struct wlr_scene_node *a_node =
		a->client && a->client->scene ? &a->client->scene->node : NULL;
	struct wlr_scene_node *b_node =
		b->client && b->client->scene ? &b->client->scene->node : NULL;
	int a_index = wsov_scene_node_sibling_index(a_node);
	int b_index = wsov_scene_node_sibling_index(b_node);
	if (a_index == b_index)
		return 0;
	return a_index < b_index ? -1 : 1;
}

void wsov_capture_cell_items(struct wsov_cell *cell) {
	cell->item_count = 0;
	Client *c;
	wl_list_for_each_reverse(c, &server.focus_stack, flink) {
		if (!wsov_client_on_tag(c, cell->tag))
			continue;
		wsov_item_add(cell, c);
		cell->items[cell->item_count - 1].workspace_geom = c->animation.current;
	}
	wl_list_for_each(c, &server.fadeout_clients, fadeout_link) {
		if (c->mon != wsov.mon)
			continue;
		wsov_snapshot_item_add(cell, c);
	}
	/*
	 * Reproduce Mango's scene stack for every preview. Scene layers dominate;
	 * within one layer (for example LyrTop when float_full_to_top is enabled),
	 * the real scene sibling order still decides which window is on top.
	 */
	if (cell->item_count > 1)
		qsort(cell->items, cell->item_count, sizeof(*cell->items),
			  wsov_item_stack_compare);
}

void wsov_capture_bar_rect(struct wsov_bar_item *item,
						   struct wlr_scene_rect *rect) {
	if (!rect || !rect->node.enabled || item->rect_count >= 2)
		return;
	int lx, ly;
	if (!wsov_scene_node_layout_coords(&rect->node, &lx, &ly))
		return;
	struct wsov_bar_rect *snapshot = &item->rects[item->rect_count++];
	snapshot->box = (struct wlr_box){lx, ly, rect->width, rect->height};
	memcpy(snapshot->color, rect->color, sizeof(snapshot->color));
}

void wsov_capture_bar_buffer(struct wsov_bar_item *item,
							 struct wlr_scene_buffer *node) {
	if (!node || !node->node.enabled || !node->buffer ||
		item->buffer_count >= 2)
		return;
	int lx, ly;
	if (!wsov_scene_node_layout_coords(&node->node, &lx, &ly))
		return;
	struct wlr_buffer *buffer = wlr_buffer_lock(node->buffer);
	if (!buffer)
		return;
	int width = node->dst_width > 0 ? node->dst_width : node->buffer->width;
	int height = node->dst_height > 0 ? node->dst_height : node->buffer->height;
	item->buffers[item->buffer_count++] = (struct wsov_bar_buffer){
		.box = {lx, ly, width, height},
		.buffer = buffer,
		.src_box = node->src_box,
		.dst_width = node->dst_width,
		.dst_height = node->dst_height,
		.transform = node->transform,
		.opacity = node->opacity,
	};
}

void wsov_capture_bar(struct wsov_cell *cell, MangoBarDecoration *bar) {
	if (!bar)
		return;
	if (cell->bar_item_count == cell->bar_item_capacity) {
		int capacity =
			cell->bar_item_capacity ? cell->bar_item_capacity * 2 : 8;
		struct wsov_bar_item *items =
			realloc(cell->bar_items, capacity * sizeof(*items));
		if (!items)
			return;
		cell->bar_items = items;
		cell->bar_item_capacity = capacity;
	}
	struct wsov_bar_item *item = &cell->bar_items[cell->bar_item_count++];
	memset(item, 0, sizeof(*item));
	Client *owner = bar->node_data;
	item->layer = owner ? client_target_layer(owner) : LyrTile;
	if (owner && !bar->scene_buffer->buffer)
		mango_bar_decoration_update(
			bar, client_get_title(owner),
			wsov.mon->wlr_output ? wsov.mon->wlr_output->scale : 1.0f);
	wsov_capture_bar_rect(item, bar->border);
	wsov_capture_bar_rect(item, bar->bg);
	wsov_capture_bar_buffer(item, bar->scene_buffer);
	wsov_capture_bar_buffer(item, bar->close_buffer);
}

void wsov_clear_cell_bars(struct wsov_cell *cell) {
	for (int i = 0; i < cell->bar_item_count; i++) {
		struct wsov_bar_item *item = &cell->bar_items[i];
		for (int j = 0; j < item->buffer_count; j++)
			wlr_buffer_unlock(item->buffers[j].buffer);
	}
	cell->bar_item_count = 0;
}

void wsov_draw_group_bar_for_capture(Client *c) {
	if (!c)
		return;
	struct wlr_box saved = c->animation.current;
	/* Bar layout must use the arranged workspace geometry. During tag changes
	 * animation.current can still contain the source/floating size. */
	c->animation.current = c->geom;
	client_draw_group_bar(c, (struct ivec2){0});
	c->animation.current = saved;
}

void wsov_draw_tabbar_for_capture(Client *c) {
	if (!c)
		return;
	struct wlr_box saved = c->animation.current;
	c->animation.current = c->geom;
	client_draw_tabbar(c, (struct ivec2){0});
	c->animation.current = saved;
}

bool wsov_group_capture_owner(Client *c) {
	for (Client *cur = client_group_head(c); cur; cur = cur->group_next) {
		if (cur == c)
			return true;
		if (!client_is_parked(cur))
			return false;
	}
	return true;
}

void wsov_capture_cell_bars(struct wsov_cell *cell) {
	wsov_clear_cell_bars(cell);
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (!c->group_bar && !c->tab_bar)
			continue;

		Client *group_head = c->group_bar ? client_group_head(c) : NULL;
		if (group_head && wsov_group_capture_owner(c)) {
			Client *group_focus = client_group_focused(group_head);
			Client *group_anchor = NULL;
			if (group_focus && group_focus->mon == wsov.mon &&
				!group_focus->is_tab_hidden &&
				(group_focus->isglobal ||
				 (group_focus->tags & wsov_tag_bit(cell->tag))) &&
				client_wants_group_bar(group_focus) && ISNORMAL(group_focus) &&
				!group_focus->isfullscreen) {
				group_anchor = group_focus;
			} else {
				for (Client *cur = group_head; cur; cur = cur->group_next) {
					if (cur->mon == wsov.mon && !cur->is_tab_hidden &&
						(cur->isglobal ||
						 (cur->tags & wsov_tag_bit(cell->tag))) &&
						client_wants_group_bar(cur) && ISNORMAL(cur) &&
						!cur->isfullscreen) {
						group_anchor = cur;
						break;
					}
				}
			}
			if (group_anchor && !wsov_drag_hides_group(cell, group_head)) {
				/* Arrange does not render decorations. Recalculate the group
				 * strip against this tag's arranged geometry before taking the
				 * snapshots, including members that are parked from
				 * server.clients. */
				wsov_draw_group_bar_for_capture(group_anchor);
				for (Client *cur = group_head; cur; cur = cur->group_next) {
					if (cur->group_bar)
						wsov_capture_bar(cell, cur->group_bar);
				}
			}
		}

		if (!c->tab_prev) {
			Client *tab_head = c;
			Client *tab_anchor = NULL;
			for (Client *cur = tab_head; cur; cur = cur->tab_next) {
				if (cur->mon == wsov.mon && cur->is_tab_focus &&
					!cur->is_tab_hidden &&
					(cur->isglobal || (cur->tags & wsov_tag_bit(cell->tag)))) {
					tab_anchor = cur;
					break;
				}
			}
			if (tab_anchor)
				wsov_draw_tabbar_for_capture(tab_anchor);
			for (Client *cur = tab_head; cur; cur = cur->tab_next) {
				if (cur->tab_bar && cur->tab_bar->scene->node.enabled)
					wsov_capture_bar(cell, cur->tab_bar);
			}
		}
	}
}

void wsov_position_cell_items(struct wsov_cell *cell) {
	double scale_x = (double)cell->buffer_w / MANGO_MAX(1, wsov.mon->m.width);
	double scale_y = (double)cell->buffer_h / MANGO_MAX(1, wsov.mon->m.height);
	for (int i = 0; i < cell->item_count; i++) {
		struct wlr_box geo = cell->items[i].workspace_geom;
		int x = cell->box.x + wsov_border_width() +
				(int)lround((geo.x - wsov.mon->m.x) * scale_x);
		int y = cell->box.y + wsov_border_width() +
				(int)lround((geo.y - wsov.mon->m.y) * scale_y);
		int w = MANGO_MAX(1, (int)lround(geo.width * scale_x));
		int h = MANGO_MAX(1, (int)lround(geo.height * scale_y));
		cell->items[i].box = (struct wlr_box){x, y, w, h};
		cell->items[i].render_scale = fmin(scale_x, scale_y);
		cell->items[i].render_scale_x = scale_x;
		cell->items[i].render_scale_y = scale_y;
	}
}

void wsov_capture_workspace_tags(uint32_t tags) {
	if (!workspace_overview_is_active() || wsov.cell_count <= 0)
		return;
	bool saved_capturing_layout = wsov.capturing_layout;
	wsov.capturing_layout = true;

	Monitor *m = wsov.mon;
	uint32_t selected = 0;
	for (int i = 0; i < wsov.cell_count; i++) {
		uint32_t bit = wsov_tag_bit(wsov.cells[i].tag);
		if (tags & bit)
			selected |= bit;
	}
	if (!selected) {
		wsov.capturing_layout = saved_capturing_layout;
		return;
	}

	uint32_t saved_tags[2] = {m->tagset[0], m->tagset[1]};
	uint32_t saved_seltags = m->seltags;
	uint32_t saved_curtag = m->pertag->curtag;
	uint32_t saved_prevtag = m->pertag->prevtag;
	Client *saved_sel = m->sel;
	Client *saved_prevsel = m->prevsel;
	bool saved_tag_gather = config.tag_gather;

	sync_tag_ratio_state_from_clients(m, saved_curtag);
	config.tag_gather = false;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c) &&
			(c->isglobal || (c->tags & selected))) {
			c->wsov_hidden = false;
			client_update_visibility(c);
		}
	}

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		uint32_t tag = cell->tag;
		if (!(selected & wsov_tag_bit(tag)))
			continue;
		m->tagset[saved_seltags] = wsov_tag_bit(tag);
		m->pertag->curtag = tag;
		arrange(m, false, true);
		wsov_settle_tag_animations(m, selected);
		wsov_capture_cell_items(cell);
		wsov_capture_cell_bars(cell);
	}

	m->seltags = saved_seltags;
	m->tagset[0] = saved_tags[0];
	m->tagset[1] = saved_tags[1];
	m->pertag->curtag = saved_curtag;
	m->pertag->prevtag = saved_prevtag;
	arrange(m, false, true);
	wsov_settle_tag_animations(m, selected);
	m->sel = saved_sel;
	m->prevsel = saved_prevsel;
	config.tag_gather = saved_tag_gather;

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c) &&
			(c->isglobal || (c->tags & selected))) {
			c->wsov_hidden = true;
			client_update_visibility(c);
		}
	}
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (selected & wsov_tag_bit(cell->tag))
			wsov_position_cell_items(cell);
	}
	wsov.capturing_layout = saved_capturing_layout;
}

void wsov_capture_workspace_layouts(void) {
	uint32_t tags = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		tags |= wsov_tag_bit(wsov.cells[i].tag);
	wsov_capture_workspace_tags(tags);
}

const struct wlr_drm_format *wsov_pick_buffer_format(void) {
	uint32_t caps = server.allocator->buffer_caps;
	const struct wlr_drm_format_set *formats =
		wlr_renderer_get_texture_formats(server.renderer, caps);
	if (!formats)
		return NULL;

	static const uint32_t candidates[] = {
		DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_XRGB8888,
		DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	};
	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		const struct wlr_drm_format *format =
			wlr_drm_format_set_get(formats, candidates[i]);
		if (format)
			return format;
	}
	return NULL;
}

struct wlr_buffer *wsov_create_buffer(int width, int height) {
	if (!server.allocator || !server.renderer || width <= 0 || height <= 0)
		return NULL;
	const struct wlr_drm_format *format = wsov_pick_buffer_format();
	if (!format)
		return NULL;
	return wlr_allocator_create_buffer(server.allocator, width, height, format);
}

void wsov_release_cell_buffers(struct wsov_cell *cell) {
	if (!cell)
		return;
	for (int i = 0; i < 2; i++) {
		if (cell->contents_buffers[i]) {
			wlr_buffer_drop(cell->contents_buffers[i]);
			cell->contents_buffers[i] = NULL;
		}
		if (cell->foreground_buffers[i]) {
			wlr_buffer_drop(cell->foreground_buffers[i]);
			cell->foreground_buffers[i] = NULL;
		}
	}
	cell->pool_width = 0;
	cell->pool_height = 0;
}

struct wlr_buffer *wsov_acquire_cell_buffer(struct wsov_cell *cell,
											struct wlr_scene_buffer *target,
											int width, int height) {
	if (!cell || !target)
		return NULL;

	struct wlr_buffer **pool = target == cell->contents
								   ? cell->contents_buffers
								   : cell->foreground_buffers;
	if (cell->pool_width != width || cell->pool_height != height) {
		for (int i = 0; i < 2; i++) {
			if (cell->contents_buffers[i]) {
				wlr_buffer_drop(cell->contents_buffers[i]);
				cell->contents_buffers[i] = NULL;
			}
			if (cell->foreground_buffers[i]) {
				wlr_buffer_drop(cell->foreground_buffers[i]);
				cell->foreground_buffers[i] = NULL;
			}
		}
		cell->pool_width = width;
		cell->pool_height = height;
	}

	/*
	 * A zero lock count means the compositor has finished presenting that
	 * buffer. Reusing only released buffers keeps the previous frame visible
	 * until the replacement has been fully rendered, so this cannot introduce
	 * tearing or stale pixels.
	 */
	for (int i = 0; i < 2; i++) {
		if (pool[i] && pool[i]->n_locks == 0) {
			return pool[i];
		}
	}

	int slot = 0;
	for (int i = 0; i < 2; i++) {
		if (!pool[i] || pool[i]->n_locks == 0) {
			slot = i;
			break;
		}
		slot = i;
	}
	struct wlr_buffer *buffer = wsov_create_buffer(width, height);
	if (!buffer)
		return NULL;
	if (pool[slot])
		wlr_buffer_drop(pool[slot]);
	pool[slot] = buffer;
	return buffer;
}

void wsov_render_scene_buffer(struct wlr_scene_buffer *scene_buffer, int sx,
							  int sy, void *data) {
	struct wsov_surface_render_data *render = data;
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_buffer->buffer)
		return;

	struct wlr_client_buffer *client_buffer =
		wlr_client_buffer_get(scene_buffer->buffer);
	struct wlr_texture *texture = client_buffer ? client_buffer->texture : NULL;
	bool owned = false;
	if (!texture && scene_buffer->WLR_PRIVATE.texture) {
		texture = scene_buffer->WLR_PRIVATE.texture;
	}
	if (!texture && scene_surface && scene_surface->surface->buffer &&
		scene_surface->surface->buffer->texture) {
		texture = scene_surface->surface->buffer->texture;
	}
	if (!texture) {
		texture =
			wlr_texture_from_buffer(server.renderer, scene_buffer->buffer);
		owned = true;
	}
	if (!texture)
		return;
	wsov_rebind_pass(render->resources);
	if (!wsov_retain_texture(render->resources, texture, owned)) {
		if (owned)
			wlr_texture_destroy(texture);
		return;
	}

	int surface_width = scene_buffer->dst_width > 0
							? scene_buffer->dst_width
							: scene_buffer->buffer->width;
	int surface_height = scene_buffer->dst_height > 0
							 ? scene_buffer->dst_height
							 : scene_buffer->buffer->height;

	int cell_x = render->cell->box.x + wsov_border_width();
	int cell_y = render->cell->box.y + wsov_border_width();
	int dx;
	int dy;
	if (render->item->is_snapshot) {
		dx = (int)lround((sx - wsov.mon->m.x) * render->scale_x);
		dy = (int)lround((sy - wsov.mon->m.y) * render->scale_y);
	} else {
		dx = (int)lround((render->item->box.x - cell_x) * render->cell_scale +
						 sx * render->scale_x);
		dy = (int)lround((render->item->box.y - cell_y) * render->cell_scale +
						 sy * render->scale_y);
	}
	int dw = MANGO_MAX(1, (int)lround(surface_width * render->scale_x));
	int dh = MANGO_MAX(1, (int)lround(surface_height * render->scale_y));

	float alpha = scene_buffer->opacity;
	struct wlr_fbox source_box = scene_buffer->src_box;
	if (source_box.width <= 0 || source_box.height <= 0) {
		source_box = (struct wlr_fbox){0, 0, scene_buffer->buffer->width,
									   scene_buffer->buffer->height};
	}
	struct wlr_color_primaries primaries = {0};
	if (scene_surface)
		wlr_color_primaries_from_named(&primaries, scene_buffer->primaries);
	struct wlr_linux_drm_syncobj_surface_v1_state *syncobj =
		scene_surface
			? wlr_linux_drm_syncobj_v1_get_surface_state(scene_surface->surface)
			: NULL;
	wlr_render_pass_add_texture(
		render->pass,
		&(struct wlr_render_texture_options){
			.texture = texture,
			.src_box = source_box,
			.dst_box = {dx, dy, dw, dh},
			.alpha = &alpha,
			.clip = &render->clip,
			.transform = scene_buffer->transform,
			.filter_mode = WLR_SCALE_FILTER_BILINEAR,
			.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
			.transfer_function = scene_buffer->transfer_function,
			.primaries = scene_surface ? &primaries : NULL,
			.color_encoding = scene_buffer->color_encoding,
			.color_range = scene_buffer->color_range,
			.wait_timeline = syncobj ? syncobj->acquire_timeline : NULL,
			.wait_point = syncobj ? syncobj->acquire_point : 0,
		});
}

void wsov_render_scene_surfaces(struct wlr_scene_tree *tree, int origin_x,
								int origin_y, void *data) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled)
			continue;
		int x = origin_x + node->x;
		int y = origin_y + node->y;
		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
			wsov_render_scene_buffer(buffer, x, y, data);
		} else if (node->type == WLR_SCENE_NODE_TREE) {
			wsov_render_scene_surfaces(wlr_scene_tree_from_node(node), x, y,
									   data);
		}
	}
}

void wsov_add_rect(struct wlr_render_pass *pass, struct wlr_box box,
				   const float color[4],
				   enum wlr_render_blend_mode blend_mode) {
	if (box.width <= 0 || box.height <= 0)
		return;
	wlr_render_pass_add_rect(
		pass, &(struct wlr_render_rect_options){
				  .box = box,
				  .color =
					  {
						  .r = wsov_premultiplied(color[0], color[3]),
						  .g = wsov_premultiplied(color[1], color[3]),
						  .b = wsov_premultiplied(color[2], color[3]),
						  .a = color[3],
					  },
				  .blend_mode = blend_mode,
			  });
}

bool wsov_fadeout_client_alive(const Client *client) {
	Client *c;
	wl_list_for_each(c, &server.fadeout_clients, fadeout_link) {
		if (c == client)
			return true;
	}
	return false;
}

void wsov_render_client(struct wlr_render_pass *pass, struct wsov_cell *cell,
						struct wsov_item *item,
						struct wsov_render_resources *resources,
						double buffer_scale) {
	/*
	 * A snapshot may have completed its close animation after the cell was
	 * captured. Its Client and scene are no longer valid, so do not dereference
	 * either one unless it is still owned by the fadeout list.
	 */
	if (item->is_snapshot && !wsov_fadeout_client_alive(item->client))
		return;

	Client *c = item->client;
	int cell_x = cell->box.x + wsov_border_width();
	int cell_y = cell->box.y + wsov_border_width();
	double render_scale_x = item->render_scale_x * buffer_scale;
	double render_scale_y = item->render_scale_y * buffer_scale;
	double render_scale = fmin(render_scale_x, render_scale_y);
	struct wlr_box window_box = {
		.x = (int)lround((item->box.x - cell_x) * buffer_scale),
		.y = (int)lround((item->box.y - cell_y) * buffer_scale),
		.width = (int)lround(item->box.width * buffer_scale),
		.height = (int)lround(item->box.height * buffer_scale),
	};

	const float *frame_color = (!item->is_snapshot && c && c->border)
								   ? get_border_color(c)
								   : config.bordercolor;
	int border_x = !item->is_snapshot && item->border_width > 0
					   ? MANGO_MAX(1, (int)lround((double)item->border_width *
												  render_scale_x))
					   : 0;
	int border_y = !item->is_snapshot && item->border_width > 0
					   ? MANGO_MAX(1, (int)lround((double)item->border_width *
												  render_scale_y))
					   : 0;
	bool live = wsov_item_is_live(cell, item);
	if (!wsov_item_has_effect_border(cell, item)) {
		wsov_add_rect(pass,
					  (struct wlr_box){window_box.x, window_box.y,
									   window_box.width, border_y},
					  frame_color, WLR_RENDER_BLEND_MODE_NONE);
		wsov_add_rect(
			pass,
			(struct wlr_box){window_box.x,
							 window_box.y + window_box.height - border_y,
							 window_box.width, border_y},
			frame_color, WLR_RENDER_BLEND_MODE_NONE);
		wsov_add_rect(pass,
					  (struct wlr_box){window_box.x, window_box.y, border_x,
									   window_box.height},
					  frame_color, WLR_RENDER_BLEND_MODE_NONE);
		wsov_add_rect(
			pass,
			(struct wlr_box){window_box.x + window_box.width - border_x,
							 window_box.y, border_x, window_box.height},
			frame_color, WLR_RENDER_BLEND_MODE_NONE);
	}

	if (!item->is_snapshot && (window_box.width <= 2 || window_box.height <= 2))
		return;

	if (render_scale <= 0.0)
		return;
	/* Live clients own their surface and effects. Only the frame and non-live
	 * content belong in the offscreen cell buffer. */
	if (live)
		return;

	pixman_region32_t clip;
	int clip_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int clip_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	pixman_region32_init_rect(&clip, 0, 0, clip_w, clip_h);
	struct wsov_surface_render_data data = {
		.pass = pass,
		.cell = cell,
		.item = item,
		.resources = resources,
		.scale_x = render_scale_x,
		.scale_y = render_scale_y,
		.cell_scale = buffer_scale,
		.clip = clip,
	};
	struct wlr_scene_tree *scene =
		item->scene ? item->scene : (c ? c->scene : NULL);
	if (!scene)
		return;
	if (item->is_snapshot) {
		wsov_render_scene_surfaces(scene, scene->node.x, scene->node.y, &data);
	} else {
		wsov_render_scene_surfaces(scene, 0, 0, &data);
	}
	pixman_region32_fini(&clip);
}

void wsov_render_bar_item_at(struct wlr_render_pass *pass,
							 struct wsov_bar_item *item,
							 struct wsov_render_resources *resources,
							 int origin_x, int origin_y, double scale_x,
							 double scale_y, pixman_region32_t *clip) {
	for (int i = 0; i < item->rect_count; i++) {
		struct wsov_bar_rect *rect = &item->rects[i];
		struct wlr_box box = {
			.x = (int)lround((rect->box.x - origin_x) * scale_x),
			.y = (int)lround((rect->box.y - origin_y) * scale_y),
			.width = MANGO_MAX(1, (int)lround(rect->box.width * scale_x)),
			.height = MANGO_MAX(1, (int)lround(rect->box.height * scale_y)),
		};
		wsov_add_rect(pass, box, rect->color,
					  WLR_RENDER_BLEND_MODE_PREMULTIPLIED);
	}

	for (int i = 0; i < item->buffer_count; i++) {
		struct wsov_bar_buffer *snapshot = &item->buffers[i];
		struct wlr_client_buffer *client_buffer =
			wlr_client_buffer_get(snapshot->buffer);
		struct wlr_texture *texture =
			client_buffer ? client_buffer->texture : NULL;
		bool owned = false;
		if (!texture) {
			texture =
				wlr_texture_from_buffer(server.renderer, snapshot->buffer);
			owned = true;
		}
		if (!texture)
			continue;
		if (!wsov_retain_texture(resources, texture, owned)) {
			if (owned)
				wlr_texture_destroy(texture);
			continue;
		}
		int buffer_w = snapshot->buffer->width;
		int buffer_h = snapshot->buffer->height;
		struct wlr_fbox src = snapshot->src_box;
		if (src.width <= 0 || src.height <= 0)
			src = (struct wlr_fbox){0, 0, buffer_w, buffer_h};
		int dst_w = snapshot->dst_width > 0 ? snapshot->dst_width : buffer_w;
		int dst_h = snapshot->dst_height > 0 ? snapshot->dst_height : buffer_h;
		float alpha = snapshot->opacity;
		struct wlr_box dst = {
			.x = (int)lround((snapshot->box.x - origin_x) * scale_x),
			.y = (int)lround((snapshot->box.y - origin_y) * scale_y),
			.width = MANGO_MAX(1, (int)lround(dst_w * scale_x)),
			.height = MANGO_MAX(1, (int)lround(dst_h * scale_y)),
		};
		wlr_render_pass_add_texture(
			pass, &(struct wlr_render_texture_options){
					  .texture = texture,
					  .src_box = src,
					  .dst_box = dst,
					  .alpha = &alpha,
					  .clip = clip,
					  .transform = snapshot->transform,
					  .filter_mode = WLR_SCALE_FILTER_BILINEAR,
					  .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
					  .transfer_function = WLR_COLOR_TRANSFER_FUNCTION_GAMMA22,
					  .color_encoding = WLR_COLOR_ENCODING_NONE,
					  .color_range = WLR_COLOR_RANGE_NONE,
				  });
	}
}

void wsov_render_cell_layer_range(struct wlr_render_pass *pass,
								  struct wsov_cell *cell,
								  struct wsov_render_resources *resources,
								  uint32_t first_layer, uint32_t end_layer,
								  double buffer_scale) {
	double scale_x =
		(double)cell->buffer_w / MANGO_MAX(1, wsov.mon->m.width) * buffer_scale;
	double scale_y = (double)cell->buffer_h / MANGO_MAX(1, wsov.mon->m.height) *
					 buffer_scale;
	pixman_region32_t clip;
	int clip_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int clip_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	pixman_region32_init_rect(&clip, 0, 0, clip_w, clip_h);

	for (uint32_t layer = first_layer; layer < end_layer; layer++) {
		for (int i = 0; i < cell->item_count; i++) {
			struct wsov_item *item = &cell->items[i];
			if (item->layer != layer ||
				wsov_drag_hides_item(cell, item->client))
				continue;
			wsov_render_client(pass, cell, item, resources, buffer_scale);
		}
		for (int i = 0; i < cell->bar_item_count; i++) {
			struct wsov_bar_item *item = &cell->bar_items[i];
			if (item->layer == layer) {
				wsov_render_bar_item_at(pass, item, resources, wsov.mon->m.x,
										wsov.mon->m.y, scale_x, scale_y, &clip);
			}
		}
	}
	pixman_region32_fini(&clip);
}

bool wsov_cell_has_layer_range(const struct wsov_cell *cell,
							   uint32_t first_layer, uint32_t end_layer) {
	for (int i = 0; i < cell->item_count; i++) {
		if (cell->items[i].layer >= first_layer &&
			cell->items[i].layer < end_layer)
			return true;
	}
	for (int i = 0; i < cell->bar_item_count; i++) {
		if (cell->bar_items[i].layer >= first_layer &&
			cell->bar_items[i].layer < end_layer)
			return true;
	}
	return false;
}

void wsov_render_cell_part(struct wsov_cell *cell,
						   struct wlr_scene_buffer *target,
						   uint32_t first_layer, uint32_t end_layer,
						   double buffer_scale) {
	if (!target)
		return;
	if (!wsov_cell_has_layer_range(cell, first_layer, end_layer)) {
		wlr_scene_buffer_set_buffer(target, NULL);
		return;
	}

	int buffer_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int buffer_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	struct wlr_buffer *buffer =
		wsov_acquire_cell_buffer(cell, target, buffer_w, buffer_h);
	if (!buffer)
		return;

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server.renderer, buffer, NULL);
	if (!pass) {
		return;
	}

	float background[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	struct wsov_render_resources resources = {0};
	resources.target = buffer;
	wsov_add_rect(pass, (struct wlr_box){0, 0, buffer_w, buffer_h}, background,
				  WLR_RENDER_BLEND_MODE_NONE);
	wsov_render_cell_layer_range(pass, cell, &resources, first_layer, end_layer,
								 buffer_scale);

	if (!wlr_render_pass_submit(pass)) {
		wsov_release_textures(&resources);
		return;
	}
	wsov_release_textures(&resources);

	wlr_scene_buffer_set_buffer(target, buffer);
	wlr_scene_buffer_set_source_box(target, NULL);
	wlr_scene_buffer_set_filter_mode(target, WLR_SCALE_FILTER_BILINEAR);
}

void wsov_prepare_cell_for_render(struct wsov_cell *cell) {
	if (!cell || !cell->capture_pending)
		return;
	wsov_capture_cell_items(cell);
	/* Only the active view owns the global tab projection. Other workspaces
	 * retain their last complete strip snapshot. */
	if (cell->tag == get_mon_curtag(wsov.mon))
		wsov_capture_cell_bars(cell);
	wsov_position_cell_items(cell);
	cell->capture_pending = false;
	cell->contents_dirty = true;
	cell->foreground_dirty = true;
}

void wsov_render_cell_raw(struct wsov_cell *cell, double buffer_scale) {
	if (!cell || !cell->contents || !cell->foreground || cell->buffer_w <= 0 ||
		cell->buffer_h <= 0)
		return;

	wsov_render_cell_part(cell, cell->contents, 0, LyrTop, buffer_scale);
	wsov_render_cell_part(cell, cell->foreground, LyrTop, NUM_LAYERS,
						  buffer_scale);
	cell->contents_dirty = false;
	cell->foreground_dirty = false;
}

bool wsov_render_cell(struct wsov_cell *cell, double buffer_scale) {
	if (!cell)
		return false;
	if (!wsov_cell_preview_visible(cell, wsov.progress)) {
		cell->contents_dirty = true;
		cell->foreground_dirty = true;
		return false;
	}
	wsov_prepare_cell_for_render(cell);
	wsov_render_cell_raw(cell, buffer_scale);
	return true;
}

bool wsov_refresh_animated_cells(void) {
	if (!workspace_overview_is_active())
		return false;

	bool any_animated = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!wsov_cell_live_content_visible(cell, wsov.progress)) {
			if (!cell->capture_pending) {
				cell->capture_pending = true;
				cell->contents_dirty = true;
				cell->foreground_dirty = true;
			}
			continue;
		}
		bool animated = false;

		Client *c;
		wl_list_for_each(c, &server.clients, link) {
			if (!wsov_client_on_tag(c, cell->tag))
				continue;
			if (c->animation.running || c->opacity_animation.running ||
				c->need_output_flush) {
				animated = true;
				break;
			}
		}
		if (!animated) {
			wl_list_for_each(c, &server.fadeout_clients, fadeout_link) {
				if (c->mon != wsov.mon ||
					(!c->isglobal && !(c->tags & wsov_tag_bit(cell->tag))))
					continue;
				/* Fadeout snapshots are ticked directly from the output
				 * frame and do not use animation.running. */
				animated = true;
				break;
			}
		}
		if (!animated)
			continue;

		cell->capture_pending = true;
		wsov_prepare_cell_for_render(cell);
		any_animated = true;
	}
	return any_animated;
}

bool wsov_render_dirty_cells(bool apply_camera) {
	bool rendered = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!cell->contents_dirty && !cell->foreground_dirty)
			continue;
		if (!wsov_cell_preview_visible(cell, wsov.progress))
			continue;
		wsov_prepare_cell_for_render(cell);

		if (cell->contents_dirty)
			wsov_render_cell_part(cell, cell->contents, 0, LyrTop,
								  wsov_output_scale());
		if (cell->foreground_dirty)
			wsov_render_cell_part(cell, cell->foreground, LyrTop, NUM_LAYERS,
								  wsov_output_scale());
		cell->contents_dirty = false;
		cell->foreground_dirty = false;
		rendered = true;
	}

	if (rendered && apply_camera)
		wsov_apply_camera(wsov.progress);
	return rendered;
}

void wsov_flush_dirty_cells(void *data) {
	(void)data;
	wsov.render_idle = NULL;
	if (!workspace_overview_is_active() || wsov.closing)
		return;
	if (wsov.animating || wsov.layout_animating || wsov.scroll_animating)
		return;
	wsov_render_dirty_cells(true);
}

void wsov_schedule_dirty_render(void) {
	if (wsov.animating || wsov.layout_animating || wsov.scroll_animating)
		return;
	if (wsov.render_idle)
		return;
	wsov.render_idle =
		wl_event_loop_add_idle(server.event_loop, wsov_flush_dirty_cells, NULL);
	if (!wsov.render_idle)
		wsov_flush_dirty_cells(NULL);
}

void wsov_cancel_scheduled_render(void) {
	if (!wsov.render_idle)
		return;
	wl_event_source_remove(wsov.render_idle);
	wsov.render_idle = NULL;
}

void wsov_render_all(void) {
	double buffer_scale = wsov_output_scale();
	for (int i = 0; i < wsov.cell_count; i++)
		wsov_render_cell(&wsov.cells[i], buffer_scale);
}

void wsov_render_tags(uint32_t tags) {
	double buffer_scale = wsov_output_scale();
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (tags & wsov_tag_bit(cell->tag))
			wsov_render_cell(cell, buffer_scale);
	}
}
