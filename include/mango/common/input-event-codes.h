#ifndef MANGO_INPUT_EVENT_CODES_H
#define MANGO_INPUT_EVENT_CODES_H

#if defined(__linux__)
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#define BTN_EXTRA 0x114
#define BTN_FORWARD 0x115
#define BTN_BACK 0x116
#define BTN_TASK 0x117
#endif

#endif
