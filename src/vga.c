/* from https://wiki.osdev.org/Bare_Bones */

#include <stddef.h>
#include <stdint.h>

#include "vga.h"
#include "utils.h"

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

// from https://wiki.osdev.org/Text_Mode_Cursor
static void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}
static void disable_cursor(void) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}
static void update_cursor(int x, int y) {
	const uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos>>8) & 0xFF));
}

static size_t term_row;
static size_t term_col;
static uint8_t term_color;
static uint16_t *const term_buffer = (uint16_t*)VGA_MEMORY;

#define TERM_AT(row, col) term_buffer[(row)*VGA_WIDTH + col]

void terminal_initialize(void)
{
	term_row = 0;
	term_col = 0;
	term_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

	enable_cursor(8, 15);
	update_cursor(term_col, term_row);
	
	for (size_t y = 0; y < VGA_HEIGHT; ++y) {
		for (size_t x = 0; x < VGA_WIDTH; ++x) {
			TERM_AT(y, x) = vga_entry(' ', term_color);
		}
	}
}

void terminal_setcolor(uint8_t color)
{
	term_color = color;
}
uint8_t terminal_getcolor(void) {
	return term_color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	TERM_AT(y, x) = vga_entry(c, color);
}

static void term_adv(void) {
	if (++term_col == VGA_WIDTH) {
		term_col = 0;
		if (++term_row == VGA_HEIGHT) {
			term_row = 0;
		}
		if (VGA_HEIGHT - term_row < 3) terminal_scroll(5);
	}
	update_cursor(term_col, term_row);
}
size_t terminal_getx(void) {
	return term_col;
}
size_t terminal_gety(void) {
	return term_row;
}
void terminal_goto(size_t x, size_t y) {
	if (x >= VGA_WIDTH) x = VGA_WIDTH-1;
	if (y >= VGA_HEIGHT) y = VGA_HEIGHT-1;

	term_row = y;
	term_col = x;
	update_cursor(term_col, term_row);
}
void terminal_scroll(size_t lines) {
	if (lines >= VGA_HEIGHT) {
		for (size_t y = 0; y < VGA_HEIGHT; ++y) {
			for (size_t x = 0; x < VGA_WIDTH; ++x) {
				TERM_AT(y, x) = vga_entry(' ', term_color);
			}
		}
		terminal_goto(term_col, 0);
	} else {
		for (size_t y = 0; y + lines < VGA_HEIGHT; ++y) {
			for (size_t x = 0; x < VGA_WIDTH; ++x) {
				TERM_AT(y, x) = TERM_AT(y + lines, x);
			}
		}
		for (size_t y = VGA_HEIGHT - lines; y < VGA_HEIGHT; ++y) {
			for (size_t x = 0; x < VGA_WIDTH; ++x) {
				TERM_AT(y, x) = vga_entry(' ', term_color);
			}
		}
		if (term_row > lines) terminal_goto(term_col, term_row - lines);
		else terminal_goto(term_col, 0);
	}
}
void terminal_putchar(char c)
{
	if (c == '\n') {
		++term_row;
		update_cursor(term_col, term_row);
		if (VGA_HEIGHT - term_row < 3) terminal_scroll(5);
	} else if (c == '\t') {
		term_col &= ~3;
		term_col += 4;
		update_cursor(term_col, term_row);
	} else if (c == '\r') {
		term_col = 0;
		update_cursor(term_col, term_row);
	} else {
		terminal_putentryat(c, term_color, term_col, term_row);
		term_adv();
	}
}

void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}
