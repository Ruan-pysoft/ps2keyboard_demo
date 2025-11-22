BITS 32

; see https://wiki.osdev.org/Interrupt_Descriptor_Table

section .data
align 8
IDT:

resq 50

IDT_end:

align 8
idtr:
dw IDT_end - IDT - 1 ; size
dd IDT ; offset

section .text
global load_idt

%macro set_interrupt 1
	mov [IDT + %1*8], ax
	mov word [IDT + %1*8 + 2], 0x8
	mov word [IDT + %1*8 + 4], 0x8E00
	shr eax, 16
	mov [IDT + %1*8 + 6], ax
%endmacro
%macro set_trap 1
	mov [IDT + %1*8], ax
	mov word [IDT + %1*8 + 2], 0x8
	mov word [IDT + %1*8 + 4], 0x8F00
	shr eax, 16
	mov [IDT + %1*8 + 6], ax
%endmacro

align 4
debug_isr:
	mov dword [0xB8000], ') : '
	cli
	hlt

align 4
double_fault_isr:
	mov dword [0xB8000], 'D F '
	cli
	hlt
align 4
general_protection_isr:
	mov dword [0xB8000], 'G P '
	cli
	hlt
align 4
page_fault_isr:
	mov dword [0xB8000], 'P F '
	cli
	hlt
align 4
alignment_check_isr:
	mov dword [0xB8000], 'A C '
	cli
	hlt

align 4
irq1_keyboard:
	; see https://wiki.osdev.org/Interrupt_Service_Routines
	pushad
	cld
	extern keyboard_interrupt_handler
	call keyboard_interrupt_handler
	popad
	iret


load_idt:
	cli

	mov eax, double_fault_isr
	set_interrupt 8
	mov eax, general_protection_isr
	set_interrupt 13
	mov eax, page_fault_isr
	set_interrupt 14
	mov eax, alignment_check_isr
	set_interrupt 17

	mov eax, irq1_keyboard
	set_interrupt 0x21

	mov eax, debug_isr
	set_interrupt 49

	lidt [idtr]

	ret
