#ifndef __LAYOUT_LAYOUT_H__
#define __LAYOUT_LAYOUT_H__ 1

#include "horizontal.h"
#include "vertical.h"
#include "scroll.h"
#include "dwindle.h"
#include "overview.h"
#include "../mango.h"

/* layout(s) */
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
