static void tile(Monitor *m);
static void center_tile(Monitor *m);
static void right_tile(Monitor *m);
static void overview(Monitor *m);
static void grid(Monitor *m);
static void scroller(Monitor *m);
static void deck(Monitor *mon);
static void monocle(Monitor *m);
static void vertical_tile(Monitor *m);
static void vertical_overview(Monitor *m);
static void vertical_grid(Monitor *m);
static void vertical_scroller(Monitor *m);
static void vertical_deck(Monitor *mon);
static void tgmix(Monitor *m);

/* layout(s) */
Layout overviewlayout = {"O", overview, "overview"};

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
	TGMIX,
};

Layout layouts[] = {
	// 最少两个,不能删除少于两个
	/* symbol     arrange function   name */
	{"T", tile, "tile", TILE},						 // 平铺布局
	{"S", scroller, "scroller", SCROLLER},			 // 滚动布局
	{"G", grid, "grid", GRID},						 // 格子布局
	{"M", monocle, "monocle", MONOCLE},				 // 单屏布局
	{"K", deck, "deck", DECK},						 // 卡片布局
	{"CT", center_tile, "center_tile", CENTER_TILE}, // 居中布局
	{"RT", right_tile, "right_tile", RIGHT_TILE},	 // 右布局
	{"VS", vertical_scroller, "vertical_scroller",
	 VERTICAL_SCROLLER},								   // 垂直滚动布局
	{"VT", vertical_tile, "vertical_tile", VERTICAL_TILE}, // 垂直平铺布局
	{"VG", vertical_grid, "vertical_grid", VERTICAL_GRID}, // 垂直格子布局
	{"VK", vertical_deck, "vertical_deck", VERTICAL_DECK}, // 垂直卡片布局
	{"TG", tgmix, "tgmix", TGMIX},						   // 混合布局
};
