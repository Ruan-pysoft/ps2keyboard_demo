/* from https://wiki.osdev.org/Bare_Bones */

#include <stddef.h>
#include <stdint.h>

#include "utils.h"
#include "vga.h"
#include "pic.h"
#include "ps2.h"

/* Check if the compiler thinks you are targeting the wrong operating system */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with an ix86-elf compiler"
#endif

void kernel_early_main(void) {
	disable_interrupts();

	terminal_initialize();

	// map IRQ0 - IRQ7 to 0x20 to 0x27 and IRQ8 - IRQ15 to 0x70 to 0x77
	// and initialise the PIC
	PIC_remap(0x20, 0x70);
	//disable all PIC interrupts
	PIC1_IRQ_set_mask(0b11111111);
	PIC2_IRQ_set_mask(0b11111111);

	keyboard_init();
	IRQ_unmask(1); // enable keyboard interrupts

	enable_interrupts();
}

void kernel_main(void) {
	terminal_writestring("Hello, world!\r\n");

	for (;;) {
		while (ke_query()) {
			struct key_event event = ke_pop();
			if (event.type != KEY_PRESS && event.type != KEY_BOUNCE) continue;

			if (event.key == KEY_ENTER) {
				terminal_writestring("\r\n");
			} else if (event.key == KEY_BACKSPACE) {
				size_t x, y;
				x = terminal_getx();
				y = terminal_gety();

				if (x > 0) {
					--x;
					terminal_goto(x, y);
					terminal_putchar(' ');
					terminal_goto(x, y);
				}
			} else if (kb_ascii_map[event.key]) {
				terminal_putchar(kb_ascii_map[event.key]);
			}
		}

		__asm__ volatile("hlt" ::: "memory");
	}
}
