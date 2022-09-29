/*
 * testcd protocol, for reading raw blocks from a mounted CD-ROM
 */

struct breadargs {
	short	dev;
	u_long	block;
};

union breadres switch (int status) {
case 0:
	opaque	block[2048];
default:
	void;
};

program TESTCD_PROGRAM {
	version TESTCD_VERSION {
		void
		TESTCD_NULL(void) = 0;

		breadres
		TESTCD_BREAD(breadargs) = 18;
	} = 1;
} = 391012;
