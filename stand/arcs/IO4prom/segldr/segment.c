#include <sys/types.h>
#include <sys/EVEREST/promhdr.h>
#include <sys/sbd.h>
#include <sys/EVEREST/evconfig.h>
#include "sl.h"
#include "lzw.h"

struct prid_to_segtype {
	int prid;
	int boardtype;
	int segtype;
	char *name;
};

const struct prid_to_segtype p2s[] = {
	{0x400,		EVTYPE_IP19,	STYPE_R4000,	"R4400 IP19"},
	{0x1000,	EVTYPE_IP21,	STYPE_TFP,	"R8000 IP21"},
	{0x900,		EVTYPE_IP25,    STYPE_R10000, 	"R10000 IP25"},
	{0,		0,		0,		(char *)0}
};

struct segnames {
	int segtype;
	char *name;
};

const struct segnames sn[] = {
	{STYPE_R4000,		"R4400"},
	{STYPE_TFP,		"R8000"},
	{STYPE_R10000,		"R10000"},
	{STYPE_MASTER,		"MASTER"},
	{STYPE_GFX,		"Graphics"},
	{-1,			"Unknown"},
	{0,			(char *)0}
};
	

int
which_segtype(int prid, int boardtype)
{
	int i;

	for (i = 0; p2s[i].prid != 0; i++) {
		if ((prid == p2s[i].prid) &&
		   (!boardtype || (boardtype == p2s[i].boardtype)))
			return p2s[i].segtype;
	}

	return -1;
}

char *
which_segname(int prid, int boardtype)
{
	int i;

	for (i = 0; p2s[i].prid != 0; i++) {
		if ((prid == p2s[i].prid) &&
		   (!boardtype || (boardtype == p2s[i].boardtype)))
			return p2s[i].name;
	}

	return "Unknown";
}


int
which_segnum(seginfo_t *si, int seg_type)
{
	int i;

	for (i = 0; i < si->si_numsegs; i++) {
		if ((STYPE_MASK & si->si_segs[i].seg_type) == seg_type)
			return i;
	}

	/* If we get here, we couldn't find the right type. */
	return -1;
}


char *
seg_string(int segtype)
{
	int i;

	for (i = 0; sn[i].name != (char *)0; i++) {
		if ((segtype & STYPE_MASK) == sn[i].segtype)
			return sn[i].name;
	}

	return "NOT FOUND";
}


int
get_segtable(seginfo_t *si, int window)
{
	copy_prom(SEGINFO_OFFSET, si, sizeof(seginfo_t), window);
	if (si->si_magic == SI_MAGIC)
		return 1;
	else
		return 0;
}


int
get_promhdr(evpromhdr_t *ph, int window)
{
	copy_prom(0, ph, sizeof(evpromhdr_t), window);
	if ((ph->prom_magic == PROM_MAGIC) || ((ph->prom_magic == PROM_NEWMAGIC)))
		return 1;
	else
		return 0;
}


int
lzw_decompress(char *buffer, promseg_t *ps)
{
    prom_lzw_hdr_t *lzw;

    in_buf = buffer;
    out_buf = (char *)ps->seg_startaddr;
    in_size = (int)ps->seg_length;
    in_next = sizeof(prom_lzw_hdr_t) + 3;
    out_next = 0;
    out_checksum = 0;

    lzw = (prom_lzw_hdr_t *)buffer;
    in_size -= lzw->lzw_padding;

    if (lzw->lzw_magic != LZW_MAGIC) {
	loprintf("ERROR: No LZW magic number! (Is 0x%x, should be 0x%x)\n",
		lzw->lzw_magic, LZW_MAGIC);
	return 0;
    }

    out_size = lzw->lzw_real_length + 1024;

    setup_compress();

    decompress();

    if (out_next != lzw->lzw_real_length) {
	loprintf("ERROR: Decompresed length (0x%x) != stored length (0x%x)\n",
		out_next, lzw->lzw_real_length);
	return 0;
    }

    if (out_checksum != lzw->lzw_real_cksum) {
	loprintf("ERROR: Decompressed checksum (0x%x) != stored cksum (0x%x)\n",
		out_checksum, lzw->lzw_real_cksum);
	return 0;
    }

    return 1;
}


int
load_segment(promseg_t *ps, int window)
{
	__uint32_t cksum = 0;
 	evpromhdr_t ph;
	char *buffer = (char *)FLASHBUF_BASE;

	if (!ps->seg_startaddr && !ps->seg_length &&
	     (ps->seg_type == SEGTYPE_MASTER)) {
		loprintf("NOTE: Converting obsolete prom image.\n");
		if (!get_promhdr(&ph, window)) {
		    loprintf("ERROR: Can't get prom header for obsolete segment.\n");
		    return 0;
		}
		
		ps->seg_offset = PROMDATA_OFFSET;
		ps->seg_entry = (__int64_t)ph.prom_entry;
		ps->seg_startaddr = (__int64_t)ph.prom_startaddr;
		ps->seg_length = ph.prom_length;
		ps->seg_cksum = ph.prom_cksum;
	}	

	if ((ps->seg_type & SFLAG_COMPMASK) == SFLAG_UNCOMPRESSED) {
		cksum = copy_prom(ps->seg_offset, (void *)ps->seg_startaddr,
			  ps->seg_length, window);
		if (cksum != ps->seg_cksum) {
			loprintf("Checksums don't match!\n");
			loprintf("computed 0x%x, stored 0x%x\n",
							cksum, ps->seg_cksum);
			return 0;
		}
	} else {
		cksum = copy_prom(ps->seg_offset, buffer, ps->seg_length,
				  window);
		if (cksum != ps->seg_cksum) {
			loprintf("Compressed data checksums don't match!\n");
			loprintf("computed 0x%x, stored 0x%x\n",
							cksum, ps->seg_cksum);
			return 0;
		}
		if ((ps->seg_type & SFLAG_COMPMASK) == SFLAG_LZW) {
			if (!lzw_decompress(buffer, ps)) {
				loprintf("Decompression failed!\n");
				return 0;
			}
		} else {
			loprintf("Unsupported compression type!\n");
			return 0;
		}
	}

	return 1;
}

int
load_and_exec(promseg_t *ps, int window)
{
	if (load_segment(ps, window)) {
		if (ps->seg_entry < ps->seg_startaddr ||
		    ps->seg_entry > ps->seg_startaddr + ps->seg_length) {
			loprintf("Segment entry point out of range!\n");
		 } else {
		     jump_addr((void *)ps->seg_entry);
		     /* NOTREACHED */
		 }
	} else {
		loprintf("Segment load failed.  Can't execute image.\n");
	}
	return 0;
}

void
list_segments(seginfo_t *si)
{
	int i;

	loprintf("Total number of segments: %d\n", si->si_numsegs);
	loprintf("#   Type   (Size)\n");
	for (i = 0; i < si->si_numsegs; i++) {
		loprintf("%b  %s (%d, 0x%x)\n", i,
			seg_string((int)si->si_segs[i].seg_type),
			si->si_segs[i].seg_length,
			si->si_segs[i].seg_length);
	}
}

int
get_window(int slot)
{
	evbrdinfo_t *brd;

	brd = &EVCFGINFO->ecfg_board[slot];

	if ((EVCLASS_MASK & brd->eb_type) != EVCLASS_IO) {
		loprintf("Slot 0x%a doesn't contain an IO board.\n", slot);
		return -1;
	}

	return brd->eb_io.eb_winnum;
}
