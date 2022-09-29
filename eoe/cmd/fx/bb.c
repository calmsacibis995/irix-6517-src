#include "fx.h"
#include <sys/smfd.h>



/*
 * menu item to add to the badblock list
 */
void
addbb_func(void)
{
	daddr_t bn, lastbn;
	int hadargs;

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no badblock forwarding on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	lastbn = DP(&vh)->dp_drivecap;
	hadargs = !noargs();
	while( !hadargs || !noargs() ) {
	    argbn(&bn, (daddr_t)-1, lastbn, "add badblock");
	    /* SCSI doesn't try to to preserve block contents when
	     * sparing from the exercising code, since there have already
	     * been so many retries (by firmware and driver).  For the
	     * cases where someone accidentally spares the wrong block,
	     * or is sparing a block with a soft error, we give it a
	     * try here. */
	    if(drivernum == SCSI_NUMBER) {
		u_char *sbuf = malloc(DP(&vh)->dp_secbytes);
		int nodata = 1;
		if(!sbuf)
		    printf("Can't allocate memory for saving data,"
			   "data not saved\n");
		else {
		    printf("trying to save the data at block %lld\n", bn);
		    nodata = gread(bn, sbuf, DP(&vh)->dp_secbytes/BBSIZE);
		    if(nodata)
			scerrwarn("couldn't read data to save it\n");
		}
		(void)addibb(bn);
		if(sbuf) {
		    if(!nodata) {
			if(gwrite(bn, sbuf, DP(&vh)->dp_secbytes/BBSIZE))
			    scerrwarn("rewrite of saved data failed\n");
			else
			    printf("rewrote saved data OK\n");
		    }
		    free(sbuf);
		}
	    }
	    else
		(void)addibb(bn);
	}
}




/*
 * menu item to print the badblock list
 * sort then dump the badblock list
 */
void
showbb_func(void)
{
	char flags[6];
	int typ, list;

	switch (drivernum)
	{

	case SCSI_NUMBER:
		checkflags("lbcfmg", flags);
		if((flags[0] + flags[1] + flags[2]) > 1)
		{
			printf("can't specify more than one of -l, -c, and -b\n");
			break;
		}
		else if(flags[0] || !(flags[0] + flags[1] + flags[2]))
			typ = 1;	/* logical */
		else if(flags[1])
			typ = -1;
		else if(flags[2])
			typ = 0;
		if(flags[3])
			list = 3;	/* full list */
		if(flags[4])
			list = 1; /* mfg only */
		if(flags[5] || !(flags[3]+flags[4]+flags[5]))
			list = 2; /* grown only */
		if(print_scsidefects(typ, list) && (typ == 1 || typ == -1))
			/* failed for this type.  Probably a drive that doesn't support
			 * logical, like IBM DDRS-39130W, so fall back to cyl if so.
			 * bug #554444. */
				(void)print_scsidefects(0, list);
		break;

#ifdef SMFD_NUMBER
	case SMFD_NUMBER:
		printf("no badblock lists on this drive type\n");
		break;
#endif /* SMFD_NUMBER */

	default:
		printf("Don't know how to show badblocks for driver type %u\n",
		    drivernum);
		break;
	}
}


/*
 * add to the badblock list.
 */
int
addibb(daddr_t bn)
{
	switch (drivernum)
	{

#ifdef SMFD_NUMBER
	case SMFD_NUMBER:
		printf("can't add bad blocks on floppy\n");
		return -1;
#endif /* SMFD_NUMBER */

	case SCSI_NUMBER:
		printf("...adding bad block %llu\n", bn);
		if (scsi_addbb(bn) < 0)
		{
			errwarn("Failed to add bad block");
			return -1;
		}
		break;

	default:
		printf("Don't know how to add bad blocks for driver type %u\n",
		    drivernum);
		return -1;
	}

	logmsg("added bad block %u\n", bn, 0, 0);
	return 0;
}


/*
 * find the end of the area which may be exercised, given a starting
 * block number.  if starting outside of the replacement partition, stay
 * outside of it.  if inside, stay away from the next in-use replacement
 * track And the next savedefect track.
 */
daddr_t
exarea(daddr_t startbn)
{
	int i;
	uint cap = 0;	/* zero in case of errors */

	switch(drivernum) {
#ifdef SMFD_NUMBER
	case SMFD_NUMBER:
		{
			scsi_readcapacity(&cap);
			if(startbn > cap)
				return 0;
			i = cap;
			break;
		}
#endif /* SMFD_NUMBER */
	case SCSI_NUMBER:
		/*	use lesser of capacity and end of drive; for drives
			with zone-bit-recording, or drives that have
			spares allocated on a per zone basis where a zone is
			more than one track. */
		scsi_readcapacity(&cap);

		if(cap > PT(&vh)[PTNUM_VOLUME].pt_nblks)
			cap = PT(&vh)[PTNUM_VOLUME].pt_nblks;
		i = cap;
		break;

	}
	return i - startbn;
}

void
fatal_mapbanner(void)
{
	banner("WARNING!");
	banner("WARNING!");
	errno = 0;
	scerrwarn("THIS DISK CONTAINS AT LEAST ONE BAD BLOCK WHICH CANNOT BE MAPPED OUT.\n"
		"        THIS DISK SHOULD NOT BE USED!\n");
}
