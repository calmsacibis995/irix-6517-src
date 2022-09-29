#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/rtdisk/RCS/scsi.c,v 1.4 1996/04/23 01:21:33 doucette Exp $"

/*
 * ggd/scsi.c
 * 
 * This file contains routines which issue scsi mode sense and mode
 * select commands in order to enable and disable scsi disk drive
 * level retry mechanisms.
 *
 * This file uses unprotected global data. It is not multithreaded.
 *
 *
 */
#include <bstring.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dvh.h>
#include <sys/scsi.h>  
#include <sys/dkio.h>  
#include <sys/dksc.h>  

extern int errno;

/* 
 * SENSE_LEN is length of additional data plus 4 byte sense header
 * plus 8 byte block descriptor + 2 byte page header 
 */
#define SENSE_LEN_ADD (8+4+2)

/*
 * lengths of each page of mode select sense,
 * len of 0 means page not supported.
 */
u_char	pglengths[ ALL + 1];
/*
 * masks for changable page bits
 */
struct  common changeable[ ALL + 1];
/*
 * masks for current page bits
 */
struct  common currentbits[ ALL + 1];


#if 0
/*      
 * Used to mask off bits that aren't changeable.  Some drives are
 * not tolerant of attempts to change them, even if they are the
 * same as the current/default value.
 *
 * NOTE: this could be a problem, since some drives want to see
 * 0 for non-changeable bits, and others want to see the current
 * bits.  Neither SCSI1 nor SCSI2 are really quite clear what to
 * do about this....   We have prevailed on most of our suppliers
 * to always accept the current values, even if the bits are NOT
 * changeable.
 */
static void
maskch(u_char *set, u_char *change, u_char *cur, u_char len)
{
	register u_char i;

	for(i=0; i < len; i++) {
		*set &= *change;
		*set |= ~(*change) & *cur;
		set++, change++, cur++;
	}
}
#endif


/* 
 * Issue scsi device mode sense command.
 *
 *
 * RETURNS:
 *	0 on success
 *	non 0 on failure
 */
int
scsi_modesense( int fd, char *addr, int len, u_char pgcode)
{
	int			ret;
	struct dk_ioctl_data	modesense_data;

	modesense_data.i_page 	= pgcode;
	modesense_data.i_addr 	= (caddr_t)addr;
	modesense_data.i_len	= (uint_t)(len + SENSE_LEN_ADD);

	if ( modesense_data.i_len > 0xff ) {
		/*
		 * only 1 byte for length
		 */
		modesense_data.i_len = 0xff;
	}

	ret = ioctl( fd, DIOCSENSE, &modesense_data );

#ifdef DEBUG
	if (ret != 0 ) {
		printf("DIOCSENSE FAILED with %d, error %d \n",ret, errno);
	}
#endif
	
	return( ret );
}

#if 0
/*
 * Issue scsi device mode select command.
 * 
 * 
 * RETURNS:
 * 	0 on success
 * 	non 0 on failure.
 */
static int
scsi_modeselect( int fd, char *addr)
{
	struct mode_sense_data	*msaddr;
	struct dk_ioctl_data	modeselect_data;
	int			ret;
	u_char			*pgcode;

	msaddr = (struct mode_sense_data *)addr;

	pgcode = &(( u_char *)addr)[offsetof(struct mode_sense_data,dk_pages)+
			offsetof( struct common, pg_code)];
	
	msaddr->dpofua = 0;
	if (msaddr->bd_len == 0) {
		pgcode -= sizeof(msaddr->block_descrip);
	}
	*pgcode &= ALL;

	/* 
	 * the ERR_RECOV page should be of nonzero length.
	 */
	if (pglengths[ *pgcode ] == 0) {
#ifdef DEBUG
		printf("err_recov mode page has length zero \n"); 
#endif
		return( -1 );
	}
	
	/*
	 * setup mode select parameters.
	 */
	modeselect_data.i_len  = (uint_t)(msaddr->sense_len + 1);
#ifdef NOTDEF
	assert( modeselec_data.i_len == (pglengths[*pgcode] + 4 + 2) );
#endif
	modeselect_data.i_addr = (caddr_t)msaddr;

	/*
	 * clear reserved fields.
	 */
	msaddr->reserv0 = msaddr->reserv1 = msaddr->sense_len = 0;
	msaddr->mediatype = msaddr->wprot = msaddr->dpofua = 0;

	/*
	 * fix bits that are not changable, 
	 * set comments in maskch() routine.
	 */
	maskch( msaddr->dk_pages.common.pg_maxlen, 
		changeable[*pgcode].pg_maxlen,
		currentbits[*pgcode].pg_maxlen,
		pglengths[*pgcode]);

	ret = ioctl(fd, DIOCSELECT, &modeselect_data);

	return( ret );
}
#endif

/*
 * scsi_setup()
 *	Obtain the scsi mode sense data from the given disk device and
 *	store it in the global pglengths[], currentbits[], and changeable[] 
 *	arrays.
 *
 *  RETURNS:
 *	0 on success
 *	non 0 on errors
 */
/* ARGSUSED */
int
scsi_setup( int fd, char *diskname )
{
	struct mode_sense_data	data;
	u_char			*d = (u_char *)&data;
	int			i, len, maxd, pgnum, ret;

	len = sizeof(data) - (1+SENSE_LEN_ADD);
	bzero( &data, sizeof(data) );
	bzero( pglengths, sizeof(pglengths) );
	bzero( changeable, sizeof(changeable) );
	bzero( currentbits, sizeof(currentbits) );

	ret = scsi_modesense( fd, (char *)(&data), len, ALL|CURRENT);
	if ( ret != 0 ) {
#ifdef DEBUG
		printf("Could not get scsi sense information for %s.\n",
			diskname);
#endif
		return( ret );
	}

	maxd = data.sense_len + 1;

	if (data.bd_len > 7) {
		i = 4 + data.bd_len;
	} else {
		i = 4;
	}

	/*
	 * fill in scsi mode sense page length info.
 	 */
	while ( i < maxd ) {
		pgnum  = d[i] & ALL;
		pglengths[pgnum] = d[i + 1];
		i += pglengths[pgnum] + 2; 
		
	}

	/*
 	 * get info for changeable/current bits.
	 * see comment at routine maskch() for more info.
	 */
	ret = 0;
	for (i = 0; i < ALL; i++) {
		if ( (len = (int)pglengths[i]) != 0 ) {
			ret = scsi_modesense( fd, (char *)&data,
				len, (uchar_t)(i|CHANGEABLE));
			if ( ret == 0) {
				bcopy(  &data.dk_pages.common,
					&changeable[i],
					len);
			} else {
#ifdef DEBUG
				printf("Cannot get scsi sense change bits for %s\n",
					diskname);
#endif
				ret = -1;
			}

			ret = scsi_modesense( fd, (char *)&data, 
				len, (uchar_t)(i|CURRENT));
			if ( ret == 0) {
				bcopy(  &data.dk_pages.common,
					&currentbits[i],
					len);
			} else {
#ifdef DEBUG
				printf("Cannot get scsi sense current bits for %s\n",diskname);
#endif
				ret = -1;
			}
		}
	}
	return ( ret );
}


/*
 * scsi_set_disk_to_no_retry()
 *	Disable scsi disk retry mechanism on the given drive.
 *	This routine sets the E_RC (read continuous) bit in the
 *	scsi read/write error recovery page. It prevents the drive 
 *	from performing any actions to recover from disk error. 
 *
 *	NOTE:
 *	WITH THE E_RC BIT SET, THE DISK DRIVE CAN SILENTLY RETURN
 *	CORRUPTED DATA. THE USER MUST BEWARE THAT THE DATA READ FROM
 *	A REALTIME DEVICE IS NOT 100% RELIABLE.
 *
 *	I have only seen corrupted data returned shortly after the disk
 *	has performed a very long seek. Even in this case, corrupted data
 * 	is seldom returned.
 *
 * RETURNS:
 *	0 on success
 *	non 0 on errors;
 */
/* ARGSUSED */
int
scsi_set_disk_to_no_retry( int fd, char *diskname)
{
/*
 * disable this feature until it can be determined if it works on seagate drives
 */
#if 0
	struct mode_sense_data	err_params_sen;
	struct mode_sense_data	err_params_sel;
	struct err_recov	*err_page;
	int			len, ret;

	ret = scsi_setup( fd, diskname );
	if (ret != 0) {
		return( ret );
	}

	bzero( &err_params_sen, sizeof(err_params_sen) );
	bzero( &err_params_sel, sizeof(err_params_sel) );
	
	len = (int)pglengths[ERR_RECOV];
	ret = scsi_modesense( fd, (char*)(&err_params_sen),
			len, ERR_RECOV|CURRENT);
	if ( ret != 0 ) {
#ifdef DEBUG
		printf("Could not get scsi error sense data for %s\n",
			diskname);
#endif
		return ( ret );
	}

	err_page = (struct err_recov *)
			(err_params_sen.block_descrip + 
			(ulong_t)err_params_sen.bd_len);

#ifdef DEBUG
	printf("Error page bits: %x \n", err_page->e_err_bits);
	printf("E_DCR	= %x \n", (err_page->e_err_bits & E_DCR)  >> 0 );
	printf("E_DTE	= %x \n", (err_page->e_err_bits & E_DTE)  >> 1 );
	printf("E_PER	= %x \n", (err_page->e_err_bits & E_PER)  >> 2 );
	printf("E_RC	= %x \n", (err_page->e_err_bits & E_RC)   >> 4 );
	printf("E_TB	= %x \n", (err_page->e_err_bits & E_TB)   >> 5 );
	printf("E_ARRE	= %x \n", (err_page->e_err_bits & E_ARRE) >> 6 );
	printf("E_AWRE	= %x \n", (err_page->e_err_bits & E_AWRE) >> 7 );
#endif

	/*
 	 * CHANGE BITS to disable drive level retries
 	 */
	err_page->e_err_bits |= E_RC;

#ifdef NOTYET
	err_page->e_err_bits |= E_EER;
	err_page->e_err_bits |= E_PER;
	err_page->e_err_bits |= E_DTE;
	err_page->e_err_bits &= ~E_DCR;
#endif

	err_page->e_err_bits &= ~E_ARRE;
	err_page->e_err_bits &= ~E_AWRE;

	bcopy(  err_page, 
		&err_params_sel.dk_pages.err_recov,
		pglengths[ERR_RECOV] + 4 + 1);
	err_params_sel.bd_len = 8;
	err_params_sel.sense_len = (uchar_t)(13 + pglengths[ERR_RECOV]);

	ret = scsi_modeselect( fd, (char *)(&err_params_sel));

	if (ret != 0 ) {
#ifdef DEBUG
		printf("Scsi mode select failed for %s\n", diskname);
#endif
		return( ret ) ;
	}

	bzero( &err_params_sen, sizeof(err_params_sen) );
	
	len = (int)pglengths[ERR_RECOV];
	ret = scsi_modesense( fd, (char*)(&err_params_sen),
		len, ERR_RECOV|CURRENT);
	if ( ret != 0 ) {
#ifdef DEBUG
		printf("Could not get scsi error sense data for %s\n",
			diskname);
#endif
		return ( ret );
	}

	err_page = (struct err_recov *)
			(err_params_sen.block_descrip + 
			(ulong_t)err_params_sen.bd_len);

#ifdef DEBUG
	printf("Error page bits: %x \n", err_page->e_err_bits);
	printf("E_DCR	= %x \n", (err_page->e_err_bits & E_DCR)  >> 0 );
	printf("E_DTE	= %x \n", (err_page->e_err_bits & E_DTE)  >> 1 );
	printf("E_PER	= %x \n", (err_page->e_err_bits & E_PER)  >> 2 );
	printf("E_RC	= %x \n", (err_page->e_err_bits & E_RC)   >> 4 );
	printf("E_TB	= %x \n", (err_page->e_err_bits & E_TB)   >> 5 );
	printf("E_ARRE	= %x \n", (err_page->e_err_bits & E_ARRE) >> 6 );
	printf("E_AWRE	= %x \n", (err_page->e_err_bits & E_AWRE) >> 7 );
#endif

#endif
	return( 0 );
}

/*
 * scsi_set_disk_to_retry()
 *
 *	Enable disk retry mechanism on the given drive.
 *	This routine clears the E_RC (read continuous) bit
 *	in the scsi read/write error recovery page. It causes 
 *	the driver to try and recover data after a disk error.
 *	It also causes the drive to return errors to the host in cause
 *	the data cannot be recovered.
 *
 * RETURNS:
 *	0 on success
 *	non 0 on error
 */
/* ARGSUSED */
int
scsi_set_disk_to_retry( int fd, char * diskname )
{
/*
 * disable this feature until it can be determined if it works on seagate drives
 */
#if 0
	struct mode_sense_data	err_params_sen;
	struct mode_sense_data	err_params_sel;
	struct err_recov	*err_page;
	int			len, ret;

	ret = scsi_setup( fd, diskname );
	if ( ret != 0 ) {
		return( ret );
	}

	bzero( &err_params_sen, sizeof(err_params_sen) );
	bzero( &err_params_sel, sizeof(err_params_sel) );
	
	len = (int)pglengths[ERR_RECOV];
	ret = scsi_modesense( fd, (char*)(&err_params_sen),
		len, ERR_RECOV|CURRENT);
	if ( ret != 0 ) {
#ifdef DEBUG
		printf("Could not get scsi error sense data for %s\n",
			diskname);
#endif
		return ( ret );
	}

	err_page = (struct err_recov *)
			(err_params_sen.block_descrip + 
			(ulong_t)err_params_sen.bd_len);

#ifdef DEBUG
	printf("Error page bits before change: %x \n", err_page->e_err_bits);
	printf("E_DCR	= %x \n", (err_page->e_err_bits & E_DCR)  >> 0 );
	printf("E_DTE	= %x \n", (err_page->e_err_bits & E_DTE)  >> 1 );
	printf("E_PER	= %x \n", (err_page->e_err_bits & E_PER)  >> 2 );
	printf("E_RC	= %x \n", (err_page->e_err_bits & E_RC)   >> 4 );
	printf("E_TB	= %x \n", (err_page->e_err_bits & E_TB)   >> 5 );
	printf("E_ARRE	= %x \n", (err_page->e_err_bits & E_ARRE) >> 6 );
	printf("E_AWRE	= %x \n", (err_page->e_err_bits & E_AWRE) >> 7 );
#endif

	/*
 	 * CHANGE BITS to enable disk retries
 	 */
	err_page->e_err_bits &= ~E_RC;

#ifdef NOTYET
	err_page->e_err_bits &= ~E_EER;
	err_page->e_err_bits &= ~E_PER;
	err_page->e_err_bits &= ~E_DTE;
	err_page->e_err_bits &= ~E_DCR;
#endif

	err_page->e_err_bits |= E_ARRE;
	err_page->e_err_bits |= E_AWRE;

	bcopy(  err_page, 
		&err_params_sel.dk_pages.err_recov,
		pglengths[ERR_RECOV] + 4 + 1);
	err_params_sel.bd_len = 8;
	err_params_sel.sense_len = (uchar_t)(13 + pglengths[ERR_RECOV]);

	ret = scsi_modeselect( fd, (char *)(&err_params_sel));

	if (ret != 0 ) {
#ifdef DEBUG
		printf("Scsi mode select failed for %s\n",diskname);
#endif
		return( ret );
	}

	bzero( &err_params_sen, sizeof(err_params_sen) );
	
	len = (int)pglengths[ERR_RECOV];
	ret = scsi_modesense( fd, (char*)(&err_params_sen),
		len, ERR_RECOV|CURRENT);
	if ( ret != 0 ) {
#ifdef DEBUG
		printf("Could not get scsi error sense data for %s\n",	
			diskname);
#endif
		return ( ret );
	}

	err_page = (struct err_recov *)
			(err_params_sen.block_descrip + 
			(ulong_t)err_params_sen.bd_len);

#ifdef DEBUG
	printf("Error page bits after change: %x \n", err_page->e_err_bits);
	printf("E_DCR	= %x \n", (err_page->e_err_bits & E_DCR)  >> 0 );
	printf("E_DTE	= %x \n", (err_page->e_err_bits & E_DTE)  >> 1 );
	printf("E_PER	= %x \n", (err_page->e_err_bits & E_PER)  >> 2 );
	printf("E_RC	= %x \n", (err_page->e_err_bits & E_RC)   >> 4 );
	printf("E_TB	= %x \n", (err_page->e_err_bits & E_TB)   >> 5 );
	printf("E_ARRE	= %x \n", (err_page->e_err_bits & E_ARRE) >> 6 );
	printf("E_AWRE	= %x \n", (err_page->e_err_bits & E_AWRE) >> 7 );
#endif

#endif
	return( 0 );
}
