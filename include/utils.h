#pragma once

#include <stdint.h>

// see https://wiki.osdev.org/Inline_Assembly/Examples#I/O_access

static inline void outb(uint16_t port, uint8_t val) {
	__asm__ volatile(
		"outb %b0, %w1"
		:
		: "a" (val), "Nd" (port)
		: "memory"
	);
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile(
		"inb %w1, %b0"
		: "=a" (ret)
		: "Nd" (port)
		: "memory"
	);
	return ret;
}

static inline void io_wait(void) {
	outb(0x80, 0);
}

static inline void disable_interrupts(void) {
	__asm__ volatile( "cli" ::: "memory");
}
static inline void enable_interrupts(void) {
	__asm__ volatile( "sti" ::: "memory");
}
