#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define SRCDIR "src/"
#define BUILDDIR "build/"
#define KERNELNAME "ps2kernel"

#define CC_POST "/opt/cross/bin/i686-elf-gcc"
#define AS_POST "/opt/cross/bin/i686-elf-as"
#define LD_POST "/opt/cross/bin/i686-elf-gcc"
const char *CC;
const char *AS;
const char *LD;

#define CONST_CMD(name, ...) \
const char *_RAW_##name[] = { __VA_ARGS__ }; \
const Cmd name = { .items = _RAW_##name, .capacity = ARRAY_LEN(_RAW_##name), .count = ARRAY_LEN(_RAW_##name), };

CONST_CMD(CFLAGS,
	"-m32",
	"-ffreestanding",
	"-nostdlib",
	"-O2",
	"-std=c99",
	"-Wall",
	"-Wextra",
	"-I./include",
	"-isystem", "./include/libk",
);
CONST_CMD(LDFLAGS,
	  "-O2",
	  "-nostdlib",
);
CONST_CMD(LD_LFLAGS,
	  //"-lgcc",
);

Cmd cmd = {0};
Procs procs = {0};

bool debug = false;

bool assemble(const char *module, Cmd *objs) {
	const char *source = temp_sprintf(SRCDIR"%s.s", module);
	const char *object = temp_sprintf(BUILDDIR"%s.o", module);

	cmd_append(&cmd, AS, source, "-o", object);
	if (debug) cmd_append(&cmd, "-g");
	if (!cmd_run(&cmd, .async = &procs)) return false;
	cmd_append(objs, object);

	return true;
}

bool nasm_assemble(const char *module, Cmd *objs) {
	const char *source = temp_sprintf(SRCDIR"%s.s", module);
	const char *object = temp_sprintf(BUILDDIR"%s.o", module);

	cmd_append(&cmd, "nasm", "-felf32", source, "-o", object);
	if (debug) cmd_append(&cmd, "-g");
	if (!cmd_run(&cmd, .async = &procs)) return false;
	cmd_append(objs, object);

	return true;
}

bool compile(const char *module, Cmd *objs) {
	const char *source = temp_sprintf(SRCDIR"%s.c", module);
	const char *object = temp_sprintf(BUILDDIR"%s.o", module);

	cmd_append(&cmd, CC);
	cmd_extend(&cmd, &CFLAGS);
	if (debug) cmd_append(&cmd, "-g", "-Og");
	cmd_append(&cmd, "-c", source, "-o", object);
	if (!cmd_run(&cmd, .async = &procs)) return false;
	cmd_append(objs, object);

	return true;
}

#define try(thing) if (!(thing)) return 1

int main(int argc, char **argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "debug") == 0 || strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
			debug = true;
		}
	}

	const char *HOME = getenv("HOME");
	NOB_ASSERT(HOME != NULL);
	CC = temp_sprintf("%s%s", HOME, CC_POST);
	AS = temp_sprintf("%s%s", HOME, AS_POST);
	LD = temp_sprintf("%s%s", HOME, LD_POST);

	try(mkdir_if_not_exists(BUILDDIR));

	/* COMPILE OBJECT FILES */

	Cmd objs = {0};

	try(assemble("boot", &objs));
	try(nasm_assemble("gdt", &objs));
	try(nasm_assemble("idt", &objs));
	try(compile("kernel", &objs));
	try(compile("vga", &objs));
	try(compile("pic", &objs));
	try(compile("ps2", &objs));
	try(procs_wait(procs));
	
	cmd_append(&cmd, LD);
	cmd_append(&cmd, "-T", SRCDIR"linker.ld");
	cmd_extend(&cmd, &LDFLAGS);
	cmd_append(&cmd, "-o", BUILDDIR KERNELNAME".bin");
	cmd_extend(&cmd, &objs);
	cmd_extend(&cmd, &LD_LFLAGS);
	try(cmd_run(&cmd));

	cmd_append(&cmd, "grub-file", "--is-x86-multiboot", BUILDDIR KERNELNAME".bin");
	if (cmd_run(&cmd)) {
		puts("multiboot confirmed");
	} else {
		puts("the file is not multiboot");
		return 1;
	}

	for (int i = 0; i < argc; ++i) {
		if (strcmp(argv[i], "run") == 0) {
			cmd_append(&cmd, "qemu-system-i386", "-kernel", BUILDDIR KERNELNAME".bin");
			if (debug) cmd_append(&cmd, "-S", "-s");
			try(cmd_run(&cmd));
			break;
		}
	}

	return 0;
}
