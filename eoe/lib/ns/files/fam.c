#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <bstring.h>
#include <sys/time.h>

#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

#define RPCPROG_FAM 391002
#define RPCVERS_FAM 1

#define FILE_FAM_STATE_UNCONNECTED 0
#define FILE_FAM_STATE_PMAP_UNREGISTERED 1
#define FILE_FAM_STATE_CONNECTING 2
#define FILE_FAM_STATE_CONNECTED 3

int __file_fam_state = FILE_FAM_STATE_UNCONNECTED;
int __file_fam_fd = -1;

extern nsd_file_t *__nsd_mounts;

unsigned short pmap_getport(struct sockaddr_in *,int,int,int);

/*
**  WARNING !!! This code can block  XXX -- RGM -- XXX
**
**  The long term intention is to write a (local) file access montior
**  That can request cache flushes.
**
*/

typedef struct file_fam_map {
	char *file;
	char **maps;
	struct file_fam_map *next;
} file_fam_map_t;

file_fam_map_t *__file_fam_maps = 0;


static int
file_fam_register(char *file)
{
	static int count=0;
	char header[sizeof(long)];
	char buf[MAXPATHLEN];
	int i;
	uint32_t nbytes;

	nbytes = snprintf(buf, sizeof(buf), "%c%d %d %d %s\n", 'W', count++,
	    0, 0, file);

	nsd_logprintf(1, "registering %s with fam\n",file);

	header[0] = header[1] = 0;
	header[2]= (nbytes >> 8) & 0x000000ff;
	header[3]= nbytes & 0x000000ff;
	
	i = write(__file_fam_fd, header, sizeof(long));
	if (i != sizeof(long)) {
		nsd_logprintf(1, "nsd_fam_connect header failed");
		return NSD_ERROR;
	}
	i = write(__file_fam_fd,buf, nbytes);
	if (i != nbytes) {
		nsd_logprintf(1, "write failed [%d (wanted %d)]\n",i, nbytes);
	}		
	return NSD_OK;
}

/* ARGSUSED */
static int
file_fam_callback(nsd_file_t **rqp, int fd) 
{
	int req;
	char buf[MAXPATHLEN], code=0, change[8], filename[MAXPATHLEN];
	char *msg, **maps;
	nsd_file_t *dp, *fp;
	file_fam_map_t *map;

	nsd_logprintf(1, "entering file_fam_callback:\n");
	
	memset(&buf, 0, MAXPATHLEN);

	/*
	** fam messages look like
	** L%c%d [%s ]%s
	** A (binary) long  -- size
	** a char -- code
	** a ascii number -- request
	** a optional string -- change fields
	** a string -- filename
	*/

	if (read(fd, &buf, MAXPATHLEN) == 0) {
		nsd_logprintf(1,"connection to fam has died ... reconnecting\n");
		nsd_callback_remove(fd);
		shutdown(fd,2);
		close(fd);
		__file_fam_state = FILE_FAM_STATE_UNCONNECTED;
		return file_fam_connect(NULL);
	}
	msg = buf;

	/*	size = (long)*msg; */
	msg += sizeof(long);

	sscanf(msg, "%c", &code);

	if (code == 'c') {
		sscanf( msg, "%c%d %s %s", &code, &req, change, filename);
		/*
		** chage will be a string with the concat of the following
		** d dev
		** i ino
		** u uid
		** g gid
		** s size
		** m mtime
		** c ctime (inode changed)
		*/
	} else {
		sscanf( msg, "%c%d %s", &code, &req, filename );
		if (!(code == 'c' || code == 'C' || 
		      code == 'e' || code == 'F')) {
			nsd_logprintf(4, "unintresting event %c on %s\n",
				      code, filename);
			return NSD_OK;
		}
	}
	nsd_logprintf(1, "fam reports %s has been updated\n",filename);
	
	dp = nsd_file_byname(__nsd_mounts->f_dir, ".local", strlen(".local"));
	if (! dp) {
		return NSD_ERROR;
	}

	for (map = __file_fam_maps; map ; map = map->next) {
		if (!strcmp(map->file, filename)) {
			break;
		}
	}
	if (map) {
		for (maps = map->maps; maps && *maps; maps++) {
			if (fp = nsd_file_byname(dp->f_dir, *maps, strlen(*maps))) {
				nsd_map_flush(fp); 
				nsd_file_timeout(&fp,0);
			} else {
				nsd_logprintf(1, "Cant find nsd_file for %s\n",
					      *maps);
			}
		}
	} else {
		nsd_logprintf(1, "Cant file maps registration for %s\n", filename);
	}

	return NSD_OK;
}

/* ARGSUSED */
static int
file_fam_connected(nsd_file_t **rqp, int fd) 
{
	int i, errs, size;
	file_fam_map_t *map;

	nsd_logprintf(1,"entering file_fam_connected:\n");
	
	errs=0;
	size=sizeof(errs);
	i=getsockopt(fd, SOL_SOCKET, SO_ERROR, &errs, &size);
	if (i < 0) {
		nsd_logprintf(1,"getsockopt failed\n");
		return NSD_ERROR;
	}
	nsd_callback_remove(fd);
	if (errs) {
		nsd_logprintf(0, "file_fam_connected failed: %s\n",
			      strerror(errs));
		if (errs == ECONNREFUSED || errs == ETIMEDOUT) {
			nsd_logprintf(0, "need to retry this connect\n");
		}
		shutdown(fd,2);
		close(fd);
		__file_fam_state=FILE_FAM_STATE_UNCONNECTED;
		return NSD_ERROR;
	}
	
	__file_fam_state = FILE_FAM_STATE_CONNECTED;
	nsd_callback_new(__file_fam_fd, file_fam_callback, NSD_READ);

	for (map = __file_fam_maps; map ; map = map->next) {
		file_fam_register(map->file);
	}
	return NSD_OK;
}	

/* ARGSUSED */
static int
file_fam_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nsd_logprintf(1, "entering file_fam_timeout:\n");

	/*
	*rqp = rq = to->t_file;
	if (! rq) {
		return NSD_ERROR;
	}
	*/
	nsd_timeout_remove(*rqp);

	__file_fam_state = FILE_FAM_STATE_UNCONNECTED;
	return file_fam_connect(rqp);
}

/* ARGSUSED */
int 
file_fam_connect(nsd_file_t **rqp)
{
	static int retry_timeout=0;
	nsd_file_t *rq;

	struct sockaddr_in sin;
	int i,j;
	
	if (__file_fam_state == FILE_FAM_STATE_CONNECTED) {
		return NSD_OK;
	}
	if (__file_fam_state == FILE_FAM_STATE_CONNECTING ||
	    __file_fam_state == FILE_FAM_STATE_PMAP_UNREGISTERED) {
		return NSD_CONTINUE;
	}

	nsd_logprintf(1, "file_fam_connect:\n");
	
	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_LOOPBACK;
	sin.sin_port = pmap_getport(&sin, RPCPROG_FAM, RPCPROG_FAM, IPPROTO_TCP);
	if (sin.sin_port == 0) {
		if (retry_timeout >= 480000) {
			nsd_logprintf(0, "could not find sgi_fam registration with portmap\n");
			nsd_logprintf(0, "Expect upto 5 minute delay in changes to nsd monitored files\n");
			return NSD_ERROR;
		}	
		if (!retry_timeout) {
			__file_fam_state = FILE_FAM_STATE_PMAP_UNREGISTERED;
			retry_timeout=30000;
		} else {
			retry_timeout *= 2;
		}
		if (rqp && *rqp) {
			rq = *rqp;
		} else {
			if (nsd_file_init(&rq, "fam_pmap_timeout", 
					  sizeof("fam_pmap_timeout"), NULL,
					  NFREG) != NSD_OK) {
				nsd_logprintf(1, "\tfailed to init file\n");
				return NSD_ERROR;
			}
		}
		nsd_logprintf(1, "retrying portmap querry for sgi_fam in %d seconds\n",
			     retry_timeout/1000);
		nsd_timeout_new(rq,retry_timeout, file_fam_timeout, NULL);
		return NSD_CONTINUE;
	}		
	if (retry_timeout) {
		if (!rqp || !*rqp) {
			nsd_logprintf(1, "missing request with retry_timeout set\n");
		} else {
			nsd_timeout_remove(*rqp);
			nsd_file_clear(rqp);
		}
		retry_timeout=0;
	}
	
	__file_fam_fd = socket(PF_INET, SOCK_STREAM,0);
	
	if (__file_fam_fd < 0) {
		nsd_logprintf(0,"file_fam_connect socket failed\n");
		return NSD_ERROR;
	}
	j = O_ACCMODE & fcntl(__file_fam_fd,F_GETFL);

	i = fcntl(__file_fam_fd,F_SETFL,j | FNONBLK);
	if (i == -1) {
		nsd_logprintf(0,"fcntl setfl failed\n");
	}

	i = connect(__file_fam_fd,&sin, sizeof(sin));
	/*
	** This is a non-blocking connect, we expect it to fail with
	** EINPROGRESS ... This is a good thing
	*/
	if (i < 0) {
		if (errno != EINPROGRESS) {
			nsd_logprintf(1,"nsd_fam_connect connect failed\n");
			shutdown(__file_fam_fd, 2);
			close(__file_fam_fd);
			return NSD_ERROR;
		}
		nsd_callback_new(__file_fam_fd, file_fam_connected, NSD_WRITE);
		__file_fam_state=FILE_FAM_STATE_CONNECTING;
		return NSD_CONTINUE;
	}

	__file_fam_state = FILE_FAM_STATE_CONNECTED;
	nsd_callback_new(__file_fam_fd, file_fam_callback, NSD_READ);
	return NSD_OK;
}

int
file_fam_monitor(char *file, char *maps[])
{
	int i;
	file_fam_map_t *fm;
	
	nsd_logprintf(1, "Entering file_fam_monitor:\n");

	i = file_fam_connect(NULL);
	if (i == NSD_OK ) {
		file_fam_register(file);
	}
	
	fm = nsd_calloc(1, sizeof(file_fam_map_t));
	if (!fm) {
		nsd_logprintf(0, "calloc failed\n");
		return NSD_ERROR;
	}

	fm->file=file;
	fm->maps=maps;
	fm->next=__file_fam_maps;
	__file_fam_maps = fm;
	return NSD_OK;
}	
