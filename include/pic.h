#pragma once

#include <stdint.h>

void PIC_sendEOI(uint8_t irq);

void PIC_remap(int offset1, int offset2);

uint8_t PIC1_IRQ_get_mask(void);
uint8_t PIC2_IRQ_get_mask(void);
void PIC1_IRQ_set_mask(uint8_t mask);
void PIC2_IRQ_set_mask(uint8_t mask);

void IRQ_mask(uint8_t line);
void IRQ_unmask(uint8_t line);
