#ifndef __LAYOUT_LAYOUT_H__
#define __LAYOUT_LAYOUT_H__ 1

#include "mango/common/types.h"
#include "mango/layout/dwindle.h"
#include "mango/layout/horizontal.h"
#include "mango/layout/overview.h"
#include "mango/layout/scroll.h"
#include "mango/layout/vertical.h"
#include <stdbool.h>
#include <stdint.h>
#include <wlr/util/box.h>

struct LayoutContext {
	Client *probe;
	struct wlr_box *out;
};

static inline bool
layout_predict_box(void (*core)(Monitor *m, const struct LayoutContext *ctx),
				   Monitor *m, Client *c, struct wlr_box *out) {
	struct wlr_box geo = {0};
	struct LayoutContext ctx = {.probe = c, .out = &geo};
	core(m, &ctx);
	*out = geo;
	return geo.width > 0 && geo.height > 0;
}

/* layout(s) */
typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
	bool (*predict)(Monitor *m, Client *c, struct wlr_box *out);
	const char *name;
	uint32_t id;
} Layout;

extern Layout overviewlayout;

enum {
	TILE,
	SCROLLER,
	GRID,
	MONOCLE,
	DECK,
	CENTER_TILE,
	VERTICAL_SCROLLER,
	VERTICAL_TILE,
	VERTICAL_GRID,
	VERTICAL_DECK,
	RIGHT_TILE,
	DWINDLE,
	FAIR,
	VERTICAL_FAIR,
};

extern Layout layouts[14];

#endif
