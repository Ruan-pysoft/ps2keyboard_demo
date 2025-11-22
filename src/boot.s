/* from https://wiki.osdev.org/Bare_Bones */

/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number', lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/*
 * Declare a multiboot header that marks the program as a kernel. These are
 * magic values that are documented in the multiboot standard. The bootloader
 * will search for this signature in the first 8KiB of the kernel file, aligned
 * at a 32-bit boundary. The signature is in its own section so the header can
 * be forced to be within the first 8 KiB of the kernel file.
 */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/*
 * The multiboot standard does not define the value of the stack pointer
 * register (esp) and it is up to the kernel to provide a stack. This allocates
 * room for a small stack by creating a symbol at the bottom of it, then
 * allocating 16384 bytes for it, and finally creating a symbol at the top. The
 * stack grows downwards on x86. The stack is in its own section so it can be
 * marked nobits, which means the kernel file is smaller because it does not
 * contain an uninitialized stack. The stack on x86 must be 16-byte aligned
 * according to the System V ABI standard and de-facto extensions. The compiler
 * will assume the stack is properly aligned and failure to align the stack
 * will result in undefined behaviour.
 */
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

/*
 * The linker script specifies _start as the entry point to the kernel and the
 * bootloader will jump to this position once the kernel has been loaded. It
 * doesn't make sense to return from this function as the bootloader is gone.
 */
.section .text
.global _start
.type _start, @function
_start:
	/* loaded into 32-bit protected mode, no interrupts, no paging */
	/* for this project, I currently see no point in paging,
	   I may or may not enable interrupts, we'll see */

	cli

	/* setup stack for c++ */
	mov $stack_top, %esp

	/*
	 * note: features like floating point instructions and instruction set
	 * extensions are not initialised yet. gdt should in theory be loaded
	 * here, but if I build a single monolithic executable I don't think
	 * that's necessary. similar for paging.
	 */

	call load_gdt
	call load_idt

	/* this function should set up the absolute bare essentials */
	call kernel_early_main

	/*
	 * Enter the high-level kernel. The ABI requires the stack is 16-byte
	 * aligned at the time of the call instruction (which afterwards pushes
	 * the return pointer of size 4 bytes). The stack was originally
	 * 16-byte aligned above and we've pushed a multiple of 16 bytes to the
	 * stack since (pushed 0 bytes so far), so the alignment has thus been
	 * preserved and the call is well defined.
	 */
	call kernel_main

	cli
1:	hlt
	jmp 1b

/*
 * Set the size of the _start symbol to the current location '.' minus its
 * start. This is useful when debugging or when you implement call tracing.
 */
.size _start, . - _start
