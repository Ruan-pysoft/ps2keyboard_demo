BITS 32

section .text
global load_gdt

align 8
GDT:
; NULL descriptor
dq 0

; Code Segment
; quad 0x00CF9A000000FFFF
dw 0xFFFF ; limit bits 0 - 15
dw 0 ; base bits 0 - 15
db 0 ; base bits 16 - 23
db 0x9A ; access byte
db 0xCF ; limit bits 16 - 19 & flags
db 0 ; base bits 24 - 31

; Data Segment
; quad 0x00CF92000000FFFF
dw 0xFFFF ; limit bits 0 - 15
dw 0 ; base bits 0 - 15
db 0 ; base bits 16 - 23
db 0x92 ; access byte
db 0xCF ; limit bits 16 - 19 & flags
db 0 ; base bits 24 - 31
GDT_end:

align 8
gdtr:
dw GDT_end - GDT - 1 ; limit storage
dd GDT ; base storage

load_gdt:
	; flat/long mode setup from https://wiki.osdev.org/GDT_Tutorial

	cli
	lgdt [gdtr]

	; reload segments

	jmp 0x08:reload_CS
reload_CS:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	ret
