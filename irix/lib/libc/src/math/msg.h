#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	fprintf(stderr, "libc not compiled -n32 or -64\n");
	exit(0xdeadbeef);
#endif

