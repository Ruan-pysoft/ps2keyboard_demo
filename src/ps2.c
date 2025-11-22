#include "ps2.h"

#include <stdbool.h>
#include <stdint.h>

#include "pic.h"
#include "utils.h"
#include "vga.h"

#define DATA_PORT 0x60
#define STATUS_READ 0x64
#define COMMAND_WRITE 0x64

#define CMD_READ_CONFIG     0x20
#define CMD_READ(n)         (0x20 + n)
#define CMD_WRITE_CONFIG    0x60
#define CMD_WRITE(n)        (0x60 + n)
#define CMD_PORT2_DISABLE   0xA7
#define CMD_PORT2_ENABLE    0xA8
#define CMD_PORT2_TEST      0xA9
#define CMD_CONTROLLER_TEST 0xAA
#define CMD_PORT1_TEST      0xAB
#define CMD_DUMP_DIAGNOSTIC 0xAC
#define CMD_PORT1_DISABLE   0xAD
#define CMD_PORT1_ENABLE    0xAE
#define CMD_SEND_PORT2      0xD4

#define CFG_PORT1_INTR      (1 << 0)
#define CFG_PORT2_INTR      (1 << 1)
#define CFG_SYS             (1 << 2) // should always be set
#define CFG_PORT1_CLOCK_OFF (1 << 4)
#define CFG_PORT2_CLOCK_OFF (1 << 5)
#define CFG_PORT1_TRANSLATE (1 << 6)

static bool has_two_channels;
static bool port1_works;
static bool port2_works;
static bool port1_iskb;
static uint8_t port1_kbtp;
static bool port2_iskb;
static uint8_t port2_kbtp;

static inline bool can_take_input(void) {
	return (inb(STATUS_READ)&0b10) == 0;
}
static inline void wait_for_writable(void) {
	while (!can_take_input()); // wait for input buffer to be clear
}
static inline bool has_output(void) {
	return inb(STATUS_READ)&0b1;
}
static inline void wait_for_readable(void) {
	while (!has_output()); // wait for output buffer to be full
}
static inline void send_commb(uint8_t byte) {
	wait_for_writable();
	outb(COMMAND_WRITE, byte);
}
static inline void send_commb2(uint8_t byte0, uint8_t byte1) {
	send_commb(byte0);
	wait_for_writable();
	outb(DATA_PORT, byte1);
}
static inline void send_commw(uint16_t word) {
	send_commb2(word&0xFF, word>>8);
}
static inline uint8_t read_resp(void) {
	wait_for_readable();
	return inb(DATA_PORT);
}

static bool port1_sendb(uint8_t byte) {
	int tries = 0;

	while (tries < 1024) {
		if (!can_take_input()) {
			++tries;
		} else {
			outb(DATA_PORT, byte);

			return true;
		}
	}

	return false;
}
static bool port2_sendb(uint8_t byte) {
	int tries = 0;

	send_commb(CMD_SEND_PORT2);

	while (tries < 1024) {
		if (!can_take_input()) {
			io_wait();
			++tries;
		} else {
			outb(DATA_PORT, byte);

			return true;
		}
	}

	return false;
}

static bool read_with_timeout(uint8_t *out) {
	int tries = 0;

	while (tries < 1024) {
		if (has_output()) {
			*out = inb(DATA_PORT);
			return true;
		} else {
			io_wait();
			++tries;
		}
	}

	return false;
}

static void kb_sched_comm(uint8_t comm);

void keyboard_init(void) {
	// disable PS/2
	send_commb(CMD_PORT1_DISABLE);
	send_commb(CMD_PORT2_DISABLE);

	// flush output buffer
	while (has_output()) inb(DATA_PORT);

	// set controller config byte
	send_commb(CMD_READ_CONFIG);
	uint8_t cfg = read_resp();
	cfg &= ~CFG_PORT1_INTR;
	cfg &= ~CFG_PORT1_TRANSLATE;
	cfg &= ~CFG_PORT1_CLOCK_OFF;
	send_commb2(CMD_WRITE_CONFIG, cfg);

	// perform controller self test
	send_commb(CMD_CONTROLLER_TEST);
	if (read_resp() != 0x55) {
		// self test failed!
		terminal_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_RED));
		terminal_writestring("Failed PS/2 controller self-test!");
		return;
	}
	send_commb2(CMD_WRITE_CONFIG, cfg);

	// determine if there are two channels
	send_commb(CMD_PORT2_ENABLE);
	send_commb(CMD_READ_CONFIG);
	cfg = read_resp();
	if (cfg & CFG_PORT2_CLOCK_OFF) {
		has_two_channels = false;
	} else {
		has_two_channels = true;
		send_commb(CMD_PORT2_DISABLE);
		cfg &= ~CFG_PORT2_INTR;
		cfg &= ~CFG_PORT2_CLOCK_OFF;
		send_commb2(CMD_WRITE_CONFIG, cfg);
	}

	// perform interface tests
	send_commb(CMD_PORT1_TEST);
	port1_works = read_resp() == 0;
	send_commb(CMD_PORT2_TEST);
	port2_works = read_resp() == 0;
	if (!port1_works && !port2_works) {
		// no PS/2 ports working!
		terminal_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_RED));
		terminal_writestring("No working PS/2 ports available!");
		return;
	}

	// enable devices
	if (port1_works) {
		send_commb(CMD_PORT1_ENABLE);
		cfg |= CFG_PORT1_INTR;
	}
	if (port2_works) {
		send_commb(CMD_PORT2_ENABLE);
		cfg |= CFG_PORT2_INTR;
	}
	send_commb2(CMD_WRITE_CONFIG, cfg);

	// reset devices
	// WARN: I'm pretty sure this code is brittle... bytes between different ports as well as responses and user inputs might get mixed up
	if (port1_works) {
		port1_sendb(0xFF);
		uint8_t byte0 = 0, byte1 = 0;
		if (!read_with_timeout(&byte0) || !read_with_timeout(&byte1)) {
			port1_works = false;
			send_commb(CMD_PORT1_DISABLE);
		}

		if ((byte0 == 0xFA && byte1 == 0xAA) || (byte0 == 0xAA && byte1 == 0xFA)) {
			uint8_t device_type;
			if (!read_with_timeout(&device_type)) {
				port1_iskb = true;
				port1_kbtp = 0;
			} else if (device_type == 0xAB) {
				port1_iskb = true;
				port1_kbtp = 0;
				read_with_timeout(&port1_kbtp);
			} else {
				port1_iskb = false;
			}
		} else {
			port1_works = false;
			send_commb(CMD_PORT1_DISABLE);
		}
	}
	if (port2_works) {
		port2_sendb(0xFF);
		uint8_t byte0 = 0, byte1 = 0;
		if (!read_with_timeout(&byte0) || !read_with_timeout(&byte1)) {
			port2_works = false;
			send_commb(CMD_PORT1_DISABLE);
		}

		if ((byte0 == 0xFA && byte1 == 0xAA) || (byte0 == 0xAA && byte1 == 0xFA)) {
			uint8_t device_type;
			if (!read_with_timeout(&device_type)) {
				port2_iskb = true;
				port2_kbtp = 0;
			} else if (device_type == 0xAB) {
				port2_iskb = true;
				port2_kbtp = 0;
				read_with_timeout(&port2_kbtp);
			} else {
				port2_iskb = false;
			}
		} else {
			port2_works = false;
			send_commb(CMD_PORT2_DISABLE);
		}
	}

	if (!port1_iskb) {
		terminal_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_RED));
		terminal_writestring("Unexpected PS/2 setup (port1 is not keyboard)!");
		return;
	}

	// enable keyboard scancode sending
	kb_sched_comm(0xF4);
}

static uint8_t kb_comm_queue[256];
static uint8_t kb_cq_head = 0;
static uint8_t kb_cq_tail = 0;

static void kb_comm_send(void);
static void kb_sched_comm(uint8_t comm) {
	const bool exec = kb_cq_tail == kb_cq_head;
	kb_comm_queue[kb_cq_tail++] = comm;
	if (exec) {
		port1_sendb(kb_comm_queue[kb_cq_head]);
	}
}

bool kb_state[KEY_MAX] = {0};
char kb_ascii_map[KEY_MAX] = {
	[KEY_0] = '0',
	[KEY_1] = '1',
	[KEY_2] = '2',
	[KEY_3] = '3',
	[KEY_4] = '4',
	[KEY_5] = '5',
	[KEY_6] = '6',
	[KEY_7] = '7',
	[KEY_8] = '8',
	[KEY_9] = '9',

	[KEY_A] = 'a',
	[KEY_B] = 'b',
	[KEY_C] = 'c',
	[KEY_D] = 'd',
	[KEY_E] = 'e',
	[KEY_F] = 'f',
	[KEY_G] = 'g',
	[KEY_H] = 'h',
	[KEY_I] = 'i',
	[KEY_J] = 'j',
	[KEY_K] = 'k',
	[KEY_L] = 'l',
	[KEY_M] = 'm',
	[KEY_N] = 'n',
	[KEY_O] = 'o',
	[KEY_P] = 'p',
	[KEY_Q] = 'q',
	[KEY_R] = 'r',
	[KEY_S] = 's',
	[KEY_T] = 't',
	[KEY_U] = 'u',
	[KEY_V] = 'v',
	[KEY_W] = 'w',
	[KEY_X] = 'x',
	[KEY_Y] = 'y',
	[KEY_Z] = 'z',

	[KEY_SPACE] = ' ',
	[KEY_ENTER] = '\n',
	[KEY_TAB] = '\t',

	//0,
};

static struct key_event ke_queue[256];
static uint8_t ke_queue_head = 0;
static uint8_t ke_queue_tail = 0;

void ke_push(struct key_event event) {
	// drop events if the queue is full
	if (ke_queue_tail + 1 != ke_queue_head) {
		ke_queue[ke_queue_tail++] = event;
	}
}
bool ke_query(void) {
	return ke_queue_tail != ke_queue_head;
}
struct key_event ke_pop(void) {
	return ke_queue[ke_queue_head++];
}

static bool key_release_pending = false;
#define KEY(byte, kb_key) \
	case byte: { \
		ke_push((struct key_event) { .key = KEY_##kb_key, .type = key_release_pending ? KEY_RELEASE : KEY_PRESS }); \
		kb_state[KEY_##kb_key] = !key_release_pending; \
		key_release_pending = false; \
	} break
void kb_handle_scancode(uint8_t initial_byte) {
	if (initial_byte == 0xF0) {
		key_release_pending = true;
		return;
	}
	switch (initial_byte) {
		KEY(0x0D, TAB);
		KEY(0x15, Q);
		KEY(0x16, 1);
		KEY(0x1A, Z);
		KEY(0x1B, S);
		KEY(0x1C, A);
		KEY(0x1D, W);
		KEY(0x1E, 2);
		KEY(0x21, C);
		KEY(0x22, X);
		KEY(0x23, D);
		KEY(0x24, E);
		KEY(0x25, 4);
		KEY(0x26, 3);
		KEY(0x29, SPACE);
		KEY(0x2A, V);
		KEY(0x2B, F);
		KEY(0x2C, T);
		KEY(0x2D, R);
		KEY(0x2E, 5);
		KEY(0x31, N);
		KEY(0x32, B);
		KEY(0x33, H);
		KEY(0x34, G);
		KEY(0x35, Y);
		KEY(0x36, 6);
		KEY(0x3A, M);
		KEY(0x3B, J);
		KEY(0x3C, U);
		KEY(0x3D, 7);
		KEY(0x3E, 8);
		KEY(0x42, K);
		KEY(0x43, I);
		KEY(0x44, O);
		KEY(0x45, 0);
		KEY(0x46, 9);
		KEY(0x4B, L);
		KEY(0x4D, P);
		KEY(0x5A, ENTER);
	}
}

void keyboard_interrupt_handler(void) {
	uint8_t initial_byte = inb(DATA_PORT);

	if (initial_byte == 0 || initial_byte == 0xFF) {
		// key detection error or internal buffer overrun
		terminal_putchar('!');
	} else if (initial_byte == 0xAA) {
		// self test passed
		terminal_putchar('v');
	} else if (initial_byte == 0xEE) {
		// echo response
		terminal_putchar('e');
	} else if (initial_byte == 0xFA) {
		// ACK
		//terminal_putchar('A');
		++kb_cq_head;
		if (kb_cq_head != kb_cq_tail) {
			port1_sendb(kb_comm_queue[kb_cq_head]);
		}
	} else if (initial_byte == 0xFC || initial_byte == 0xFD) {
		// self test failed
		terminal_putchar('x');
	} else if (initial_byte == 0xFF) {
		// resend
		//terminal_putchar('?');
		if (kb_cq_head != kb_cq_tail) {
			port1_sendb(kb_comm_queue[kb_cq_head]);
		}
	} else {
		kb_handle_scancode(initial_byte);
	}
	PIC_sendEOI(1);
}
