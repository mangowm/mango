#ifndef MANGO_TEXT_H
#define MANGO_TEXT_H

#include <wlr/types/wlr_scene.h>

struct mango_text_node {
    struct wlr_scene_buffer *scene_buffer;
    int logical_width;
    int logical_height;
};

/**
 * 创建一个独立的文字场景节点
 * @param parent 父级场景树节点（例如窗口装饰树、OSD层树或状态栏树）
 */
struct mango_text_node *mango_text_node_create(struct wlr_scene_tree *parent);

/**
 * 更新节点中的文字内容、字体、颜色和缩放
 * @param node 文字节点指针
 * @param text 要显示的文本内容
 * @param font_desc Pango 字体描述字符串 (例如 "Sans 12" 或 "JetBrains Mono Bold 14")
 * @param color RGBA 颜色数组，范围 0.0 ~ 1.0
 * @param scale 当前输出设备的缩放比例 (HiDPI 支持)
 */
void mango_text_node_update(struct mango_text_node *node, const char *text,
                            const char *font_desc, float color[4], float scale);

/**
 * 销毁文字节点并释放相关场景图资源
 */
void mango_text_node_destroy(struct mango_text_node *node);

#endif /* MANGO_TEXT_H */