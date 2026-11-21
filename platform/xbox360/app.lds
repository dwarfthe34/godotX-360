ENTRY(_start)
SECTIONS
{
	/*
	 * XEX/PE images reserve the first 0x1000 bytes of the image for headers.
	 * Place a skipped .note.* section at the image base so the first PT_LOAD
	 * starts at 0x80000000 and .text lands at RVA 0x1000. Without this,
	 * XexTool's pack command rejects .text for overlapping the PE headers.
	 */
	. = 0x80000000;
	.note.headroom : { . += 0x1000; }

	.text : {
		*(.except*);
		*(.pagetable);
		pagetable_end = .;
		*libxenon.a:cache.o(.text);
		*(.init*);
		*(.fini*);
		*(.text*);
	}
	.elfldr : { elfldr_start = .; *(.elfldr); elfldr_end = .; }

	/* XEX maps memory in 64 KiB pages with one protection flag per page.
	 * Keep writable data off the final code page so .text never shares a
	 * page with .data (a shared page would be mapped RW but not executable). */
	.data ALIGN(0x10000) : { *(.data*) }
	.sdata : { *(.sdata*) }
	.rodata : { *(.rodata*) }

	.ctors : { __CTOR_LIST__ = .; *(.ctors); __CTOR_END__ = .; }
	.dtors : { __DTOR_LIST__ = .; *(.dtors); __DTOR_END__ = .; }

	.eh_frame_hdr : { *(.eh_frame_hdr) }
	.eh_frame ALIGN(0x4): { PROVIDE (__eh_frame_start = .); KEEP (*(.eh_frame)) LONG (0); }
	.gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) }

	bss_start = .;
	.bss : { *(.bss.beginning.lower); *(.bss.beginning.upper); *(.bss*) }
	.sbss : { *(.sbss .sbss.*) }
	bss_end = .;

	heap_begin = .;
	. = 0x9E000000;
	__libc_stack_end = .;
	heap_end = .;
	. = 0xa0000000;
}
