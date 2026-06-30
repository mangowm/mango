#ifndef TAG_SCRUB_MATH_H
#define TAG_SCRUB_MATH_H
#include <stdbool.h>
#include <stdint.h>
#define TAG_SCRUB_PROJECTION_FACTOR 18.0

static inline double tag_scrub_progress(double accumulated_delta,
										double monitor_dim) {
	if (monitor_dim <= 0.0)
		return 0.0;
	double p = accumulated_delta / monitor_dim;
	if (p > 1.0)
		p = 1.0;
	if (p < -1.0)
		p = -1.0;
	return p;
}

static inline int tag_scrub_neighbor(int curtag, int dir, int ntags,
									 uint32_t occupied_mask, bool have_client,
									 bool wrap) {
	if (curtag < 1 || curtag > ntags || (dir != 1 && dir != -1))
		return 0;
	bool only_current_occupied = (occupied_mask & ~(1u << (curtag - 1))) == 0;
	bool skip_empty = have_client && !only_current_occupied;
	int t = curtag;
	for (int step = 0; step < ntags; step++) {
		t += dir;
		if (t < 1) {
			if (!wrap)
				return 0;
			t = ntags;
		} else if (t > ntags) {
			if (!wrap)
				return 0;
			t = 1;
		}
		if (t == curtag)
			return 0;
		if (!skip_empty)
			return t;
		if (occupied_mask & (1u << (t - 1)))
			return t;
	}
	return 0;
}

static inline bool gesture_scrub_should_commit(double progress_magnitude,
											   double velocity,
											   double commit_ratio) {
	double projected =
		progress_magnitude + velocity * TAG_SCRUB_PROJECTION_FACTOR;
	if (projected < 0.0)
		projected = -projected;
	return projected >= commit_ratio;
}

#endif /* TAG_SCRUB_MATH_H */
