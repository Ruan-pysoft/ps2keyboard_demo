# Basic Kernel with PS/2 Keyboard Handling

Based on various pages on the [osdev wiki](https://wiki.osdev.org/Main_Page). The purpose of this kernel is to be a very slimmed-down C-based kernel showing how to get PS/2 keyboard support up and running, because quite frankly the osdev wiki can be quite hard to follow at times, and I frequently found myself wishing I had a working example to illuminate what they meant.

I made this because I spent multiple days in my [C++ kernel](https://github.com/Ruan-pysoft/cpp_demo_kernel) trying to get PS/2 keyboard support working, without success. I suspect I have issues with my GDT and/or IDT setup, but it is very difficult to check, since I wasn't able to find a small, working kernel that can demonstrate how to set up the IDT, GDT, and PS/2 support; or I _could_, but all of them did a hundred different stuff as well, so it was difficult finding and isolating the parts I was interested in. So, I made my own reference.

I consider it mostly feature complete in its current state, perhaps I can add an example of "user-space" state management (ie. not dealt with by the ISR) for keeping track of the CapsLock, NumLock, ScrollLock, and Insert keys' status, and I might also add keyboard lights management as well. But in its current state, it neatly demonstrates how to interact with the PS/2 keyboard to read and handle key events, which is really what I wanted out of it.

## Building and Running

(Here I assume you have Qemu installed, and specifically `qemu-system-i386`. If you don't, the kernel could possibly also work on Bochs or Oracle Virtualbox or some other VM platform, but I haven't tested it)

Also, my build instructions assume a Linux system with (at minimum) the GNU C Compiler, GNU's `as`, and NASM installed. Once again, you can likely get it working on other systems/with other tools, but you'll have to figure it out yourself.

The project is built using [nob.h](https://github.com/tsoding/nob.h). To build, first bootstrap the build system by running `cc -o nob nob.c`, and then build the kernel by running `./nob`.

The kernel can be found in `build/ps2kernel.bin`.

You can run the kernel either by calling `qemu-system-i386 -kernel build/ps2kernel.bin` manually, or using `./nob run`, which builds and then runs the kernel.

You can build/run debug builds of the kernel (starting QEMU suspended and listening for a GDB connection) using `./nob debug` and `./nob run debug`.

## Basic Overview

Linker instructions can be found at `src/linker.ld`, which is copied over from my C++ kernel.

The `_start` function is located in `src/boot.s`, written in GNU assembler format; once again, it is mostly copied over from my C++ kernel.

I load the GDT and IDT in the files `src/gdt.s` and `src/idt.s`. It is surprisingly straight-forward, about a screenful of actual code in each case, despite the fact that each one took me multiple hours to get right. They are both written in NASM, because I couldn't figure out how to get long jumps working properly for `src/gdt.s` in GNU assembly (I was probably being stupid). Though I prefer NASM anyways, tbh, so I might just rewrite `src/boot.s` at some point to drop the GNU `as` dependency.

There is some basic support for the VGA text-mode interface located in `src/vga.c`, which is mostly just copied over from the osdev wiki [Bare Bones tutorial](https://wiki.osdev.org/Bare_Bones), since I couldn't just copy over my C++ code into a C project. Well, I could, that's the beauty of C and C++, but it would defeat the point of a tiny reference kernel written in C. Also, C is just more fun to work with than C++, so I'd rather not introduce any C++ unless I have to.

The real meat of this demo comes in `src/pic.c` and `src/ps2.c`.

Basic support for and setup of the PIC chip is located in `src/pic.c`. It is quite straight-forward, and pretty much just a direct implementation of the [8259 PIC article](https://wiki.osdev.org/8259_PIC#Programming_with_the_8259_PIC) on the osdev wiki, but having a reference implementation (and roughly in my coding style as well) is useful.

All the hard work of supporting the keyboard is found in `src/ps2.c`. This probably took me almost as much time is the IDT to get in a working state, and then another few hours adding features and refining my keyboard ISR.

At the top of `src/ps2.c` there is a bunch of utility functions for speaking to the PIC controller, but the first star of the show is the `keyboard_init` function, which initialises the PS/2 device as well as the PS/2 keyboard. The `keyboard_init` function is a _relatively_ straightforward implementation of the initialisation instructions from the [PS/2 article](https://wiki.osdev.org/I8042_PS/2_Controller). However, it did take an hour or two of playing to figure out exactly how to send and receive bytes to not cause any crashes or hangs.

Everything below the keyboard initialisation function is either my own code for keyboard handling, or an implementation of information from the [PS/2 Keyboard article](https://wiki.osdev.org/PS/2_Keyboard).

Commands sent to the keyboard are scheduled via a ring buffer (I think? It's a fix-sized queue is the point) as the next command can only be sent once the keyboard ACKS's the previous one (it does not handle interleaved commands). Furthermore, the keyboard might request a command to be re-sent. As to the actual implementation of the command queue, it has length 256 so that the tail and head can be tracked via 8-bit integers and wrapping is handled by the hardware, and I always just assume that there is space in the ring buffer, so if more than 255 (I think) commands are sent to the keyboard before it can respond, commands are going to get skipped.

Keys received from the keyboard are handled in two ways: It updates the state of each key in a state array (pressed or not), as well as adding a key press or key release event to a keyboard event queue. If a key is already down when it is pressed, a "bounce" event is sent instead, so that the software can distinguish between an actual key press and a key press generated by holding a key down. The key event queue is implemented similarly to the keyboard command queue, except that when the buffer is full it will simply drop new events, rather than overriding previous events.

I also have a `char` array, mapping keyboard keys to ASCII characters, with a key being mapped to the NULL character if it isn't associated with a graphical or space character.

The keyboard ISR calls the `keyboard_interrupt_handler` function, which may itself call the `kb_handle_scancode` function if a scan code is received.

The `keyboard_interrupt_handler` reads a single byte from the PS/2 data port, which is the byte which generated the interrupt. One important thing I have learnt is that the ISR should **only** read a single byte from the data port, otherwise read bytes will be re-sent with their own interrupt (this makes handling multi-byte scan codes... interesting). The `keyboard_interrupt_handler` finishes by sending the end of interrupt byte to the PS/2 controller.

The two response bytes I actually handle is 0xFA which is an acknowledge of a command, which will pop the front of the command queue, and send the next command if there is one; and 0xFF which requests a re-send of the previous command, which will do so if the command queue is not empty.

If the response byte is not recognised, it is assumed to be a scan code and is sent to the `kb_handle_scancode` function.

The `kb_handle_scancode` function, is pretty much just a straight implementation of the giant scan code set 2 table in the PS/2 Keyboard article, and it was a pain in the backside to make, and truly left me flabbergasted at the pure insanity of whoever designed this interface. It has to keep some state to keep track of multi-byte scan codes, as only one byte may be handled at a time. If the pause key or printscr key scancode is being received and gets interrupted by an unexpected byte, then it is dropped; a printscr press/release or pause key press&release is only registered if the complete sequence is received without any interruption.

If you want to know how it is implemented, read the source code, it's not that complicated (even though it was a pain to implement). If you want a description, go read the osdev article.

## Licensing

This kernel is set free under the [Unlicense](https://unlicense.org/).
This means that it is released into the public domain
for all to use as they see fit.
Use it in a way I'll approve of,
use it in a way I won't,
doesn't make much of a difference to me.

I only ask (as a request in the name of common courtesy,
**not** as a legal requirement of any sort)
that you do not claim this work as your own
but credit me as appropriate.

The full terms are as follows:

```
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>
```
