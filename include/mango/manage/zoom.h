#ifndef __MANAGE_ZOOM_H__
#define __MANAGE_ZOOM_H__ 1

#include "mango/common/types.h"

#include <stdbool.h>

/*
 * Screen magnifier.
 *
 * The scene always renders at the output resolution. While the magnifier is
 * active one extra pass crops that frame around the pointer and scales it back
 * up, so the composited image is the only thing that changes: clients keep
 * rendering normal-resolution buffers and never learn about the zoom.
 *
 * One level is shared by every output; the output under the pointer is the one
 * that is drawn magnified. Unrelated to the `zoom` dispatcher, which swaps the
 * focused window with master.
 */

/*
 * Whether `m` has to build a frame even though nothing in the scene changed: a
 * step in progress, a crop that still needs a buffer, or a crop on screen that
 * needs a frame to retire it. Moving the pointer damages nothing, so without
 * this the magnified output would keep showing the last crop.
 */
bool screen_zoom_needs_frame(const Monitor *m);

/*
 * Crop this frame to the magnifier if `m` is the output it draws: replaces the
 * buffer `state` carries with the magnified one, marks the whole frame damaged
 * and keeps it from being torn. A frame it cannot magnify is committed as it
 * is. Also asks for the next frame while a step runs.
 */
void screen_zoom_compose(Monitor *m, struct wlr_output_state *state);

/* Move the level by `delta` (negative zooms out), clamped to [1, max]. */
void screen_zoom_by(float delta);

/* Animate back to 1x. */
void screen_zoom_off(void);

/* The pointer or the target output moved: the crop follows them. */
void screen_zoom_refresh(void);

/* Options changed: bring the level back inside the new bounds. */
void screen_zoom_reapply(void);

/*
 * Drop the magnifier's buffer for an output, once the magnifier is off and
 * before the output or its renderer goes away.
 */
void screen_zoom_release(Monitor *m);

#endif
