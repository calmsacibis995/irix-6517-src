#ident	"IP22diags/pon/pon_caches.c:  $Revision: 1.2 $"

#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/types.h"

/*
 * pon_caches: verify minimal functionality of the caches
 *
 * checks for:
 *	stuck data lines
 *	shorted data lines
 *	stuck address lines
 *	shorted address lines
 *	basic spill/fill functionality
 *
 * verify that we can walk ones and zeros across the
 * entire width of each cache bank, that we can walk
 * ones and zeros across the width of the address lines
 * to each cache ram, and that we can select cache
 * banks properly.
 */

typedef unsigned short	cdata_t; 		/* interesting data size for cache tests */

#define	CACHE_BANK	0x080000		/* banks are at least half a megabyte */
#define	CACHE_SIZE	0x400000		/* caches are at most four megabytes */
#define	CACHE_WIDE	(256/(8 * sizeof (cdata_t)))	/* data paths are up to 256 bits wide */

#define	CACHE_MAXIX	(CACHE_SIZE/(sizeof (cdata_t)))
#define	BANK_MAXIX	(CACHE_BANK/(sizeof (cdata_t)))

#define	MEMORY_ARENA_OFFSET	0xC00000	/* memory offset we can scribble on */
#define	MEMORY_ARENA_ADDR	(K0_RAMBASE + 0xC00000)

static void
report_cache_error(cdata_t addr, unsigned exp, unsigned act, char *tname)
{
    uncache_K0();
    puts(tname);
    cache_K0();
}

#define	EXPECT(base, index, expr, tname)				\
{									\
    cdata_t    *addr = (cdata_t *)((base)+(index));			\
    cdata_t	exp = (expr);						\
    cdata_t	act = *addr;						\
    if (exp != act) {							\
	report_cache_error(addr, exp, act, tname);			\
	error ++;							\
    }									\
}

volatile cdata_t	flush_dummy;

pon_caches()
{
    unsigned		bank_offset;
    volatile cdata_t   *bank_ptr;
    volatile cdata_t   *flush_ptr;
    unsigned		test_index;
    cdata_t		test_data;
    int			i;
    int			error = 0;

    puts("Secondary and primary caches address/data test\n");
    cache_K0();
    for (bank_offset = 0; bank_offset < CACHE_SIZE; bank_offset += CACHE_BANK) {
	bank_ptr = (cdata_t *)(MEMORY_ARENA_ADDR + bank_offset);
	flush_ptr = (cdata_t *)((unsigned)bank_ptr ^ CACHE_SIZE);
/* CACHE RAM DATA AND SPILL/FILL TESTS */
	for (test_index = 0; test_index < CACHE_WIDE; ++test_index) {
	    for (test_data = 1; test_data != 0; test_data <<= 1) {
/* Walking Data Ones tests */
		for (i=0; i<CACHE_WIDE; i++)
		    bank_ptr[i] = 0;			/* - clear across the data lines */
		bank_ptr[test_index] = test_data;	/* - write a single bit to "1" */
		for (i=0; i<CACHE_WIDE; i++)
		    if (i != test_index)		/* - verify the data */
			EXPECT(bank_ptr, i, 0, "d1");
		EXPECT(bank_ptr, test_index, test_data, "d2");
		flush_dummy = flush_ptr[test_index];	/* - force the data to memory */
		for (i=0; i<CACHE_WIDE; i++)
		    if (i != test_index)		/* - verify the data */
			EXPECT(bank_ptr, i, 0, "d3");
		EXPECT(bank_ptr, test_index, test_data, "d4");
/* Walking Data Zeros tests */
		for (i=0; i<CACHE_WIDE; i++)
		    bank_ptr[i] = ~0;			/* - fill across the data lines */
		bank_ptr[test_index] = ~test_data;	/* - write a single bit to "0" */
		for (i=0; i<CACHE_WIDE; i++)
		    if (i != test_index)		/* - verify the data */
			EXPECT(bank_ptr, i, ~0, "d5");
		EXPECT(bank_ptr, test_index, ~test_data, "d6");
		flush_dummy = flush_ptr[test_index];	/* - force the data to memory */
		for (i=0; i<CACHE_WIDE; i++)
		    if (i != test_index)		/* - verify the data */
			EXPECT(bank_ptr, i, ~0, "d7");
		EXPECT(bank_ptr, test_index, ~test_data, "d8");
	    }
	}
/* CACHE RAM BANK ADDRESSING TESTS */
/* walk a "1" across the index lines, writing distinct data */
	test_data = 0x10;
	test_index = 0;
	for (i=0; i<CACHE_WIDE; ++i)
	    bank_ptr[test_index+i] = test_data++;
	for (test_index = CACHE_WIDE;
	     test_index < BANK_MAXIX;
	     test_index <<= 1) {
	    for (i=0; i<CACHE_WIDE; ++i)
		bank_ptr[test_index+i] = test_data++;
	}
/* Verify that the data looks OK */
	test_data = 0x10;
	test_index = 0;
	for (i=0; i<CACHE_WIDE; ++i)
	    EXPECT(bank_ptr, test_index+i, test_data++, "a1");
	for (test_index = CACHE_WIDE;
	     test_index < BANK_MAXIX;
	     test_index <<= 1) {
	    for (i=0; i<CACHE_WIDE; ++i)
		EXPECT(bank_ptr, test_index+i, test_data++, "a2");
	}
/* walk a "0" across the index lines, writing distinct data */
	test_data = 0x10;
	for (test_index = CACHE_WIDE;
	     test_index < BANK_MAXIX;
	     test_index <<= 1) {
	    for (i=0; i<CACHE_WIDE; ++i)
		bank_ptr[BANK_MAXIX-test_index+i] = test_data++;
	}
	test_index = BANK_MAXIX;
	for (i=0; i<CACHE_WIDE; ++i)
	    bank_ptr[BANK_MAXIX-test_index+i] = test_data++;
/* Verify that the data looks OK */
	test_data = 0x10;
	for (test_index = CACHE_WIDE;
	     test_index < BANK_MAXIX;
	     test_index <<= 1) {
	    for (i=0; i<CACHE_WIDE; ++i)
		EXPECT(bank_ptr, BANK_MAXIX-test_index+i, test_data++, "a3");
	}
	test_index = BANK_MAXIX;
	for (i=0; i<CACHE_WIDE; ++i)
	    EXPECT(bank_ptr, BANK_MAXIX-test_index+i, test_data++, "a4");
    }

/* CACHE RAM INTER-BANK ADDRESSING TESTS */
    bank_ptr = (cdata_t *)(MEMORY_ARENA_ADDR);
/* write distinct data at the start of each bank */
    test_data = 0x10;
    for (test_index = 0; test_index < CACHE_MAXIX; test_index += BANK_MAXIX)
	for (i=0; i<CACHE_WIDE; ++i)
	    bank_ptr[test_index+i] = test_data++;
/* Verify that the data looks OK */
    test_data = 0x10;
    for (test_index = 0; test_index < CACHE_MAXIX; test_index += BANK_MAXIX)
	for (i=0; i<CACHE_WIDE; ++i)
	    EXPECT(bank_ptr, test_index+i, test_data++, "a5");

    puts("9");
    /*cache_init();*/
    flush_cache2();

    /*uncache_K0();*/
    puts("a");
    return(0);
}
