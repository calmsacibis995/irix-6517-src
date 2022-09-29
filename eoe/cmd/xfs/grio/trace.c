#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/trace.c,v 1.7 1997/02/25 18:11:42 kanoj Exp $"

/*
 * trace.c
 *	grio utility
 *
 *
 */

#include "griostat.h"

int
trace_commands( void)
{
#ifdef NOTNOW
	int 		i, ret, j = 0, resvcount = 0;
	int		fd, tracing = 1000;
	grio_return_t	grioret;
	struct stat	statbuf;

	fd = start_trace( );
	while (tracing--) {
        	/*
         	 * Read info from pipe.
         	 */
        	ret = read( fd, &grioret, sizeof(grio_return_t));
        	if (ret != sizeof(grio_return_t)) {
               		printf("read failed \n");
               		ret = -1;
        	}
		if ( grioret.gblk.resv_type == GRIO_RESV_FILE ) {
			printf("FILE RESV: pid %d ino %lld dev %x \n",
				grioret.gblk.procid,
				grioret.gblk.ino,
				grioret.gblk.fs_dev);
		}

		if (    ( grioret.gblk.resv_type == GRIO_UNRESV_FILE ) ||
			( grioret.gblk.resv_type == GRIO_UNRESV_FILE_ASYNC ) ) {
			printf("FILE UNRESV: pid %d ino %lld dev %x \n",
				grioret.gblk.procid,
				grioret.gblk.ino,
				grioret.gblk.fs_dev);
		}

	}
	end_trace( fd );
#endif
	return ( 0 );
}

#ifdef NOTNOW
int
start_trace( void )
{
	grio_blk_t	grioblk;
	int     fd, ret, r;
        char    pidstr[GRIO_PID_LEN];

	grioblk.fd 		= time(0);
	grioblk.sub_cmd		= GRIO_TRACE_REQS;
	grioblk.procid		= getpid();
	grioblk.resv_type	= GRIO_GET_INFO;

        /*
         * Create return pipe.
         */
        bzero( return_pipe, GRIO_PIPE_NAMLEN );
        strncpy( return_pipe, GRIO_PIPE, strlen(GRIO_PIPE));
        strncat( return_pipe, ".", 1);
        sprintf( pidstr, "%d", getpid());
        strcat( return_pipe, pidstr);

        unlink (return_pipe);

        if (ret = mknod(return_pipe, S_IFIFO, 0 )) {
                printf("mknod failed errno = %d \n", errno);
                return( -1);
        }

        if ((fd = open( return_pipe, O_RDWR)) == -1 ) {
                printf("open failed errno = %d \n", errno);
                return( -1 );
        }

        /*
         * Send request to ggd daemon.
         */
        if (ret = syssgi( SGI_GRIO, GRIO_WRITE_GRIO_REQ, &grioblk)) {
                printf("Could not write to daemon \n");
		close( fd );
                return( -1 );
        }

        return( fd );
}

int
end_trace( int fd) 
{
	int			ret;
	grio_blk_t		grioblk;

	grioblk.fd 		= time(0);
	grioblk.sub_cmd		= GRIO_UNTRACE_REQS;
	grioblk.procid		= getpid();
	grioblk.resv_type	= GRIO_GET_INFO;

        /*
         * Send request to ggd daemon.
         */
        if (ret = syssgi( SGI_GRIO, GRIO_WRITE_GRIO_REQ, &grioblk)) {
                printf("Could not write to daemon \n");
                return( -1 );
        }

	close( fd );
	unlink(return_pipe);
	return( 0 );
}
#endif
