#ifndef DYNARR_H__
#define DYNARR_H__

#define DYNARR_DEF(t)                                                          \
	struct {                                                                   \
		t *arr;                                                                \
		size_t len, cap, size;                                                 \
	}

#define DYNARR_INIT(p)                                                         \
	((p)->arr = reallocarray((p)->arr, ((p)->cap = 1),                         \
							 ((p)->size = sizeof(((p)->arr[0])))))

#define DYNARR_FINI(p) free((p)->arr)

#define DYNARR_PUSH(p, v)                                                      \
	do {                                                                       \
		if ((p)->len >= (p)->cap) {                                            \
			while ((p)->len >= ((p)->cap *= 2))                                \
				;                                                              \
			(p)->arr = reallocarray((p)->arr, (p)->cap, (p)->size);            \
		}                                                                      \
		(p)->arr[(p)->len++] = (v);                                            \
	} while (0)

#define DYNARR_POP(p) ((p)->arr[(p)->len--])

#define DYNARR_CLR(p) ((p)->len = 0)

#endif
