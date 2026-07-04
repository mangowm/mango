#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/util/box.h>

typedef struct Monitor Monitor;
typedef struct Client Client;

extern int32_t enablegaps;
extern struct wl_list clients;
extern void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact);
extern void resize(Client *c, struct wlr_box geo, int32_t interact);
extern Config config;
extern struct wlr_cursor *cursor;

struct Infinite_layout_data {
	int32_t center_x, center_y;
	struct wl_list nodes;
	bool isRigid;
} static infinite_data;

struct Infinite_node {
	struct wl_list link;
	Client *client;
	// here maybe lx and ly
};

static struct Infinite_node *find_node(Client *c) {
	struct Infinite_node *n;
	wl_list_for_each(n, &infinite_data.nodes, link) {
		if (n->client == c)
			return n;
	};

	return NULL;
}

static void init_infinite_layout_data(Monitor *m) {
	infinite_data.center_x = m->w.x + m->w.width / 2;
	infinite_data.center_y = m->w.y + m->w.height / 2;

	wl_list_init(&infinite_data.nodes);
}

static bool check_colision(float nx, float ny, Client *self, int32_t gap,
						   Monitor *m) {
	Client *other;
	wl_list_for_each(other, &clients, link) {
		if (other == self || !VISIBLEON(other, m) || !ISFAKETILED(other))
			continue;
		int32_t g2 = gap / 2;
		int32_t ax = (int32_t)roundf(nx);
		int32_t ay = (int32_t)roundf(ny);
		int32_t aw = self->geom.width;
		int32_t ah = self->geom.height;
		int32_t bx = other->geom.x;
		int32_t by = other->geom.y;
		int32_t bw = other->geom.width;
		int32_t bh = other->geom.height;
		if (!(ax + aw + g2 <= bx - g2 || bx + bw + g2 <= ax - g2 ||
			  ay + ah + g2 <= by - g2 || by + bh + g2 <= ay - g2))
			return true;
	}
	return false;
}

static struct wlr_box calculate_position(Monitor *m, Client *c, int32_t gap) {
	// Start from the window's current position
	float px = c->geom.x;
	float py = c->geom.y;

	// If current position already overlaps, push outward (away from center)
	// until clear
	if (check_colision(px, py, c, gap, m)) {
		float ox = px - infinite_data.center_x;
		float oy = py - infinite_data.center_y;
		float olen = sqrtf(ox * ox + oy * oy);
		if (olen > 0.5f) {
			ox /= olen;
			oy /= olen;
			for (int s = 0; s < 2000; s++) {
				px += ox;
				py += oy;
				if (!check_colision(px, py, c, gap, m))
					break;
			}
		}
	}

	// Now slide toward center
	float dx = infinite_data.center_x - (px + c->geom.width / 2.0f);
	float dy = infinite_data.center_y - (py + c->geom.height / 2.0f);
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 1.0f) {
		return (struct wlr_box){
			.x = (int32_t)roundf(px),
			.y = (int32_t)roundf(py),
			.width = c->geom.width,
			.height = c->geom.height,
		};
	}

	dx /= len;
	dy /= len; // unit vector toward center

	while (len > 0.0f) {
		float nx = px + dx;
		float ny = py + dy;
		if (!check_colision(nx, ny, c, gap, m)) {
			px = nx;
			py = ny;
			len -= 1.0f;
		} else {
			break;
		}
	}

	return (struct wlr_box){
		.x = (int32_t)roundf(px),
		.y = (int32_t)roundf(py),
		.width = c->geom.width,
		.height = c->geom.height,
	};
}

static void set_initial_positon(Monitor *m, Client *c) {
	int32_t gap = enablegaps ? m->gappiv : 0;

	struct wlr_box pos = calculate_position(m, c, gap);

	c->geom.x = pos.x;
	c->geom.y = pos.y;
}

static void move_canvas(Monitor *m, int32_t dx, int32_t dy) {
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (infinite_data.isRigid) {
			if (!VISIBLEON(c, m) || !ISFAKETILED(c))
				continue;
		} else { // continue on base of isRigid
			if (!VISIBLEON(c, m))
				continue;
		}

		c->geom.x += dx;
		c->geom.y += dy; // move the windows in the oposite direction of the
						 // scroll in the canvas
		resize(c, c->geom, 0);
	}
}

void center_focused(Monitor *m, Client *c) {
	int32_t cx = m->w.x + m->w.width / 2;
	int32_t cy = m->w.y + m->w.height / 2;
	int32_t dx = cx - (c->geom.x + c->geom.width / 2);
	int32_t dy = cy - (c->geom.y + c->geom.height / 2);
	move_canvas(m, dx, dy);
}

void home_canvas(Monitor *m) {
	Client *c;
	int32_t cx = 0, cy = 0, i = 0;
	wl_list_for_each(c, &clients, link) {
		if (infinite_data.isRigid) {
			if (!VISIBLEON(c, m) || !ISFAKETILED(c))
				continue;
		} else { // continue on base of isRigid
			if (!VISIBLEON(c, m))
				continue;
		}

		cx += c->geom.x + c->geom.width / 2;
		cy += c->geom.y + c->geom.height / 2;
		i++;
	}
	if (!i)
		return;
	int32_t dx = infinite_data.center_x - cx / i;
	int32_t dy = infinite_data.center_y - cy / i;
	move_canvas(m, dx, dy);
}

int32_t home_canvas_wrapper(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m)
		home_canvas(m);
	return 0;
}

int32_t center_focused_wrapper(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m && m->sel)
		center_focused(m, m->sel);
	return 0;
}

int32_t scroll_left(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m)
		move_canvas(m, -120, 0);
	return 0;
}
int32_t scroll_right(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m)
		move_canvas(m, 120, 0);
	return 0;
}
int32_t scroll_up(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m)
		move_canvas(m, 0, -120);
	return 0;
}
int32_t scroll_down(const Arg *arg) {
	Monitor *m = xytomon(cursor->x, cursor->y);
	if (m)
		move_canvas(m, 0, 120);
	return 0;
}

void main_infinite(Monitor *m) {
	int32_t i, n = 0;
	Client *c = NULL;

	init_infinite_layout_data(m);

	// --- Map all the windows to infinite_node ---
	wl_list_for_each(c, &clients, link) {
		struct Infinite_node *node = find_node(c);
		if (!node) {
			node = calloc(1, sizeof(*node));
			node->client = c;
			wl_list_insert(&infinite_data.nodes, &node->link);

			// Scale new window geometry by user-configurable factor
			float s = config.new_window_scale;
			if (fabsf(s - 1.0f) > 0.001f) {
				c->geom.width = (int32_t)roundf(c->geom.width * s);
				c->geom.height = (int32_t)roundf(c->geom.height * s);
				if (c->geom.width < 1)
					c->geom.width = 1;
				if (c->geom.height < 1)
					c->geom.height = 1;
			}
		}
	};

	if (infinite_data.isRigid) {
		n = m->visible_fake_tiling_clients;
		if (n == 0)
			return;

		int32_t gap = enablegaps ? m->gappiv : 0;

		// --- Gaps ---
		if (config.smartgaps && n == 1) {
			gap = 0;
		}

		i = 0;
		wl_list_for_each(c, &clients, link) {
			if (!VISIBLEON(c, m) || !ISFAKETILED(c))
				continue;

			if (c->geom.width == 0) {
				set_initial_positon(m, c);
			}

			i++;
		}

		// Build array of visible clients sorted by distance to center (closest
		// first) so collision checks see already-placed windows' final
		// positions
		Client **order = calloc(i, sizeof(Client *));
		int32_t idx = 0;
		wl_list_for_each(c, &clients, link) {
			if (!VISIBLEON(c, m) || !ISFAKETILED(c))
				continue;
			order[idx++] = c;
		}
		for (int32_t a = 0; a < i - 1; a++) {
			for (int32_t b = a + 1; b < i; b++) {
				float da =
					hypotf(order[a]->geom.x + order[a]->geom.width / 2.0f -
							   infinite_data.center_x,
						   order[a]->geom.y + order[a]->geom.height / 2.0f -
							   infinite_data.center_y);
				float db =
					hypotf(order[b]->geom.x + order[b]->geom.width / 2.0f -
							   infinite_data.center_x,
						   order[b]->geom.y + order[b]->geom.height / 2.0f -
							   infinite_data.center_y);
				if (db < da) {
					Client *t = order[a];
					order[a] = order[b];
					order[b] = t;
				}
			}
		}
		// Poll until stable (no window moved) or max passes
		bool moved;
		int pass = 0;
		do {
			moved = false;
			for (int32_t a = 0; a < i; a++) {
				struct wlr_box pos = calculate_position(m, order[a], gap);
				if (pos.x != order[a]->geom.x || pos.y != order[a]->geom.y)
					moved = true;
				client_tile_resize(order[a], pos, 0);
			}
			pass++;
		} while (moved && pass < 100);
		free(order);
	} else {
		wl_list_for_each(c, &clients, link) {
			if (!VISIBLEON(c, m)) // even if the client is floating
				continue;

			resize(
				c, c->geom,
				0); // the geometry has been aplied at the start of the function
		}
	}

	// --- free the nodes ---
	struct Infinite_node *_n, *tmp;
	wl_list_for_each_safe(_n, tmp, &infinite_data.nodes, link) {
		bool alive = false;
		wl_list_for_each(c, &clients, link) {
			if (c == _n->client && VISIBLEON(c, m) /* && ISFAKETILED(c) */)
				alive = true;
		}
		if (!alive) {
			wl_list_remove(&_n->link);
			free(_n);
		}
	};
}

void infinite(Monitor *m) {
	infinite_data.isRigid = true;
	main_infinite(m);
}

void free_infinite(Monitor *m) {
	infinite_data.isRigid = false;
	main_infinite(m);
}
