#ident	"$Revision: 1.6 $"

/*
 * is_vh.c -- volume header routines
 */

#include <sys/types.h>
#include <sys/dvh.h>
#include <libsc.h>

/*	compute volume header checksum.  */
int
vh_checksum(struct volume_header *vh)
{
	register int csum;
	register int *ip;

	csum = 0;
	for (ip = (int *)vh; ip < (int *)(vh + 1); ip++)
		csum += *ip;
	return (csum);
}

void
vh_list(struct volume_header *vh)
{
	register struct volume_directory *vd;

	for (vd = vh->vh_vd; vd < &vh->vh_vd[NVDIR]; vd++)
		if(vd->vd_nbytes>0 && vd->vd_lbn != -1)
		printf("\t%-8s %-7d bytes @ blk %-4d\n", vd->vd_name, vd->vd_nbytes, vd->vd_lbn);
}

void
dump_vh(struct volume_header *vh, char *head)
{
    int i;

    printf("volhdr struct (%s):\n", head);
    printf("vh: magic=%x root=%x swap=%x csum=%x, bfile=%s.\n",
	vh->vh_magic, vh->vh_rootpt, vh->vh_swappt, vh->vh_csum, vh->vh_bootfile);
    vh_list(vh);
    for(i=0; i<NPARTAB; i++)
	    if(vh->vh_pt[i].pt_nblks > 0)
		    printf("pnum %x: %x blks @ lbn=%x type=%x\n",
		      i, vh->vh_pt[i].pt_nblks,
		      vh->vh_pt[i].pt_firstlbn, vh->vh_pt[i].pt_type);
}

/*
 * is_vh -- decide if we're looking at a reasonable volume_header
 */
int
is_vh(struct volume_header *vhp)
{
	if (vhp->vh_magic != VHMAGIC)
		return(0);

	return vh_checksum(vhp) == 0;
}

/* used by new code in sash that changes partition table */
void
set_vhchksum(struct volume_header *vhp)
{
	vhp->vh_magic = VHMAGIC;
	vhp->vh_csum = 0;
	vhp->vh_csum = -vh_checksum(vhp);
}
