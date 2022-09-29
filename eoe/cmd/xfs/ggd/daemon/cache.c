#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <invent.h>
#include <grio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/ktime.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

dev_cached_info_t	dev_info[MAX_DEVINFOS_CACHED];


/* REFERENCED */
void
init_cache(void)
{
	bzero(dev_info, 
		MAX_DEVINFOS_CACHED * sizeof(dev_cached_info_t));
}


/*
** return the index at which info. about the given devnum should
** be inserted (or over-written); 
*/

STATIC int
find_insert_point(dev_t devnum)
{
	long long     start_index, i, cache_full;
	
	start_index = i = devnum % MAX_DEVINFOS_CACHED;
	cache_full = 0;

	do {
		if(dev_info[i].devnum == devnum ||
			dev_info[i].info == NULL)  {
			/* either same devnum, or vacant slot */
			return  (int) i;
		}  else  {
			i = (i + 1) % MAX_DEVINFOS_CACHED;
			if(i == start_index)  {
				cache_full = 1;
			}
		}
	} while (! cache_full);
	
	return	GGD_CACHE_FULL;
}


/*
** return index in cache where info for this devnum is, if present;
*/

STATIC dev_cached_info_t *
lookup_cache(dev_t devnum)
{
	long long	start_index, i, present;
	
	start_index = i = devnum % MAX_DEVINFOS_CACHED;
	present = 1;

	do {
		if(dev_info[i].devnum == devnum)  {
			return	&(dev_info[i]);
		} else {
			i = (i + 1) % MAX_DEVINFOS_CACHED;
			if(i == start_index)  {
				present = 0;
			}
		}
	} while (present);

	return NULL;
}


/*
** insert the given info about devnum into the cache
** return 0 on success;
** return -1 on failure.
** failure can be due to:
**		overflow in the cache
*/

int
insert_info_in_cache(dev_t devnum, void *info)
{
	int	i;

	i = find_insert_point(devnum);
	if(i == GGD_CACHE_FULL)  {
		return -1;
	}  else  {
		dev_info[i].devnum = devnum;
		dev_info[i].info = info;
		return	0;
	}
}


/*
** delete the info for this devnum; if info about devnum was already
** in the cache, the info. is deleted, and 0 is returned; if no info
** about devnum exists in the cache, -1 is returned
*/ 

int
delete_info(dev_t devnum)
{
	dev_cached_info_t	*devinfo_p;

	devinfo_p = lookup_cache(devnum);

	if(devinfo_p == NULL)  {  /* devnum's info wasn't present */
		return -1;
	}  else  {   /* delete the info for devnum; return 0 */
		bzero(devinfo_p, sizeof(dev_cached_info_t));
		return 0;
	}
}



/*
 * Search the cache and return the devnode info if it is there.
 */

device_node_t *
devinfo_from_cache(dev_t devnum)
{
	dev_cached_info_t 	*cached_info;

	/*
 	 * If found in cache, return it, elae, return NULL
	 */

	if(cached_info = lookup_cache(devnum)) {
		return (device_node_t *) cached_info->info;
	}

	/* the device-specific part of ggd must catch the NULL
	** returned, thus figure out that info wasn't there,
	** query the device in some way to get the info, and then
	** insert the info into ggd-cache via insert_info_in_cache()
	*/

	return NULL;
	
}


void
update_device_reservations(time_t now)
{
	int 			i, rate;
	__int64_t		maxreqtime, maxreqsize, lrate, sec;
	device_node_t		*devnp;

	for (i = 0; i < MAX_DEVINFOS_CACHED; i++) 
		if ((devnp = (device_node_t *)dev_info[i].info) != NULL) {
			/*
			 * remove expired reservations on this node
			 */
			remove_expired_reservations(devnp, now);

			/*
			 * Start any new guarantees
			 */
			start_new_device_reservations(devnp, now);

			/*
			 * if the remaining bandwidth cannot be
			 * determined, assume that none is remaining.
			 */
			if (determine_remaining_bandwidth(devnp,
				now, &maxreqsize, &maxreqtime)) 
				maxreqsize = 0;

			/*
			 * convert maxreqsize bytes per maxreqtime usecs to
			 * number of kbytes/sec
			 */
			if (maxreqsize == 0LL) 
				lrate = 0LL;
			else {
				sec = maxreqtime/USEC_PER_SEC;
				lrate = maxreqsize/sec;
			}
			lrate = lrate/1024;
			rate = (int)lrate;

			update_ggd_info_node(devnp, rate);
		}
}

void
get_file_resvd_bw_info_from_cache(grio_cmd_t *griocp)
{
	int 			i;
	device_node_t		*devnp;
	device_reserv_info_t	*dresvp;
	stream_id_t		*stream_id;
	int			total_bw=0;
	reservations_t		*resvp;
	dev_resv_info_t		*resvpdp;
	int			time;
	uint_t			status;

	stream_id = &(griocp->cmd_info.gr_resv.gr_stream_id);

	for (i = 0; i < MAX_DEVINFOS_CACHED; i++)  {
		if (((devnp = (device_node_t *)dev_info[i].info) != NULL) &&
				(devnp->flags == INV_DISK))  {
			dresvp = RESV_INFO(devnp);
			/* if no reservations can be made on this device,
			 * move on.
			 */
			if ( dresvp->maxioops != 0) {
				resvp = dresvp->resvs;
				while (resvp) {
					resvpdp = DEV_RESV_INFO(resvp);
					if ( EQUAL_STREAM_ID( resvp->stream_id,
						(*stream_id)) ) {
						/* Add bandwidth to total.
						 * Use only the start resv to
						 * avoid double counting.
						 */
						if (resvpdp->resv_type ==
							RESV_START) {
							total_bw += resvpdp->iosize;
							time = resvpdp->io_time;
						}
					}
					resvp = resvp->nextresv;
				}
			}
		}
	}

	griocp->gr_bytes_bw = total_bw;
	griocp->gr_usecs_bw = time;
}
