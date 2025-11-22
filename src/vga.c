/* from https://wiki.osdev.org/Bare_Bones */

#include <stddef.h>
#include <stdint.h>

#include "vga.h"

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
	}
}
void terminal_goto(size_t x, size_t y) {
	if (x >= VGA_WIDTH) x = VGA_WIDTH-1;
	if (y >= VGA_HEIGHT) y = VGA_HEIGHT-1;

	term_row = y;
	term_col = x;
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
		if (VGA_HEIGHT - term_row < 3) terminal_scroll(5);
	} else if (c == '\t') {
		term_col &= ~3;
		term_col += 4;
	} else if (c == '\r') {
		term_col = 0;
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
