#include "pic.h"

#include "utils.h"

// see https://wiki.osdev.org/8259_PIC#Programming_with_the_8259_PIC

#define PIC1 0x20 /* IO base address for master PIC */
#define PIC2 0xA0 /* IO base address for slave PIC */
#define COMM(PIC) (PIC)
#define DATA(PIC) ((PIC) + 1)

#define PIC_EOI 0x20

void PIC_sendEOI(uint8_t irq) {
	if (irq >= 8) outb(COMM(PIC2), PIC_EOI);
	outb(COMM(PIC1), PIC_EOI);
}

#define ICW1_ICW4	0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

#define CASCADE_IRQ 2

void PIC_remap(int offset1, int offset2) {
	outb(COMM(PIC1), ICW1_INIT | ICW1_ICW4); // start init sequence in cascade mode
	io_wait();
	outb(COMM(PIC2), ICW1_INIT | ICW1_ICW4);
	io_wait();

	outb(DATA(PIC1), offset1); // ICW2: master PIC vector offset
	io_wait();
	outb(DATA(PIC2), offset2); // ICW2: slave PIC vector offset
	io_wait();

	outb(DATA(PIC1), 1 << CASCADE_IRQ); // ICW3: tell master PIC that there is a slave at IRQ2
	io_wait();
	outb(DATA(PIC2), CASCADE_IRQ); // ICW3: tell slave PIC its cascade identity
	io_wait();

	outb(DATA(PIC1), ICW4_8086); // have the PIC's use 8086 mode
	io_wait();
	outb(DATA(PIC2), ICW4_8086);
	io_wait();

	// unmask PIC's
	outb(DATA(PIC1), 0);
	outb(DATA(PIC2), 0);
}

uint8_t PIC1_IRQ_get_mask(void) {
	return inb(DATA(PIC1));
}
uint8_t PIC2_IRQ_get_mask(void) {
	return inb(DATA(PIC2));
}
void PIC1_IRQ_set_mask(uint8_t mask) {
	outb(DATA(PIC1), mask);
}
void PIC2_IRQ_set_mask(uint8_t mask) {
	outb(DATA(PIC2), mask);
}

void IRQ_mask(uint8_t line) {
	if (line < 8) PIC1_IRQ_set_mask(PIC1_IRQ_get_mask() | (1 << line));
	else PIC2_IRQ_set_mask(PIC2_IRQ_get_mask() | (1 << (line-8)));
}
void IRQ_unmask(uint8_t line) {
	if (line < 8) PIC1_IRQ_set_mask(PIC1_IRQ_get_mask() & ~(1 << line));
	else PIC2_IRQ_set_mask(PIC2_IRQ_get_mask() & ~(1 << (line-8)));
}
