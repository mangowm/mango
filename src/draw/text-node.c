#include "text-node.h"
#include <stdlib.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wayland-server-core.h>
#include <drm_fourcc.h>

// 自定义 wlr_buffer，用于将 Cairo Surface 包装并注入 wlr_scene
struct mango_text_buffer {
    struct wlr_buffer base;
    cairo_surface_t *surface;
};

static void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
    struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
    cairo_surface_destroy(buf->surface);
    free(buf);
}

static bool text_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
                                              uint32_t flags,
                                              void **data,
                                              uint32_t *format,
                                              size_t *stride) {
    (void)flags; // 该参数当前无需使用
    struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
    *data = cairo_image_surface_get_data(buf->surface);
    *format = DRM_FORMAT_ARGB8888;
    *stride = cairo_image_surface_get_stride(buf->surface);
    return true;
}

static void text_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
    // Cairo 表面数据在 flush 后稳定，此处无需额外操作
}

static const struct wlr_buffer_impl text_buffer_impl = {
    .destroy = text_buffer_destroy,
    .begin_data_ptr_access = text_buffer_begin_data_ptr_access,
    .end_data_ptr_access = text_buffer_end_data_ptr_access,
};

struct mango_text_node *mango_text_node_create(struct wlr_scene_tree *parent) {
    struct mango_text_node *node = calloc(1, sizeof(*node));
    if (!node) return NULL;

    node->scene_buffer = wlr_scene_buffer_create(parent, NULL);
    if (!node->scene_buffer) {
        free(node);
        return NULL;
    }
    return node;
}

void mango_text_node_update(struct mango_text_node *node, const char *text,
                            const char *font_desc, float color[4], float scale) {
    if (!node || !text || !font_desc) return;
    if (scale <= 0.0f) scale = 1.0f;

    // 测量文字像素尺寸
    cairo_surface_t *dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *dummy_cr = cairo_create(dummy_surface);
    PangoContext *dummy_ctx = pango_cairo_create_context(dummy_cr);
    pango_cairo_context_set_resolution(dummy_ctx, 96.0 * scale);
    PangoLayout *dummy_layout = pango_layout_new(dummy_ctx);
    PangoFontDescription *desc = pango_font_description_from_string(font_desc);
    pango_layout_set_font_description(dummy_layout, desc);
    pango_layout_set_text(dummy_layout, text, -1);

    int pixel_width, pixel_height;
    pango_layout_get_pixel_size(dummy_layout, &pixel_width, &pixel_height);

    g_object_unref(dummy_layout);
    pango_font_description_free(desc);
    g_object_unref(dummy_ctx);
    cairo_destroy(dummy_cr);
    cairo_surface_destroy(dummy_surface);

    if (pixel_width <= 0 || pixel_height <= 0) {
        wlr_scene_buffer_set_buffer(node->scene_buffer, NULL);
        node->logical_width = 0;
        node->logical_height = 0;
        return;
    }

    // 创建真实大小的 Surface 并绘制文字
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pixel_width, pixel_height);
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    PangoContext *ctx = pango_cairo_create_context(cr);
    pango_cairo_context_set_resolution(ctx, 96.0 * scale);
    PangoLayout *layout = pango_layout_new(ctx);
    desc = pango_font_description_from_string(font_desc);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text, -1);

    cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
    pango_cairo_show_layout(cr, layout);
    cairo_surface_flush(surface);

    // 包装成 wlr_buffer
    struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
    if (!buf) {
        cairo_surface_destroy(surface);
        cairo_destroy(cr);
        g_object_unref(layout);
        pango_font_description_free(desc);
        g_object_unref(ctx);
        return;
    }
    wlr_buffer_init(&buf->base, &text_buffer_impl, pixel_width, pixel_height);
    buf->surface = surface;

    // 提交给场景图
    wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);
    wlr_buffer_drop(&buf->base); // 函数内引用计数 -1，交给场景图管理

    // 设置逻辑大小，实现 HiDPI 缩放
    node->logical_width = (int)(pixel_width / scale);
    node->logical_height = (int)(pixel_height / scale);
    wlr_scene_buffer_set_dest_size(node->scene_buffer, node->logical_width, node->logical_height);

    // 清理绘制资源
    g_object_unref(layout);
    pango_font_description_free(desc);
    g_object_unref(ctx);
    cairo_destroy(cr);
}

void mango_text_node_destroy(struct mango_text_node *node) {
    if (!node) return;
    wlr_scene_node_destroy(&node->scene_buffer->node);
    free(node);
}