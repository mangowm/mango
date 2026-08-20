// flat alt-tab switcher: a panel of thumbnails for all mapped clients
// across every tag and monitor, drawn in LyrOverlay on the focused monitor.
// windows are never retiled. cycling only moves a highlight over a candidate
// list frozen from fstack at open, so mid-cycle focus churn cannot reorder
// it. releasing the mod key commits focus, escape cancels.
//
// thumbnails are plain scene buffers referencing each client's current
// buffer, refreshed from a surface commit listener, so tiles stay live
// for rendering clients and hold the last frame for suspended ones. a
// second wlr_scene_surface per surface must not be used here: scene
// surfaces share per-surface frame pacing state, and destroying the
// extra one marks the surface output suspended while the real buffer's
// outputs never re-fire, so the client stops receiving frame callbacks
// and freezes

enum {
	SW_TILE_H = 160, /* thumbnail content height */
	SW_TILE_MINW = 90,
	SW_TILE_MAXW = 384,
	SW_PAD = 6,	 /* frame border around each thumbnail */
	SW_GAP = 14, /* spacing between tiles */
	SW_MARGIN = 28,
};

static const float switcher_panel_color[4] = {0.09f, 0.09f, 0.11f, 0.92f};

struct switcher_tile;

// one snapshot node per surface (subsurfaces included). geometry is
// captured at creation and refreshed on surface commit, so relayout
// itself never touches client state
struct switcher_tile_buffer {
	struct switcher_tile *tile;
	struct wlr_surface *surface;
	struct wlr_scene_buffer *buffer;
	int sx, sy;	  /* offset relative to the root surface */
	float lw, lh; /* logical surface size at capture */
	bool is_root;
	struct wl_list link;
	struct wl_listener commit;	/* mirror newly committed buffers */
	struct wl_listener destroy; /* drop the node with its surface */
};

struct switcher_tile {
	Client *c;
	struct wlr_scene_tree *tree;	/* tile root, child of the panel tree */
	struct wlr_scene_rect *frame;	/* highlight border drawn as backing rect */
	struct wlr_scene_tree *content; /* thumbnail snapshot nodes */
	struct wl_list buffers;			/* struct switcher_tile_buffer */
	struct wlr_box clip;			/* content clip at capture */
	float src_w, src_h;				/* content size at capture */
	float root_ratio_x, root_ratio_y; /* root logical to buffer pixels */
	int cw;							  /* thumbnail content width */
	int row;
};

static struct {
	bool active;
	Monitor *mon;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *bg;
	struct switcher_tile **tiles;
	int count;
	int index;
	int x, y, panel_w, panel_h;
	uint32_t open_mods; /* modifiers held at open, gates the commit */
} sw;

bool switcher_is_active(void) { return sw.active; }

// x keycode to wlr modifier bit for the keycodes ISMODEKEYCODE accepts
static uint32_t switcher_keycode_mod(uint32_t keycode) {
	switch (keycode) {
	case 133:
	case 134:
		return WLR_MODIFIER_LOGO;
	case 37:
	case 105:
		return WLR_MODIFIER_CTRL;
	case 64:
	case 108:
		return WLR_MODIFIER_ALT;
	case 50:
	case 62:
		return WLR_MODIFIER_SHIFT;
	}
	return 0;
}

// a mod key release commits only once the last modifier held at open
// goes up, so shift may toggle cycle direction while the opener is held.
// mods may be sampled before or after the release is folded into the
// xkb state, masking the released bit out makes both orderings agree
bool switcher_should_commit(uint32_t keycode, uint32_t mods) {
	uint32_t bit = switcher_keycode_mod(keycode);
	// opened with no trackable modifier held, any mod release commits
	if (!sw.open_mods)
		return true;
	if (!(bit & sw.open_mods))
		return false;
	return (sw.open_mods & mods & ~bit) == 0;
}

// thumbnails must never take pointer focus away from real windows
static bool switcher_buffer_reject_input(struct wlr_scene_buffer *buffer,
										 double *sx, double *sy) {
	return false;
}

static bool switcher_candidate(Client *c) {
	if (c->iskilling || c->isminimized || c->isunglobal || c->is_logic_hide ||
		!client_surface(c) || !client_surface(c)->mapped ||
		client_is_unmanaged(c) || client_is_x11_popup(c))
		return false;
	return (int32_t)c->tags > 0;
}

static void switcher_content_size(Client *c, float *w, float *h) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_surface *s = client_surface(c);
		*w = s->current.width;
		*h = s->current.height;
		return;
	}
#endif
	*w = c->surface.xdg->geometry.width;
	*h = c->surface.xdg->geometry.height;
}

// scale the captured snapshot nodes into cw x SW_TILE_H, same math as
// overview_layout_card but on state captured at tile creation
static void switcher_tile_relayout(struct switcher_tile *tile) {
	int32_t w = tile->cw;
	int32_t h = SW_TILE_H;

	if (!tile->content || w <= 0 || tile->src_w <= 0 || tile->src_h <= 0)
		return;

	float scale_x = (float)w / tile->src_w;
	float scale_y = (float)h / tile->src_h;

	struct switcher_tile_buffer *entry;
	wl_list_for_each(entry, &tile->buffers, link) {
		if (entry->is_root) {
			/* source_box is in buffer pixels, convert via the
			 * logical-to-buffer ratio */
			wlr_scene_node_set_position(&entry->buffer->node, 0, 0);
			wlr_scene_buffer_set_dest_size(entry->buffer, w, h);
			struct wlr_fbox src = {
				.x = tile->clip.x * tile->root_ratio_x,
				.y = tile->clip.y * tile->root_ratio_y,
				.width = tile->src_w * tile->root_ratio_x,
				.height = tile->src_h * tile->root_ratio_y,
			};
			wlr_scene_buffer_set_source_box(entry->buffer, &src);
		} else {
			int px = (int)((entry->sx - tile->clip.x) * scale_x);
			int py = (int)((entry->sy - tile->clip.y) * scale_y);
			wlr_scene_node_set_position(&entry->buffer->node, px, py);
			wlr_scene_buffer_set_dest_size(entry->buffer,
										   (int)(entry->lw * scale_x),
										   (int)(entry->lh * scale_y));
		}
	}
}

// keep the captured geometry in sync with the surface it mirrors
static void switcher_tile_buffer_capture(struct switcher_tile_buffer *entry) {
	struct wlr_surface *surface = entry->surface;
	struct switcher_tile *tile = entry->tile;

	entry->lw = surface->current.width;
	entry->lh = surface->current.height;

	if (entry->is_root) {
		tile->root_ratio_x =
			surface->current.width > 0
				? (float)surface->current.buffer_width / surface->current.width
				: 1.0f;
		tile->root_ratio_y = surface->current.height > 0
								 ? (float)surface->current.buffer_height /
									   surface->current.height
								 : 1.0f;
		client_get_clip(tile->c, &tile->clip);
		switcher_content_size(tile->c, &tile->src_w, &tile->src_h);
	}
}

// mirror the newly committed buffer into the tile. the client renders
// through its own visible scene buffer, this only re-references the
// result, so frame pacing is untouched
static void switcher_tile_buffer_commit(struct wl_listener *listener,
										void *data) {
	struct switcher_tile_buffer *entry =
		wl_container_of(listener, entry, commit);
	if (!entry->surface->buffer)
		return;
	wlr_scene_buffer_set_buffer(entry->buffer, &entry->surface->buffer->base);
	switcher_tile_buffer_capture(entry);
	switcher_tile_relayout(entry->tile);
}

static void switcher_tile_buffer_destroy(struct wl_listener *listener,
										 void *data) {
	struct switcher_tile_buffer *entry =
		wl_container_of(listener, entry, destroy);
	wl_list_remove(&entry->commit.link);
	wl_list_remove(&entry->destroy.link);
	wl_list_remove(&entry->link);
	wlr_scene_node_destroy(&entry->buffer->node);
	free(entry);
}

static void switcher_tile_add_surface(struct wlr_surface *surface, int sx,
									  int sy, void *data) {
	struct switcher_tile *tile = data;
	if (!surface->buffer)
		return;

	struct wlr_scene_buffer *buffer =
		wlr_scene_buffer_create(tile->content, &surface->buffer->base);
	if (!buffer)
		return;
	wlr_scene_buffer_set_filter_mode(buffer, WLR_SCALE_FILTER_BILINEAR);
	buffer->point_accepts_input = switcher_buffer_reject_input;

	struct switcher_tile_buffer *entry = ecalloc(1, sizeof(*entry));
	entry->tile = tile;
	entry->surface = surface;
	entry->buffer = buffer;
	entry->sx = sx;
	entry->sy = sy;
	entry->is_root = (surface == client_surface(tile->c));
	switcher_tile_buffer_capture(entry);

	entry->commit.notify = switcher_tile_buffer_commit;
	wl_signal_add(&surface->events.commit, &entry->commit);
	entry->destroy.notify = switcher_tile_buffer_destroy;
	wl_signal_add(&surface->events.destroy, &entry->destroy);

	wl_list_insert(tile->buffers.prev, &entry->link);
}

static struct switcher_tile *switcher_tile_create(Client *c) {
	struct switcher_tile *tile = ecalloc(1, sizeof(*tile));
	tile->c = c;
	wl_list_init(&tile->buffers);
	tile->root_ratio_x = 1.0f;
	tile->root_ratio_y = 1.0f;
	client_get_clip(c, &tile->clip);
	switcher_content_size(c, &tile->src_w, &tile->src_h);
	tile->tree = wlr_scene_tree_create(sw.tree);
	tile->frame = wlr_scene_rect_create(tile->tree, 1, 1, config.bordercolor);
	tile->content = wlr_scene_tree_create(tile->tree);
	wlr_scene_node_set_position(&tile->content->node, SW_PAD, SW_PAD);
	wlr_surface_for_each_surface(client_surface(c), switcher_tile_add_surface,
								 tile);
	return tile;
}

static void switcher_tile_destroy(struct switcher_tile *tile) {
	struct switcher_tile_buffer *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &tile->buffers, link) {
		wl_list_remove(&entry->commit.link);
		wl_list_remove(&entry->destroy.link);
		wl_list_remove(&entry->link);
		free(entry);
	}
	wlr_scene_node_destroy(&tile->tree->node);
	free(tile);
}

// compute tile sizes, wrap into rows, center rows, size the panel and
// position everything
static void switcher_layout(void) {
	int i, r;
	int frame_h = SW_TILE_H + 2 * SW_PAD;
	int maxrow_w = sw.mon->m.width * 4 / 5 - 2 * SW_MARGIN;
	if (maxrow_w < SW_TILE_MAXW + 2 * SW_PAD)
		maxrow_w = SW_TILE_MAXW + 2 * SW_PAD;

	int nrows = 1, row_w = 0, content_w = 0;
	for (i = 0; i < sw.count; i++) {
		struct switcher_tile *tile = sw.tiles[i];
		int w = (tile->src_w > 0 && tile->src_h > 0)
					? (int)(SW_TILE_H * tile->src_w / tile->src_h)
					: SW_TILE_H;
		if (w < SW_TILE_MINW)
			w = SW_TILE_MINW;
		if (w > SW_TILE_MAXW)
			w = SW_TILE_MAXW;
		tile->cw = w;

		int fw = w + 2 * SW_PAD;
		if (row_w > 0 && row_w + SW_GAP + fw > maxrow_w) {
			nrows++;
			row_w = fw;
		} else {
			row_w = row_w == 0 ? fw : row_w + SW_GAP + fw;
		}
		tile->row = nrows - 1;
		if (row_w > content_w)
			content_w = row_w;
	}

	int *rowsw = ecalloc(nrows, sizeof(int));
	for (i = 0; i < sw.count; i++) {
		struct switcher_tile *tile = sw.tiles[i];
		int fw = tile->cw + 2 * SW_PAD;
		rowsw[tile->row] += rowsw[tile->row] == 0 ? fw : SW_GAP + fw;
	}

	sw.panel_w = content_w + 2 * SW_MARGIN;
	sw.panel_h = 2 * SW_MARGIN + nrows * frame_h + (nrows - 1) * SW_GAP;
	sw.x = sw.mon->m.x + (sw.mon->m.width - sw.panel_w) / 2;
	sw.y = sw.mon->m.y + (sw.mon->m.height - sw.panel_h) / 2;
	wlr_scene_node_set_position(&sw.tree->node, sw.x, sw.y);
	wlr_scene_rect_set_size(sw.bg, sw.panel_w, sw.panel_h);

	r = -1;
	int cur_x = 0;
	for (i = 0; i < sw.count; i++) {
		struct switcher_tile *tile = sw.tiles[i];
		if (tile->row != r) {
			r = tile->row;
			cur_x = SW_MARGIN + (content_w - rowsw[r]) / 2;
		}
		wlr_scene_node_set_position(&tile->tree->node, cur_x,
									SW_MARGIN + r * (frame_h + SW_GAP));
		wlr_scene_rect_set_size(tile->frame, tile->cw + 2 * SW_PAD, frame_h);
		switcher_tile_relayout(tile);
		cur_x += tile->cw + 2 * SW_PAD + SW_GAP;
	}
	free(rowsw);
}

static void switcher_apply_highlight(void) {
	int i;
	for (i = 0; i < sw.count; i++)
		wlr_scene_rect_set_color(sw.tiles[i]->frame, i == sw.index
														 ? config.focuscolor
														 : config.bordercolor);
}

static void switcher_close(void) {
	int i;
	if (!sw.active)
		return;
	sw.active = false;
	for (i = 0; i < sw.count; i++)
		switcher_tile_destroy(sw.tiles[i]);
	free(sw.tiles);
	sw.tiles = NULL;
	sw.count = 0;
	sw.index = 0;
	wlr_scene_node_destroy(&sw.tree->node);
	sw.tree = NULL;
	sw.bg = NULL;
	sw.mon = NULL;
}

void switcher_cancel(void) { switcher_close(); }

void switcher_commit(void) {
	if (!sw.active)
		return;
	Client *tc = sw.tiles[sw.index]->c;
	switcher_close();
	if (!tc || tc->iskilling || !client_surface(tc)->mapped)
		return;
	if ((int32_t)tc->tags > 0) {
		// switch the tag on the client's own monitor before focusing.
		// focusclient may bail without moving selmon, for example on a
		// nofocus rule, so view on selmon could retag the wrong monitor
		if (!VISIBLEON(tc, tc->mon))
			view_in_mon(&(Arg){.ui = get_tags_first_tag(tc->tags)}, true,
						tc->mon, true);
		focusclient(tc, 1);
	}
}

// invalidate a dying or hidden client while the switcher is open
void switcher_drop_client(Client *dc) {
	int i, pos = -1;
	if (!sw.active)
		return;
	for (i = 0; i < sw.count; i++) {
		if (sw.tiles[i]->c == dc) {
			pos = i;
			break;
		}
	}
	if (pos < 0)
		return;
	switcher_tile_destroy(sw.tiles[pos]);
	for (i = pos; i < sw.count - 1; i++)
		sw.tiles[i] = sw.tiles[i + 1];
	sw.count--;
	if (sw.count == 0) {
		switcher_close();
		return;
	}
	if (pos < sw.index)
		sw.index--;
	if (sw.index >= sw.count)
		sw.index = 0;
	switcher_layout();
	switcher_apply_highlight();
}

static void switcher_open(int dir) {
	Client *c;
	Monitor *m;
	int n = 0;

	wl_list_for_each(m, &mons, link) {
		if (m->isoverview)
			return;
	}

	wl_list_for_each(c, &fstack, flink) {
		if (switcher_candidate(c))
			n++;
	}
	if (n == 0)
		return;

	// keep only mods a key release can clear, a latched caps or num
	// lock bit would otherwise block the commit forever
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(seat);
	sw.open_mods = kb ? (wlr_keyboard_get_modifiers(kb) &
						 (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL |
						  WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO))
					  : 0;

	sw.mon = selmon;
	sw.tree = wlr_scene_tree_create(layers[LyrOverlay]);
	sw.bg = wlr_scene_rect_create(sw.tree, 1, 1, switcher_panel_color);
	sw.tiles = ecalloc(n, sizeof(*sw.tiles));
	sw.count = 0;
	wl_list_for_each(c, &fstack, flink) {
		if (switcher_candidate(c))
			sw.tiles[sw.count++] = switcher_tile_create(c);
	}
	sw.index = n > 1 ? (dir > 0 ? 1 : n - 1) : 0;
	sw.active = true;

	switcher_layout();
	switcher_apply_highlight();
}

static void switcher_cycle(int dir) {
	sw.index = (sw.index + dir + sw.count) % sw.count;
	switcher_apply_highlight();
}

void switcher(const Arg *arg) {
	int dir = (arg && arg->i < 0) ? -1 : 1;
	if (arg && arg->i == SWITCHER_COMMIT) {
		switcher_commit();
		return;
	}
	if (arg && arg->i == SWITCHER_CANCEL) {
		switcher_cancel();
		return;
	}
	if (locked || !selmon || selmon->is_jump_mode)
		return;
	if (!sw.active)
		switcher_open(dir);
	else
		switcher_cycle(dir);
}
