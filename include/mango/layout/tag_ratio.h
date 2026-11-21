#ifndef __LAYOUT_TAG_RATIO_H__
#define __LAYOUT_TAG_RATIO_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>

struct TagRatioState;

bool layout_uses_tag_ratios(const Layout *layout);

struct TagRatioState *ensure_tag_ratio_state(Monitor *m, uint32_t tag);

void tag_ratio_state_begin(Monitor *m, uint32_t tag, bool from_view);

void sync_tag_ratio_state_from_clients(Monitor *m, uint32_t tag);

void tag_ratio_state_seed_client(Client *c);

int32_t tag_ratio_state_client_count(Monitor *m, uint32_t tag);

void tag_ratio_state_remove_client_in_tag(Monitor *m, uint32_t tag, Client *c);

void tag_ratio_state_replace_client_in_tag(Monitor *m, uint32_t tag,
										   Client *old, Client *new);

void tag_ratio_state_copy(Monitor *target, uint32_t dst_tag,
						  const Monitor *source, uint32_t src_tag);

void tag_ratio_state_move(Monitor *m, uint32_t dst_tag, uint32_t src_tag);

void tag_ratio_state_reset(Monitor *m, uint32_t tag);

void cleanup_monitor_tag_ratio_states(Monitor *m);

void tag_ratio_state_set_master_mfact(Monitor *m, uint32_t tag, double mfact);

#endif
