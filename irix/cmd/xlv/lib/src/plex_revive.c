/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.13 $"

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <bstring.h>
#include <xlv_plexd.h>
#include <xlv_oref.h>
#include <xlv_labd.h>


extern xlv_block_map_t *xlv_tab_create_block_map(xlv_tab_subvol_t *sv);
extern void            slogit(int priority, char *fmt, ...);


static boolean_t need_to_revive (
	xlv_tab_subvol_t *xlv_p,
	__int64_t        *first_blkno,
	__int64_t        *last_blkno);


/*
 * Initiates a plex revive on a subvolume.
 * This routine will examine all the volume elements in a subvolume,
 * figure out which ones are stale, and then issue a plex revive
 * request to the xlv_plexd to revive the stale block range.
 *
 * Note that if there is already a plex revive in progress, this may
 * cause more blocks to be revived than necessary. It does not break
 * anything, but causes more I/O to be issued.
 */
int
xlv_tab_revive_subvolume (xlv_tab_subvol_t	*xlv_p)
{
	xlv_revive_rqst_t	revive_request;
	__int64_t		first_blkno, last_blkno;
	int			fd;

	if (XLV_SUBVOL_EXISTS(xlv_p)) {

		if (need_to_revive (xlv_p, &first_blkno, &last_blkno)) {

			xlv_init_revive_request (&revive_request);

			if (xlv_issue_revive_request (&revive_request, xlv_p,
				first_blkno, last_blkno) != 0) 
					return -1;

			xlv_end_revive_request(&revive_request);

			fd = open(XLV_LABD_FIFO, O_NDELAY|O_WRONLY);
			if (0 > fd) {
				/*
				 * There is no reader so xlv label daemon
				 * is not running. Start the daemon since
				 * there will be label updates from the
				 * kernel when volume elements go active.
				 */
				if (0 > system(PATH_XLV_LABD)) {
					perror("Cannot start xlv_labd");
				} else {
#ifdef DEBUG
					fprintf(stderr, "DBG: Auto-start %s\n",
						PATH_XLV_LABD);
#endif
					slogit(LOG_INFO, "Auto-start %s",
						PATH_XLV_LABD);

				}
			} else {
				close(fd);
			}
		}
	}
	return 0;
}


/*
 * Determines whether a subvolume needs to be revived.
 * If so, returns the range of disk blocks that are stale.
 */
static boolean_t
need_to_revive (
	xlv_tab_subvol_t	*xlv_p,
	__int64_t		*first_blkno,
	__int64_t		*last_blkno)
{
	xlv_block_map_t         *block_map;
	xlv_block_map_entry_t	*block_map_entry;
	int			i;
	__int64_t		min_stale_blkno, max_stale_blkno;
	__int64_t		start_blkno = 0;
	boolean_t		is_stale = B_FALSE;

	if (xlv_p->num_plexes == 1)
		return B_FALSE;

	block_map = xlv_tab_create_block_map (xlv_p);
	if (block_map == NULL)
		return B_FALSE;

	min_stale_blkno = xlv_p->subvol_size;
	max_stale_blkno = 0;

	for (i=0; i < block_map->entries; i++) {

		block_map_entry = &block_map->map[i];

		/*
		 * If the set of plexes we can read from is not the same
		 * as the set of plexes we can write to, then 
		 * something is stale.
		 */
		if (block_map_entry->read_plex_vector !=
                    block_map_entry->write_plex_vector) {

			if (start_blkno < min_stale_blkno)
				min_stale_blkno = start_blkno;
			if (block_map_entry->end_blkno > max_stale_blkno)
				max_stale_blkno = block_map_entry->end_blkno;
			is_stale = B_TRUE;
		}
		start_blkno = block_map_entry->end_blkno + 1;
	}

	free (block_map);

	*first_blkno = min_stale_blkno;
	*last_blkno = max_stale_blkno;

	return (is_stale);
}


/*
 * The following routines - init_plexd_request, issue_plexd_request,
 * and end_plexd_request are used together to initialize a plexd
 * request, accumulate subvolumes that need to be revived, and then
 * issuing them to the xlv_plexd as needed.
 */


void
xlv_init_revive_request(xlv_revive_rqst_t *revive_request)
{
	bzero (revive_request, sizeof(xlv_revive_rqst_t));
	revive_request->fd = -1;
}


int
xlv_issue_revive_request (
	xlv_revive_rqst_t	*revive_request,
	xlv_tab_subvol_t	*xlv_p,
	__int64_t		first_blkno,
	__int64_t		last_blkno)
{
	unsigned		i;
	ssize_t			n;
	xlv_plexd_request_t	*plexd_request;
	xlv_oref_t		oref;

	if (revive_request->fd == -1) {
		if ((revive_request->fd = open (XLV_PLEXD_RQST_FIFO,
					        O_WRONLY)) == -1) {
                	perror ("plex_revive, open of fifo failed");
			return -1;
		}
        }

	plexd_request = &revive_request->plexd_request;

	i = plexd_request->num_requests++;

	/*
	 * Get the string name for the subvolume, e.g.
	 * "pascal_vol.log".
	 */
	XLV_OREF_INIT(&oref);
	XLV_OREF_SET_VOL(&oref, xlv_p->vol_p);
	XLV_OREF_SET_SUBVOL(&oref, xlv_p);

	xlv_oref_to_string (&oref, plexd_request->request[i].subvol_name);
	plexd_request->request[i].start_blkno = first_blkno;
	plexd_request->request[i].end_blkno = last_blkno;
	plexd_request->request[i].device = xlv_p->dev;

#ifdef DEBUG
	fprintf(stderr, "DBG: Issue revive request for %s\n",
		plexd_request->request[i].subvol_name);
#endif

	if (plexd_request->num_requests == PLEX_CPY_CMDS_PER_REQUEST) {

		n = write (revive_request->fd, plexd_request,
			   sizeof(xlv_plexd_request_t));
        	if (n != sizeof(xlv_plexd_request_t)) {
			perror ("xlv_plexd_test, write failed");
			return -1;
		}

		bzero (plexd_request, sizeof(xlv_plexd_request_t));
		revive_request->fd = -1;
	}

	return 0;
}


int
xlv_end_revive_request (xlv_revive_rqst_t     *revive_request)
{
	ssize_t			n;
	xlv_plexd_request_t	*plexd_request;

	/*
	 * First, send any requests that are still pending.
	 */
	plexd_request = &revive_request->plexd_request;
	if (plexd_request->num_requests > 0) {

		if (revive_request->fd == -1) {
			if ((revive_request->fd = open (XLV_PLEXD_RQST_FIFO,
                                                O_WRONLY)) == -1) {
				perror ("plex_revive, open of fifo failed");
				return -1;
			}
		}

		n = write (revive_request->fd, &revive_request->plexd_request,
			   sizeof(xlv_plexd_request_t));
                if (n != sizeof(xlv_plexd_request_t)) {
                        perror ("xlv_plexd_test, write failed");
                        return -1;
                }
	}
	close (revive_request->fd);

	bzero (revive_request, sizeof(xlv_revive_rqst_t));
	revive_request->fd = -1;

	return (0);
}
