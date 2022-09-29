/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.3 $"
/*
 * Command to get the kernel's perspective on XLV volumes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/uuid.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_utils.h>

#include <sys/xlv_attr.h>

#define	ME	"xlv_show"

extern xlv_tab_subvol_t	*get_subvol_space(void);
extern int	free_subvol_space(xlv_tab_subvol_t *svp);

int sflag = 0;				/* subvolume enumeration */
int vflag = 0;				/* verbose */

int print_mode = 0;


static int
usage(void)
{
	fprintf(stderr, "usage: %s [-sv]\n", ME);
	fprintf(stderr, "-s  enumerate by subvolume\n");
	fprintf(stderr, "-v  verbose mode; print all information\n");
	exit(1);
}

/*
 * Enumerate the subvolumes.
 */
void
do_subvol(xlv_attr_cursor_t *cursor)
{
	xlv_tab_subvol_t	*svp;
	xlv_attr_req_t		req;
	int			status;
	int			count;

	if (NULL == (svp = get_subvol_space())) {
		printf("Cannot malloc a subvolume entry.\n");
		exit(1);
	}

	req.attr = XLV_ATTR_SUBVOL;
	req.ar_svp = svp;

	status = 0;
	count = 0;
	while (status == NULL) {
		status = syssgi(SGI_XLV_ATTR_GET, cursor, &req);
		if (status < 0) {
			if (ENFILE == oserror())
				/* end of enumeration */
				break;
			perror("syssgi(SGI_XLV_ATTR_GET) failed.");
		} else {
			if (count) printf("\n");
			xlv_tab_subvol_print(req.ar_svp, print_mode);
			count++;
		}
	}

	if (count == 1) {
		printf("\nThere is 1 subvolume.\n");
	} else {
		printf("\nThere are %d subvolumes.\n", count);
	}

} /* end of do_vol() */

/*
 * Enumerate the volumes.
 */
void
do_vol(xlv_attr_cursor_t *cursor)
{
	xlv_tab_vol_entry_t	vol;
	xlv_attr_req_t		req;
	int			status;
	int			count;

	if (NULL == (vol.log_subvol = get_subvol_space())) {
		printf("Cannot malloc a log subvolume entry.\n");
		exit(1);
	}
	if (NULL == (vol.data_subvol = get_subvol_space())) {
		printf("Cannot malloc a data subvolume entry.\n");
		exit(1);
	}
	if (NULL == (vol.rt_subvol = get_subvol_space())) {
		printf("Cannot malloc a rt subvolume entry.\n");
		exit(1);
	}

	req.attr = XLV_ATTR_VOL;
	req.ar_vp = &vol;

	status = 0;
	count = 0;
	while (status == NULL) {
		status = syssgi(SGI_XLV_ATTR_GET, cursor, &req);
		if (status < 0) {
			if (ENFILE == oserror())
				/* end of enumeration */
				break;
			perror("syssgi(SGI_XLV_ATTR_GET) failed.");
		} else {
			if (count) printf("\n");
			xlv_tab_vol_print(req.ar_vp, print_mode);
			count++;
		}
	}

	if (count == 1) {
		printf("\nThere is 1 volume.\n");
	} else {
		printf("\nThere are %d volumes.\n", count);
	}

} /* end of do_vol() */

int
main (int argc, char **argv)
{
	xlv_attr_cursor_t	cursor;
	int			ch;

#if 0
	if (getuid()) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(1);
	}
#endif

	while ((ch = getopt(argc, argv, "sv")) != EOF)
		switch((char)ch) {
		case 'v':
			vflag++;
			print_mode = XLV_PRINT_ALL;
			break;
		case 's':
			sflag++;
			break;
		default:
			usage();
		}

	if (argc -= optind)
		usage();

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		exit(1);
	}

	if (sflag) {
		do_subvol(&cursor);
	} else {
		do_vol(&cursor);
	}

	exit(0);
}
