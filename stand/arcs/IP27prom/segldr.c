#include <sys/types.h>

#include <libkl.h>

#include "segldr.h"
#include "libc.h"
#include "unzip.h"
#include "libasm.h"

int
segldr_check(promhdr_t *ph)
{
#if 0
    printf("PROM header contains:\n");
    printf("  Magic:    0x%lx\n", ph->magic);
    printf("  Version:  0x%lx\n", ph->version);
    printf("  Length:   0x%lx\n", ph->length);
    printf("  Segments: %ld\n", ph->numsegs);
#endif

    if (ph->magic != PROM_MAGIC) {
	printf("PROM magic incorrect: %y\n", ph->magic);
	return -1;
    }

    return 0;
}

int
segldr_list(promhdr_t *ph)
{
    int		i;

    if (ph->numsegs > 6) {
	printf("Corrupt segment count in segldr header (%d)\n", ph->numsegs);
	return -1;
    }

    for (i = 0; i < ph->numsegs; i++) {
	promseg_t *seg = &ph->segs[i];

	printf("Segment %d: %s\n", i, seg->name);
	printf("  Flags:       0x%lx\n", seg->flags);
	printf("  Offset:      0x%lx\n", seg->offset);
	printf("  Entry addr:  0x%lx\n", seg->entry);
	printf("  Load addr:   0x%lx\n", seg->loadaddr);
	printf("  True len:    0x%lx\n", seg->length);
	printf("  True sum:    0x%lx\n", seg->sum);

	if (seg->flags & SFLAG_COMPMASK) {
	    printf("  Cmprsd len:  0x%lx\n", seg->length_c);
	    printf("  Cmprsd sum:  0x%lx\n", seg->sum_c);
	}

	printf("  Memory len:  0x%lx\n", seg->memlength);
    }

    return 0;
}

int
segldr_lookup(promhdr_t *ph, char *name)
{
    int		i;

    for (i = 0; i < ph->numsegs; i++)
	if (strcmp(ph->segs[i].name, name) == 0)
	    return i;

    return -1;
}

static __uint64_t      *r_ptr;	/* Pointer to data as longs             */
static __uint64_t	r_data;	/* Current 'long'                       */
static int		r_left;	/* Bytes remaining in current 'long'    */
static char	       *w_ptr;	/* Pointer to data being written to RAM */

static int
gzip_getc(void)			/* Can only fetch doubles from PROM     */
{
    int		c;

    if (r_left == 0) {
	r_data = *r_ptr++;
	r_left = 8;
    }

    c	= (int) (r_data >> 56);

    r_left--;
    r_data <<= 8;

    return c;
}

static void
gzip_putc(int c)
{
    *w_ptr++ = c;
}

int
segldr_load(promhdr_t *ph, int segnum,
	    __uint64_t alt_data, __uint64_t alt_ldaddr,
	    __uint64_t tmpbuf, __uint64_t tmpsize)
{
    int		r;
    promseg_t  *seg	= &ph->segs[segnum];
    __uint64_t	sum;
    __uint64_t	loadaddr;
    __uint64_t	dataaddr;

    if (alt_data)   dataaddr = alt_data;
    else	    dataaddr = (__uint64_t) ph + seg->offset;

    if (alt_ldaddr) loadaddr = alt_ldaddr;
    else	    loadaddr = seg->loadaddr;

    switch (seg->flags & SFLAG_COMPMASK) {

    case SFLAG_NONE:

	memcpy((void *) loadaddr,
	       (char *) dataaddr,
	       (seg->length + 7) & ~7);

	break;

    case SFLAG_GZIP:

	r_ptr = (__uint64_t *) dataaddr;
	r_left = 0;
	w_ptr = (char *) loadaddr;

	r = unzip(gzip_getc, gzip_putc, (char *) tmpbuf, (int) tmpsize);

	if (r < 0) {
	    printf("Decompress failed: %s\n", unzip_errmsg(r));
	    return -1;
	}

	break;

    default:

	printf("*** Unknown compression method: %d\n",
	       seg->flags & SFLAG_COMPMASK);
	return -1;
    }

    sum = memsum((unsigned char *) loadaddr, seg->length);

    if (sum != seg->sum) {
	printf("*** Checksum mismatch after copying segment\n");
	printf("*** Expected: %lx, Actual: %lx\n", seg->sum, sum);

	return -1;
    }

    return 0;
}

/*
 * segldr_jump
 *
 *   Jumps to the entry point, and switches the stack pointer to sp
 *   unless sp is NULL, in which case the stack pointer is left alone.
 */

int
segldr_jump(promhdr_t *ph, int segnum, __uint64_t alt_entry,
				       __uint64_t arg,
				       __uint64_t sp)
{
    promseg_t  *seg	= &ph->segs[segnum];
    __uint64_t	entry;

    if (alt_entry)
	entry = alt_entry;
    else {
	if (seg->entry < seg->loadaddr ||
	    seg->entry >= seg->loadaddr + seg->length) {
	    printf("Segment entry point out of range!\n");
	    return -1;
	}

	entry = seg->entry;
    }

    db_printf("Jumping to entry point 0x%lx\n\n", entry);

    jump_inval((__uint64_t) entry, arg, 0,
	       JINV_DCACHE | JINV_SCACHE,
	       sp);

    /* NOTREACHED */

    return 0;
}

/*
 * Update the flags of the IOC3 if we are loading an old IO prom.
 * New CPU proms dont set these by default.
 */

segldr_old_ioprom(promhdr_t *ph, promseg_t *seg)
{

    if ((!strncmp(seg->name, "io6prom", 7)) && (ph->version < 2)) {
        /* We are loading a old io6prom on a new IP27prom. Update 
           io prom data structs for this */
	printf("Running old IO6prom on newer IP27prom\n") ;
        update_old_ioprom(get_nasid()) ;
    }
    return 1 ;

}
