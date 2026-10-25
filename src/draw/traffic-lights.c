#include "mango/draw/traffic-lights.h"
#include "mango/common/server.h"
#include "mango/config/parse.h"
#include "mango/dispatch/bind.h"
#include "mango/draw/dim-node.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"

#define TL_COUNT 3

/* close, minimize, maximize */
static const float tl_colors[TL_COUNT][4] = {
	{0xFF / 255.0f, 0x5F / 255.0f, 0x57 / 255.0f, 1.0f},
	{0xFE / 255.0f, 0xBC / 255.0f, 0x2E / 255.0f, 1.0f},
	{0x28 / 255.0f, 0xC8 / 255.0f, 0x40 / 255.0f, 1.0f},
};

/* gap and margin follow the dot size */
static int32_t tl_gap(void) { return config.traffic_light_size * 2 / 3; }
static int32_t tl_margin(void) { return config.traffic_light_size * 2 / 3; }

void client_add_traffic_lights(Client *c) {
	for (int32_t i = 0; i < TL_COUNT; i++) {
		c->traffic_light[i] =
			mango_dim_node_create(c->scene_surface, tl_colors[i]);
		if (c->traffic_light[i])
			mango_dim_node_set_enabled(c->traffic_light[i], false);
	}
}

void client_remove_traffic_lights(Client *c) {
	for (int32_t i = 0; i < TL_COUNT; i++) {
		if (c->traffic_light[i]) {
			mango_dim_node_destroy(c->traffic_light[i]);
			c->traffic_light[i] = NULL;
		}
	}
}

/* Position of an action in the configured order, or -1 when it is hidden. */
static int32_t tl_slot(int32_t action) {
	for (int32_t s = 0; s < config.traffic_light_count; s++)
		if (config.traffic_light_order[s] == action)
			return s;
	return -1;
}

/* Left edge of slot s inside a surface of width sw. */
static int32_t tl_x(int32_t sw, int32_t slot) {
	int32_t size = config.traffic_light_size;
	int32_t n = config.traffic_light_count;
	int32_t total = n * size + (n - 1) * tl_gap();
	int32_t base =
		config.traffic_light_right ? sw - tl_margin() - total : tl_margin();
	return base + slot * (size + tl_gap());
}

/* Dots are for the main window of an app, not for dialogs. */
static bool tl_visible(Client *c) {
	int32_t sw, total;

	if (!c || !c->mon || !config.traffic_lights || !c->isfloating)
		return false;
	if (c->isfullscreen || c->iskilling || c->isminimized ||
		c->mon->isoverview || !VISIBLEON(c, c->mon))
		return false;
	if (client_is_float_type(c) || client_get_parent(c))
		return false;

	sw = c->geom.width - 2 * (int32_t)c->bw;
	total = config.traffic_light_count * config.traffic_light_size +
			(config.traffic_light_count - 1) * tl_gap();
	return sw > total + 2 * tl_margin();
}

/* Top-left corner of dot i, relative to the window surface. */
static bool tl_origin(Client *c, int32_t i, int32_t *x, int32_t *y) {
	int32_t slot = tl_slot(i);
	if (slot < 0)
		return false;
	*x = tl_x(c->animation.current.width - 2 * (int32_t)c->bw, slot);
	*y = tl_margin();
	return true;
}

void client_draw_traffic_lights(Client *c) {
	bool show;

	if (!c || !c->traffic_light[0])
		return;

	show = tl_visible(c);
	for (int32_t i = 0; i < TL_COUNT; i++) {
		int32_t x, y;

		if (!c->traffic_light[i])
			continue;
		if (!show) {
			mango_dim_node_set_enabled(c->traffic_light[i], false);
			continue;
		}
		if (!tl_origin(c, i, &x, &y)) {
			mango_dim_node_set_enabled(c->traffic_light[i], false);
			continue;
		}
		mango_dim_node_set_radius(c->traffic_light[i],
								  config.traffic_light_size / 2);
		mango_dim_node_set_box(c->traffic_light[i], x, y,
							   config.traffic_light_size,
							   config.traffic_light_size);
		mango_dim_node_set_enabled(c->traffic_light[i], true);
	}
}

bool traffic_lights_click(Client *c, double px, double py) {
	/* FORCE is 0, so say UNFORCE: the red dot asks the window to close. */
	Arg arg = {.tc = c, .i = UNFORCE};

	if (!tl_visible(c))
		return false;

	for (int32_t i = 0; i < TL_COUNT; i++) {
		int32_t size = config.traffic_light_size;
		int32_t x, y, slot = tl_slot(i);
		double cx, cy, r = size / 2.0 + 2.0; /* a little slack */

		if (slot < 0)
			continue;
		/* Same placement as the drawing, but from the final window box. */
		x = tl_x(c->geom.width - 2 * (int32_t)c->bw, slot);
		y = tl_margin();

		cx = c->geom.x + (int32_t)c->bw + x + size / 2.0;
		cy = c->geom.y + (int32_t)c->bw + y + size / 2.0;
		if ((px - cx) * (px - cx) + (py - cy) * (py - cy) > r * r)
			continue;

		if (i == 0)
			kill_client(&arg);
		else if (i == 1)
			minimize_window(&arg);
		else
			toggle_maximize_screen(&arg);
		return true;
	}
	return false;
}
