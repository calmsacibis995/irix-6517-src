/*
 * prim.c
 *	grio utility
 *
 */

#include "griostat.h"

/*
 * add_to_list()
 *
 *
 * RETURNS:
 *	always returns 0
 */
int
add_to_list( char *devname, device_list_t devlist[], int *devcount)
{
        strncpy(devlist[(*devcount)].name,devname, DEV_NAME_LEN);
        (*devcount) += 1;

        return( 0 );
}



/*
 * item_in_list()
 *	Determine if the given name is already found in the given list.
 *
 * RETURNS:
 *      1 if the name is in devlist
 *      0 otherwise
 */
int
item_in_list(char *name, device_list_t devlist[], int devcount)
{
        int     i;

        for (i = 0; i < devcount; i++) {
                if (strcmp(name, devlist[i].name) == 0) {
                        return( 1 );
                }
        }
        return( 0 );
}



int
issue_vdisk_ioctl( dev_t vdev, int cmd, char *buffer)
{
        int fd, ret = 1;
	char dev_name[40];

	sprintf(dev_name,"%s.%d","/tmp/grio_status_vdev", getpid());
	unlink(dev_name);

        /*
         * Make virtual disk dev node.
         */
        if (mknod( dev_name, S_IFCHR, vdev)) {
                return( 0 );
        }

        if ((fd = open( dev_name, O_RDWR )) == -1 ) {
                unlink( dev_name );
                return( 0 );
	}


        if ((ret = ioctl(fd, cmd, buffer)) < 0 ) {
                ret = 0;
        }

	close(fd);
	unlink(dev_name);

        return( ret );

}

int
send_file_resvs_command(dev_t fsdev,gr_ino_t ino, grio_stats_t *griosp )
{
	grio_cmd_t	griocp;
	grio_stats_t	*locgriosp;

	bzero( &griocp, sizeof(grio_cmd_t ) );

	griocp.gr_cmd 		= GRIO_GET_STATS;
	griocp.gr_subcmd 	= GRIO_FILE_RESVS;
	locgriosp = GRIO_GET_STATS_DATA( (&griocp) );

	locgriosp->gs_count	= griosp->gs_count;
	locgriosp->gs_maxresv	= griosp->gs_maxresv;
	locgriosp->gs_optiosize	= griosp->gs_optiosize;


	griocp.one_end.gr_dev	= fsdev;
	griocp.one_end.gr_ino	= ino;

	if ( grio_send_commands_to_daemon(1, &griocp ) ) {
		printf("could not send command to ggd daemon \n");
		exit( -1 );
	}

	griosp->gs_count	= locgriosp->gs_count;
	griosp->gs_maxresv	= locgriosp->gs_maxresv;
	griosp->gs_optiosize	= locgriosp->gs_optiosize;

	return( 0 );
}

int
show_command_stats(void) 
{

	if (ggd_info == NULL ) {
		printf("could not attach to shared memory segment \n");
		return( -1 );
	}

	printf("ggd statistics: \n");
	printf("\treserve requests:\t\t%d\n",
		ggd_info->per_cmd_count[GRIO_RESV]);
	printf("\tunreserve requests:\t\t%d\n",
		ggd_info->per_cmd_count[GRIO_UNRESV]);
	printf("\tauto unreserve requests:\t%d\n",
		ggd_info->per_cmd_count[GRIO_UNRESV_ASYNC]);
	printf("\tget bandwidth info requests:\t%d\n",
		ggd_info->per_cmd_count[GRIO_GET_BW]);
	printf("\tget statistics info requests:\t%d\n",
		ggd_info->per_cmd_count[GRIO_GET_STATS]);
	printf("\tpurge XLV information requests:\t%d\n",
		ggd_info->per_cmd_count[GRIO_PURGE_VDEV_ASYNC]);
	return( 0 );
}


int
find_file_name( char *filename, gr_ino_t ino, char *dir)
{
	int	ret = 0;
	char	system_str[STR_LEN], *tmpfilename;
	FILE	*fd;
	
	if (ino == 0) return(0);
	tmpfilename = tmpnam( NULL );
	sprintf(system_str,"/sbin/find %s -inum %lld -print -mount > %s",
		dir,ino, tmpfilename);
	system(system_str);

	fd  = fopen( tmpfilename, "r");
	if ( fd ) {

		if ( fgets( filename, STR_LEN ,fd) != NULL ) {
			ret = 1;

			/*
			 * remove carriage return character
			 */
			filename[strlen(filename)-1] = 0;
		}

		fclose( fd );
	}
	unlink( tmpfilename );

	return( ret );

}

int
find_fs_name( dev_t  fsdev, char *fsmntname )
{
	FILE		*fd;
	struct mntent	*mntent;
	struct stat	statbuf;

	fd = setmntent("/etc/mtab","r");

	while ( mntent = getmntent( fd) ) {

		if ( stat(mntent->mnt_dir,&statbuf) ) {
			printf("Stat of %s failed.\n",mntent->mnt_dir);
		} else if (statbuf.st_dev == fsdev ) {
			bcopy(  mntent->mnt_dir, 
				fsmntname,
				strlen(mntent->mnt_dir) );

			endmntent( fd );
			return( 1 );
		}
	
	}
	endmntent( fd );

	return( 0 );
}

#define MAXLOGIN	8

struct udata {
	uid_t   uid;            /* numeric user id */
	char    name[MAXLOGIN]; /* login name */
};

int	maxud, nud;
struct	udata *ud = NULL;

int
init_udata( void )
{
	struct passwd *pw;

	nud = 0;
	maxud = 0;

	while ((pw = getpwent()) != NULL) {
		while (nud >= maxud) {
			maxud += 50;
			ud = (struct udata *) ((ud == NULL) ?
				malloc(sizeof(struct udata ) * maxud) :
				realloc(ud, sizeof(struct udata ) * maxud));

			if (ud == NULL) {
				exit(1);
			}
		}

		/*
		 * Copy fields from pw file structure to udata.
		 */

		ud[nud].uid = pw->pw_uid;
		(void) strncpy(ud[nud].name, pw->pw_name, MAXLOGIN);
		nud++;
	}
	endpwent();
	return( 0 );
}

int
convert_uid_to_login( int id, char *username)
{
	int 	i;

	if (ud == NULL ) {
		init_udata();
	}

	bzero( username, STR_LEN );

	for (i = 0; i < nud; i++) {
		if ( ud[i].uid == id) {
			bcopy( ud[i].name, username, strlen( ud[i].name) );
			return( 1 );
		}
	}
	return( 1 );
}

int
find_proc_name( pid_t pidnum, char *username, char *procname)
{
	int		fd, ret = 1;
	char		pidinfostr[STR_LEN];
	prpsinfo_t	prpsinfo;

	sprintf(pidinfostr,"/proc/pinfo/%d",pidnum);

	bcopy( "unknown", username, strlen("unknown") );
	bcopy( "unknown", procname, strlen("unknown") );

	/*
	 * this open call could fail if the process 
	 * nolonger exists. it that case, the unknown
	 * string will be displayed for user and
	 * process
	 */
	fd = open( pidinfostr,O_RDONLY);
	if ( fd != -1 ) {
		if ( ioctl(fd, PIOCPSINFO, &prpsinfo) != -1 ) {
			/*
	 	 	 * convert uid to login name
		 	 */
			convert_uid_to_login( prpsinfo.pr_uid, username );

			bcopy(	prpsinfo.pr_psargs,
				procname,
				strlen(prpsinfo.pr_psargs) );

			ret = 0;
		}
		close(fd);
	}
	return( ret );
}

int
get_all_streams(void)
{
	int 			i, first = 1, count = 1;
	char			*uuidstr;
	char			filename[STR_LEN], fsname[STR_LEN];
	char			username[STR_LEN], procstr[STR_LEN];
	uint_t			status;
	grio_stream_stat_t	grios[MAX_STREAM_STAT_COUNT];

	bzero( grios, sizeof(grio_stream_stat_t)* MAX_STREAM_STAT_COUNT);

	if ( syssgi(SGI_GRIO, GRIO_GET_ALL_STREAMS, &grios) ) {
		exit( -1);
	}


	for (i = 0 ; i < MAX_STREAM_STAT_COUNT; i++) {
		if ( uuid_is_nil( &(grios[i].stream_id), &status ) ) {
			;		
		} else {
			if ( first ) {
				first = 0;
				printf("GRIO reservations:\n\n");
			}

			uuid_to_string( &(grios[i].stream_id),
				&uuidstr, &status);

			bzero(filename, STR_LEN);
			bzero(fsname, STR_LEN);
			bzero(procstr, STR_LEN);
			find_fs_name( grios[i].fs_dev, fsname);
			find_file_name( filename, grios[i].ino, fsname );
			find_proc_name( grios[i].procid, username, procstr );

			printf("stream %3d id: %s, owner pid: %6d\n",
				count,
				uuidstr, 
				grios[i].procid);

			printf("associated file: (0x%llx,0x%x) %s\n",
				grios[i].ino,
				grios[i].fs_dev, 
				filename);

			printf("current user login: %s, process args: %s \n",
				username,
				procstr);

			free( uuidstr );
			printf("\n");
			count++;
		}
	}
	if (first) {
		printf("No active GRIO reservations. \n");
	}

	return( 0 );
}

int
delete_streams( stream_id_t strlist[], int strcount)
{

	int	i;
	char	*ptr;
	uint_t	status;

	for (i = 0; i < strcount; i++) {
		uuid_to_string( &strlist[i], &ptr, &status);
		printf("deleting stream id: %s \n",ptr);
		free(ptr);
		if ( grio_unreserve_bw( &strlist[i] ) ) {
			printf("delete failed \n");
		} else {
			printf("stream deleted \n");
		}
	}
	return( 0 );
}

int
add_id_to_list( char *newid, stream_id_t strlist[], int *strcount)
{
	int		ret;
	uint_t		status;

	uuid_from_string(newid, &strlist[*strcount], &status);
	switch ( status ) {
		case uuid_s_ok:
			ret = 0;
			break;
		case uuid_s_invalid_string_uuid:
			ret = -1;
			printf("Invalid stream id string: %s \n",newid);
			break;
		case uuid_s_bad_version:
		case uuid_s_getconf_failure:
		case uuid_s_no_address:
		case uuid_s_socket_failure:
		default:
			printf("Uuid conversion error %d.\n",status);
			ret = -1;
			break;
	}

	(*strcount)++;

	return( ret );
}

int
show_ggd_info_nodes( void )
{
	int			i, count;
	ggd_node_summary_t	*ggd_node;

	count = ggd_info->num_nodes;

	printf("There are %d nodes in the system\n", count);

	for ( i = 0 ; i < count; i++) {
		ggd_node = &(ggd_info->node_info[i]);

		printf("Name %d\n", ggd_node->node_num);

		printf("Max I/O rate  %d, Remaining I/O rate %d \n",
			ggd_node->max_io_rate, ggd_node->remaining_io_rate);

		printf("Optimal I/O size %d, Optimal I/O count %d \n",
			ggd_node->optiosize, ggd_node->num_opt_ios);

	}
	return( 0 );
}
