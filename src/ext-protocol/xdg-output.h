/* 自定义 xdg-output：xwayland_ignore_scale 时给 XWayland 发物理坐标/尺寸 */
#include <math.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "xdg-output-unstable-v1-protocol.h"

#define MANGO_XDG_OUTPUT_MANAGER_VERSION 3
#define MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION 3
#define MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

/* 一个客户端绑定到的 zxdg_output_v1 资源 */
struct MangoXDGOutputResource {
	struct wl_resource *resource;
	struct wl_list link; /* xdg_output->resources */
};

/* 一个 wlr_output 对应的 xdg-output 状态 */
struct MangoXDGOutput {
	struct wl_list link; /* xdg_outputs */
	struct wlr_output *wlr_output;
	struct wl_list resources; /* MangoXDGOutputResource.link */
	struct {
		struct wl_listener description;
	};
};

static struct wl_global *xdg_output_global;
static struct wl_list xdg_outputs;

static const struct zxdg_output_v1_interface xdg_output_impl;
static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl;

/* XWayland 的 X server 也是一个 wayland 客户端。每次发送时动态判断，
 * 避免在资源创建时缓存导致 xwayland 重启/初始化时序问题 */
static bool xdg_output_resource_is_xwayland(struct wl_resource *resource) {
#ifdef XWAYLAND
	if (xwayland && xwayland->server &&
		xwayland->server->client == wl_resource_get_client(resource))
		return true;
#endif
	return false;
}

/* 从当前输出状态计算逻辑值与物理(含旋转)值 */
static void xdg_output_get_values(struct MangoXDGOutput *output, int32_t *lx,
								  int32_t *ly, int32_t *lw, int32_t *lh,
								  int32_t *px, int32_t *py, int32_t *pw,
								  int32_t *ph) {
	int32_t x = 0, y = 0, w = 0, h = 0;
	Monitor *m = output->wlr_output->data;
	if (m) {
		x = m->m.x;
		y = m->m.y;
		w = m->m.width;
		h = m->m.height;
	}

	/* 物理尺寸必须考虑输出旋转，否则旋转 90/270 时宽高未交换 */
	int32_t tw, th;
	wlr_output_transformed_resolution(output->wlr_output, &tw, &th);
	float scale =
		output->wlr_output->scale > 0.f ? output->wlr_output->scale : 1.f;

	*lx = x;
	*ly = y;
	*lw = w;
	*lh = h;
	*px = (int32_t)roundf(x * scale);
	*py = (int32_t)roundf(y * scale);
	*pw = tw;
	*ph = th;
}

static void xdg_output_send_details(struct MangoXDGOutput *output,
									struct wl_resource *resource) {
	int32_t lx, ly, lw, lh, px, py, pw, ph;
	xdg_output_get_values(output, &lx, &ly, &lw, &lh, &px, &py, &pw, &ph);

	int32_t x, y, w, h;
	if (xdg_output_resource_is_xwayland(resource) &&
		config.xwayland_ignore_scale) {
		/* 欺骗 XWayland：mango 把 X11 窗口放在 逻辑×scale 的物理坐标空间，
		 * 因此告诉 XWayland 屏幕原点在 逻辑×scale、矩形为物理(含旋转)尺寸 */
		x = px;
		y = py;
		w = pw;
		h = ph;
	} else {
		/* xwayland_ignore_scale=0：与普通客户端一样用逻辑坐标 */
		x = lx;
		y = ly;
		w = lw;
		h = lh;
	}

	zxdg_output_v1_send_logical_position(resource, x, y);
	zxdg_output_v1_send_logical_size(resource, w, h);

	if (wl_resource_get_version(resource) <
		MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		zxdg_output_v1_send_done(resource);
}

/* 无条件向该输出的所有资源重发详情。
 * 注意：不要改成"仅值变化时才发"。mango 的 XWayland 坐标模型依赖每次
 * 布局/配置变化后都收到 position/size + wl_output.done，即使数值未变，
 * 跳过重发会让 XWayland 的几何与 mango 实际摆放脱节（坐标混乱、无法点击）。 */
static void xdg_output_update(struct MangoXDGOutput *output) {
	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link)
		xdg_output_send_details(output, res->resource);
}

static void xdg_output_resource_handle_destroy(struct wl_resource *resource) {
	struct MangoXDGOutputResource *res = wl_resource_get_user_data(resource);
	if (!res)
		return;
	wl_list_remove(&res->link);
	free(res);
}

static void xdg_output_handle_destroy(struct wl_client *client,
									  struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void xdg_output_manager_handle_destroy(struct wl_client *client,
											  struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

/* 输出描述变化（version>=3 的客户端需要重发） */
static void xdg_output_handle_description(struct wl_listener *listener,
										  void *data) {
	struct MangoXDGOutput *output =
		wl_container_of(listener, output, description);
	struct wlr_output *wlr_output = output->wlr_output;
	if (!wlr_output->description)
		return;

	struct MangoXDGOutputResource *res;
	wl_list_for_each(res, &output->resources, link) {
		if (wl_resource_get_version(res->resource) >=
			MANGO_XDG_OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION)
			zxdg_output_v1_send_description(res->resource,
											wlr_output->description);
	}
}

static struct MangoXDGOutput *xdg_output_find(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output;
	wl_list_for_each(output, &xdg_outputs, link) {
		if (output->wlr_output == wlr_output)
			return output;
	}
	return NULL;
}

static struct MangoXDGOutput *xdg_output_create(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = calloc(1, sizeof(*output));
	if (!output)
		return NULL;
	output->wlr_output = wlr_output;
	wl_list_init(&output->resources);
	output->description.notify = xdg_output_handle_description;
	wl_signal_add(&wlr_output->events.description, &output->description);
	wl_list_insert(&xdg_outputs, &output->link);
	return output;
}

static void xdg_output_destroy(struct MangoXDGOutput *output) {
	/* 惰性化所有资源：zxdg_output_v1 由客户端创建，只有客户端能销毁。
	 * 直接销毁会让 libwayland 为该 id 发 delete_id，客户端在自身 destroy
	 * 请求仍在途时复用该 id，下一个请求会命中未知对象。 */
	struct MangoXDGOutputResource *res, *tmp;
	wl_list_for_each_safe(res, tmp, &output->resources, link) {
		wl_resource_set_user_data(res->resource, NULL);
		wl_list_remove(&res->link);
		free(res);
	}
	wl_list_remove(&output->description.link);
	wl_list_remove(&output->link);
	free(output);
}

static void xdg_output_manager_handle_get_xdg_output(
	struct wl_client *client, struct wl_resource *manager_resource, uint32_t id,
	struct wl_resource *output_resource) {
	struct wlr_output *wlr_output = wlr_output_from_resource(output_resource);

	struct wl_resource *resource =
		wl_resource_create(client, &zxdg_output_v1_interface,
						   wl_resource_get_version(manager_resource), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	/* 无效输出：保持惰性，不发送任何事件 */
	if (!wlr_output) {
		wl_resource_set_implementation(resource, &xdg_output_impl, NULL,
									   xdg_output_resource_handle_destroy);
		return;
	}

	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (!output)
		output = xdg_output_create(wlr_output);
	if (!output) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}

	struct MangoXDGOutputResource *res = calloc(1, sizeof(*res));
	if (!res) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}
	res->resource = resource;
	wl_list_insert(&output->resources, &res->link);
	wl_resource_set_implementation(resource, &xdg_output_impl, res,
								   xdg_output_resource_handle_destroy);

	uint32_t version = wl_resource_get_version(resource);
	if (version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION && wlr_output->name)
		zxdg_output_v1_send_name(resource, wlr_output->name);
	if (version >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION &&
		wlr_output->description)
		zxdg_output_v1_send_description(resource, wlr_output->description);

	xdg_output_send_details(output, resource);

	if (wl_resource_get_version(output_resource) >=
			WL_OUTPUT_DONE_SINCE_VERSION &&
		version >= MANGO_XDG_OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
		wl_output_send_done(output_resource);
}

static void xdg_output_manager_bind(struct wl_client *client, void *data,
									uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(
		client, &zxdg_output_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &xdg_output_manager_impl, NULL,
								   NULL);
}

static const struct zxdg_output_v1_interface xdg_output_impl = {
	.destroy = xdg_output_handle_destroy,
};

static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl = {
	.destroy = xdg_output_manager_handle_destroy,
	.get_xdg_output = xdg_output_manager_handle_get_xdg_output,
};

/* 更新所有输出的 xdg-output 详情，并为每个输出调度一次 wl_output.done。
 * done 是客户端(尤其 XWayland)应用 wl_output/xdg-output 变更的事务边界：
 * wlroots 只在 mode/scale/geometry 变化时自行调度，纯布局移动时需要这里补上，
 * 因此这里始终调度，不能只依赖"值变化"。 */
static void xdg_output_update_all(void) {
	struct MangoXDGOutput *output, *tmp;
	wl_list_for_each_safe(output, tmp, &xdg_outputs, link) {
		xdg_output_update(output);
		wlr_output_schedule_done(output->wlr_output);
	}
}

/* 输出被移除时，让对应的 xdg-output 资源变为惰性，而不是销毁它。
 * 该函数在 cleanupmon()（wlr_output destroy 监听器）中调用，保证幂等。 */
static void xdg_output_cleanup_output(struct wlr_output *wlr_output) {
	struct MangoXDGOutput *output = xdg_output_find(wlr_output);
	if (output)
		xdg_output_destroy(output);
}

void xdg_output_init(void) {
	wl_list_init(&xdg_outputs);
	xdg_output_global = wl_global_create(dpy, &zxdg_output_manager_v1_interface,
										 MANGO_XDG_OUTPUT_MANAGER_VERSION, NULL,
										 xdg_output_manager_bind);
	if (!xdg_output_global)
		wlr_log(WLR_ERROR, "failed to create zxdg_output_manager_v1 global");
}
