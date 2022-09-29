#ident "$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/grio_monitor.c,v 1.3 1995/10/05 20:35:09 jwag Exp $"

#ifdef SIM
#define _KERNEL
#include <stdio.h>
#endif
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/major.h>
#include <sys/mkdev.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_inum.h>
#include <sys/time.h>
#include <sys/grio.h>
#include <sys/bpqueue.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#ifdef SIM
#undef _KERNEL
#endif

#define		MONITORED_STREAMS_COUNT		5

int		monitoring = 1;

grio_monitor_t  monitor_info[MONITORED_STREAMS_COUNT];

/*
 * this file contains the routines that implement the grio
 * monitoring function:
 *
 */

int
grio_monitor_start( sysarg_t sysarg_id)
{
	int 		i;
	uint_t		status;
	stream_id_t	stream_id;

	if (!monitoring) {
		return( -1 );
	}

	if ( copyin( 	(caddr_t)sysarg_id, 
			&stream_id, 
			sizeof(stream_id_t))    ) {
		ASSERT( 0 );
		return( -1 );
	}

	for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
		if ( monitor_info[i].monitoring == 1 ) {
			if ( EQUAL_STREAM_ID( stream_id, 
			 	monitor_info[i].stream_id) ) {
				/*
			 	 * already monitoring
				 */
				return( 0 );
			}
		}
	}

	for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
		if ( monitor_info[i].monitoring == 0 ) {
			monitor_info[i].monitoring = 1;
			COPY_STREAM_ID( stream_id, monitor_info[i].stream_id);
			monitor_info[i].start_index = 0;
			monitor_info[i].end_index = 0;
			return( 0 );
		}
	}
	return( -1 );
}

int
grio_monitor_stop( sysarg_t sysarg_id )
{
	int 		i;
	uint_t		status;
	stream_id_t	stream_id;

	if (!monitoring) {
		return( -1 );
	}

	if ( copyin( 	(caddr_t)sysarg_id, 
			&stream_id, 
			sizeof(stream_id_t))    ) {
		ASSERT( 0 );
		return( -1 );
	}

	for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
		if ( monitor_info[i].monitoring == 1 ) {
			if ( EQUAL_STREAM_ID( stream_id, 
			 		monitor_info[i].stream_id) ) {
				monitor_info[i].monitoring = 0;
				return( 0 );
			}
		}
	}
	return( -1 );
}

int
grio_monitor_read(sysarg_t sysarg_buf, sysarg_t sysarg_id) 
{
	int 		i;
	uint_t		status;
	stream_id_t	stream_id;


	if (!monitoring) {
		return( -1 );
	}

	if ( copyin( 	(caddr_t)sysarg_id, 
			&stream_id, 
			sizeof(stream_id_t))    ) {
		ASSERT( 0 );
		return( -1 );
	}

	for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
		if ( monitor_info[i].monitoring == 1 ) {
			if ( EQUAL_STREAM_ID( stream_id, 
			 		monitor_info[i].stream_id) ) {

				if ( copyout((void *)(&(monitor_info[i])),
					(caddr_t)sysarg_buf, sizeof( grio_monitor_t) ) ) {
					ASSERT( 0 );
					return( -1 );
				}
				return( 0 );
			}
		}
	}

	return( -1 );
}

int
grio_monitor_io_start( stream_id_t *stream_id, __int64_t iosize)
{
	int	index = -1, i;
	uint_t	status;

	if (monitoring) {
		for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
			if ( monitor_info[i].monitoring == 1 ) {
				if ( EQUAL_STREAM_ID( (*stream_id), 
monitor_info[i].stream_id ) ) {
			index = monitor_info[i].start_index;

			monitor_info[i].start_index++;
			monitor_info[i].start_index = 
					monitor_info[i].start_index % 
					GRIO_MONITOR_COUNT;

			monitor_info[i].times[index].starttime	= lbolt;
			monitor_info[i].times[index].size	= iosize;
			return( index );
				}
			}
		}
	}
	return( -1 );
}

int
grio_monitor_io_end( stream_id_t *stream_id, int index )
{
	int		i;
	uint_t		status;

	if ( monitoring && (index != -1) ) {
		for (i = 0; i < MONITORED_STREAMS_COUNT; i++) {
			if ( monitor_info[i].monitoring == 1 ) {
				if ( EQUAL_STREAM_ID( (*stream_id),
					monitor_info[i].stream_id ) ) {
				monitor_info[i].times[index].endtime= lbolt;
				monitor_info[i].end_index 	= index;
				return( 0 );
				}
			}
		}
	}
	return( -1 );
}
