#pragma once

#include <stdbool.h>
#include <stdint.h>

void keyboard_init(void);
void keyboard_interrupt_handler(void);

enum key {
	KEY_0,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,

	KEY_A,
	KEY_B,
	KEY_C,
	KEY_D,
	KEY_E,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_I,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_M,
	KEY_N,
	KEY_O,
	KEY_P,
	KEY_Q,
	KEY_R,
	KEY_S,
	KEY_T,
	KEY_U,
	KEY_V,
	KEY_W,
	KEY_X,
	KEY_Y,
	KEY_Z,

	KEY_GRAVE,
	KEY_TILDE,
	KEY_EXCLAIM,
	KEY_AT,
	KEY_HASH,
	KEY_DOLLAR,
	KEY_PERCENT,
	KEY_CARAT,
	KEY_AMP,
	KEY_STAR,
	KEY_LPAREN,
	KEY_RPAREN,
	KEY_MINUS,
	KEY_UNDERSCORE,
	KEY_EQUALS,
	KEY_PLUS,
	/* ... */
	KEY_SLASH,
	KEY_QUESTION,

	KEY_SPACE,
	KEY_ENTER,
	KEY_TAB,
	KEY_BACKSPACE,

	KEY_LSHIFT,
	KEY_RSHIFT,

	KEY_MAX
};

extern bool kb_state[KEY_MAX];
extern char kb_ascii_map[KEY_MAX];

struct key_event {
	enum key key;
	enum key_event_type {
		KEY_PRESS,
		KEY_RELEASE,
	} type;
};

void ke_push(struct key_event event);
bool ke_query(void);
struct key_event ke_pop(void);
