#ident "irix/cmd/mkprou.c: $Revision: 1.17 $"

/*
 * mkprou.c
 *
 * router support to mkpart
 */

#define SN0 1

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/conf.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>

#include "mkpart.h"
#include "mkprou.h"

/* Can be found in mkplib.c */

extern int 	debug ;
extern int 	force_option ;
extern char 	*msglib[] ;
static int	debug_result ;
static int	mr_interconn_flag ;

void
part_rou_map_init(part_rou_map_t *pmap)
{
	if (pmap == NULL)
		return ;
	bzero(pmap, sizeof(part_rou_map_t)) ;
	pmap->sze = MAX_MODS_PER_PART * MAX_ROU_PER_MOD ;
}

/*
 * Debug routines to dump router data structs.
 */

/*
 * Dump router fence info.
 */
void
dump_rfence(	char 			*rname	,
		sn0drv_router_map_t	*map	,
		sn0drv_fence_t 		*rf	)
{
        int     i ;

	printf("%s - fences\n", rname) ;
        printf("lblk : 0x%llx\nfence: ", rf->local_blk.reg) ;
	ForEachValidRouterPort(map, i) {
                printf("[%d]: 0x%llx, ", i, rf->ports[i-1].reg) ;
	}
	printf("\n") ;
}

/*
 * What is each router port connected to? Another router, hub or nothing.
 */
void
dump_rou_port_map(char *rname, sn0drv_router_map_t *map, nic_t *nic)
{
	int 	i ;

	printf("%s - ports\n", rname) ;
/*	ForEachValidRouterPort(map, i) { */
	ForEachRouterPort(i) {
                if (nic)
                        printf("[%d]:<%llx, %d>", i, nic[i-1], map->type[i-1]) ;
                else
                        printf("%d ,", i) ;
	}
	printf("\n") ;
}

/*
 * Dump the info for a single router.
 */
static void
dump_rou_map(rou_map_t *rmap, int rid)
{
	int	i ;

	printf("---------------------------------------------\n") ;
	printf("<%d, %d>  |  %s  |\n", rmap->myid, rid, rmap->name) ;
	printf("---------------------------------------------\n") ;
	printf("P = %d, M = %d, NIC = %llx\n", 
		rmap->part, rmap->mod, rmap->nic) ;
	printf("---------------------------------------------\n") ;

	ForEachValidRouterPort(&rmap->portmap,i) {
		printf("Port [%d] -> <%d, %d, %d>\n", 
			i, rmap->linkind[i-1].part_ind, 
			   rmap->linkind[i-1].rmap_ind,
			   rmap->linkind[i-1].port_ind) ;
	}
}

/* 
 * Dump all routers in  a partition.
 */
static void
dump_part_rou_map(part_rou_map_t *pmap, int p)
{
	int		i ;
	rou_map_t	*rmap ;

	printf("\tPart %d has %d Routers\n", p, pmap->cnt) ;
        for (i=0; i<pmap->cnt; i++) {
                rmap = &pmap->roumap[i] ;
		dump_rou_map(rmap, i) ;
	}
}

/*
 * Dump all partitions in the system.
 */
void
dump_sys_rmap(sys_rou_map_t *sys_rmap_p)
{
	int	i ;

	printf("sys rou map has %d partitions\n", sys_rmap_p->cnt) ;
	for(i=0; i<sys_rmap_p->cnt; i++) {
		dump_part_rou_map(sys_rmap_p->pr_map[i], i) ;
	}
}

/*
 * sn0drv ioctl support
 *
 * 	Routines to help fill ioctls and get info about
 * 	the router registers.
 */
static void
fill_regop(rou_regop_t *rop, int cmd, int val)
{
	rop->op  	= cmd ;
	rop->reg 	= val ;
}

static void
fill_fence_req(sn0drv_fence_t *rf, int cmd, int val)
{
	int	port ;

	fill_regop(&rf->local_blk, cmd, val) ;

	ForEachRouterPort(port) {
		fill_regop(&rf->ports[port-1], cmd, val) ;
        }
}

/*
 * Use ioctls to get router reg values.
 */
int
get_rou_port_nic(int fd, nic_t *nicp)
{
	int 	err ;

        err = ioctl(fd, SN0DRV_ROU_PORT_NIC_GET, nicp) ;

	return err ;
}

int
get_rou_fence(int fd, sn0drv_fence_t *rf)
{
        int     err ;

        bzero(rf, sizeof(sn0drv_fence_t)) ;
        fill_fence_req(rf, FENCE_READ, 0) ;
        err = ioctl(fd, SN0DRV_ROU_FENCE_OP, rf) ;

        return err ;
}

int
get_rou_nic(int fd, nic_t *nic)
{
	int 	err ;

        err = ioctl(fd, SN0DRV_ROU_NIC_GET, nic) ;

        return err ;
}

int
get_rou_pmap(int fd, sn0drv_router_map_t *rpmap)
{
        int     err ;

        err = ioctl(fd, SN0DRV_ROU_PORT_MAP, rpmap) ;

        return err ;
}

/* strip the string /mon from the end of the file name */

static void
strip_mon(char *buf)
{
	char *cptr;

	cptr = strstr(buf, "/mon") ;
        if (cptr)
                *cptr = 0 ;
}

/*
 * Get a list of hwgraph paths of all routers in the system.
 * using the output of the ls command and read it from a pipe.
 *
 * Return:
 *
 *	Number of routers found.
 * 	Fills up routers with pointers to router strings.
 */
static int
make_rou_name_list(char **routers)
{
        char    *cmd;
        FILE    *fh ;
        char    buf[BUFSIZ] ;
        int     num_rou = 0 ;

        cmd = "ls -1 /hw/module/*/slot/r*/*router/mon";

        if ((fh = popen(cmd, "r")) != NULL) {
                while (fgets(buf, BUFSIZ, fh) != NULL) {
                        routers[num_rou] = (char *)malloc(strlen(buf) + 1);
			if (routers[num_rou] == NULL)
				continue ;
                        if (buf[strlen(buf) - 1] == '\n')
                                buf[strlen(buf) - 1] = '\0';
                        strcpy(routers[num_rou], buf);
                        num_rou++;
                }
        }
	pclose(fh) ;

	return num_rou ;
}

/*
 * Routines to use the hwgraph file system and find out
 * the router connectivity.
 */

#include <dirent.h>

/*
 * get_rou_port_link
 *
 * 	Gets the name of the file linked to direntp using readlink.
 *
 * Returns:
 *
 *	1 on success, -1 on failure.
 *	0 if this iteration can be ignored.
 *	linkbuf contains the name of the link.
 */
static int
get_rou_port_link(	char 			*dir, 
			struct dirent   	*direntp,
			sn0drv_router_map_t 	*m	,
			char			*linkbuf,
			int			linkbuflen)
{
	int		port ;
        char            buf[MAX_HWG_PATH_LEN] ;
	int 		len ;

	port = atoi(direntp->d_name) ;

	/* If port value does not fall within range or
	 * if it is not a router port, ignore and continue.
 	 */
	if (!(IsRouterPort(m,port))) {
		return 0 ;
	}

        strcpy(buf, dir) ;
        strcat(buf, "/") ;
        strcat(buf, direntp->d_name) ;

	if ((len = readlink(buf, linkbuf, linkbuflen)) < 0) {
		return -1 ;
	}
	linkbuf[len] = 0 ;

	return 1 ;
}

/*
 * search_router_dir
 * 
 *	search the router links in directory link for a match
 *	on string src.
 *
 * Returns:
 *	
 *	number of the link port on success.
 *	-1 on failure.
 */
static int
search_router_dir(char *link, char *src, sn0drv_router_map_t *srmap)
{
        DIR             *dirp ;
        struct dirent   *direntp ;
        char            linkbuf[MAX_HWG_PATH_LEN] ;
        int             linkport = -1 ;

        dirp = opendir(link) ;
        if (dirp == NULL) {
                return -1 ;
	}

        while ((direntp = readdir(dirp)) != NULL) {
                if (get_rou_port_link(link, direntp, srmap, 
				linkbuf, sizeof(linkbuf)) <= 0) {
                        continue ;
		}

		if (!strcmp(linkbuf, src)) {
			linkport = atoi(direntp->d_name) ;
			if (linkport == 0)
				continue ;
			else
				break ;
		}
	}

	closedir(dirp) ;
	return linkport ;
}

rou_map_t *
lookup_rmap(part_rou_map_t *pmap, char *path)
{
        int     i ;
        char    tmp[MAX_HWG_PATH_LEN] ;

        if (!path)
                return NULL ;

        for (i=0; i<pmap->cnt; i++) {
                strcpy(tmp, pmap->roumap[i].name) ;
                strip_mon(tmp) ;
                if (!strcmp(path, tmp))
                        return(&pmap->roumap[i]) ;
        }

	return NULL ;
}

/*
 * lookup_link_srmap
 *
 * 	search pmap for the srmap of the string given.
 *
 * Return:
 *
 *	the router map of the linkpath given, NULL on failure.
 */
sn0drv_router_map_t *
lookup_link_srmap(part_rou_map_t *pmap, char *linkpath)
{
	rou_map_t 	*rmap ;

	if ((rmap = lookup_rmap(pmap, linkpath)) == NULL)
		return NULL ;
	return &rmap->portmap ;
}

/*
 * get_rmap_indx
 *
 *	where is router "dir"? Return 'myid' field.
 * 	This is assigned to rmap_indx.
 *
 * Return:
 *
 *	Router map index on success.
 *	-1 on failure.
 */
static int
get_rmap_indx(part_rou_map_t *pmap, char *dir)
{
	rou_map_t 	*rmap ;

	if ((rmap = lookup_rmap(pmap, dir)) == NULL)
		return -1 ;
	return rmap->myid ;
}

/*
 * For every router port in the router directory, dir,
 * traverse hwgraph to find out the router it is connected to.
 * Fill up the router map structure with the rmap_index and 
 * port_index.
 */
int
process_dir(char *dir, rou_map_t *rmap, part_rou_map_t *pmap)
{
	DIR		*dirp ;
	struct dirent	*direntp ;
	char 		linkbuf[MAX_HWG_PATH_LEN] ;
	int		linkport ;
	int		srcport ;
	sn0drv_router_map_t	*srmap = &rmap->portmap ;
	sn0drv_router_map_t	*link_srmap ;
	int		rv = 1 ;
	int		stat ;

	dirp = opendir(dir) ;
	if (dirp == NULL) {
		return -1 ;
	}

	/* Each entry in this directory is the number of a port */

	while ((direntp = readdir(dirp)) != NULL) {
		srcport = atoi(direntp->d_name) ;

		/* Get the name of the link of port */

		if ((stat = get_rou_port_link(dir, direntp, srmap, linkbuf, 
					sizeof(linkbuf))) < 0) {
			rv = -1 ;
			goto close_n_ret ;
		} else if (stat == 0)
			continue ;

		if ((link_srmap = lookup_link_srmap(pmap, linkbuf)) == NULL) {
			rv = -1 ;
			goto close_n_ret ;
		}

		/* Search the link dir for a port file that is linked
		 * back to our dir.
		 */
		if ((linkport = search_router_dir(linkbuf, dir, link_srmap)) 
				<= 0) {
			db_printf("%s is not linked to %s both ways\n", 
					dir, linkbuf) ;
			if (debug) {
				debug_result = -1 ;
				continue ;
			} else {
				rv = -1 ;
				goto close_n_ret ;
			}
		}

		/* Get the rmap index with this router name */

		/* part_ind will be filled in later. */

		rmap->linkind[srcport-1].rmap_ind = 
				get_rmap_indx(pmap, linkbuf) ;
		rmap->linkind[srcport-1].port_ind = linkport ;

		db_printf("%s/%d is linked to %s/%d\n", 
			dir, srcport, linkbuf, linkport) ;
	}

close_n_ret :
	closedir(dirp) ;
	return rv ;
}

/*
 * link_rou_map
 *
 * Interconnect all routers in a partition.
 */
int
link_rou_map(part_rou_map_t *pmap)
{
	int 		i ;
	rou_map_t	*rmap ;
	char		tmp[MAX_HWG_PATH_LEN] ;

	for (i=0; i<pmap->cnt; i++) {
		rmap = &pmap->roumap[i] ;
		strcpy(tmp, rmap->name) ;
		strip_mon(tmp) ;

		/* Use hwgraph to find out where each port is linked to */

		if (process_dir(tmp, rmap, pmap) < 0)
			return -1 ;
	}
	return 1 ;
}

/*
 * create_my_rou_map
 *
 * Create the router map of this partition.
 * Called by the mkpd of each partition or by local check_sanity.
 *
 * Returns:
 *
 *	On success, number of valid bytes of data.
 *	-1 on error, count of pmap is set to 0.
 * 	pmap is filled with the router map of this partition.
 */
int
create_my_rou_map(partid_t p, part_rou_map_t *pmap, int pmapsize)
{
        char                    *routers[MAX_ROU_PER_PART] ;
	int			num_rou ;
	rou_map_t		*rmap ;
	int			i, k ;
	int			fd ;

 	/* Get a list of routers from the hwgraph. */

	num_rou 	= make_rou_name_list(routers) ;
	pmap->cnt 	= num_rou ;

        for (i=0; i<num_rou; i++) {
                fd = open(routers[i], O_RDONLY);
                if (fd < 0) {
			pmap->cnt = 0 ;
			return -1 ;
		}

		/* Cannot fit in all routers, map incomplete */

		if (i >= pmap->sze) {
			pmap->cnt = 0 ;
			return -1 ;
		}

		/* Fill all params for this router. */

		rmap 		= &pmap->roumap[i] ;
		rmap->myid 	= i ;
		rmap->mod 	= (moduleid_t)
					strtol(routers[i] + 11, NULL, 10) ;
		strcpy(		rmap->name, routers[i]) ;
		rmap->part 	= p ;
		get_rou_nic(	fd, &rmap->nic) ;
		get_rou_fence(	fd, &rmap->fence) ;
		get_rou_pmap(	fd, &rmap->portmap) ;

		/* Get the NICs of other routers connected to this. */

		ForEachValidRouterPort(&rmap->portmap,k) {
			rmap->portnic[k-1] = k ;
			get_rou_port_nic(fd, &rmap->portnic[k-1]) ;
		}

		close(fd) ;
	}

	/* Check any interconnections between local routers. */

	if (link_rou_map(pmap) < 0) {
		pmap->cnt = 0 ;
		return -1 ;
	}

	if (debug && (debug_result < 0)) {
		db_printf("Count of routers is %d\n", pmap->cnt) ;
		pmap->cnt = 0 ;
		return -1 ;
	}

	/* Does the data we have fit into the given buffer ? */

	if ((num_rou * sizeof(rou_map_t)) < pmapsize)
		return (PART_ROU_MAP_HDR_SIZE + 
				(num_rou * sizeof(rou_map_t))) ;
	else {
		pmap->cnt = 0 ;
		return -1 ;
	}
}

/*
 * After getting the router maps from all parts, we need to
 * link up all the ports.
 * Support for this follows. Called from check_sanity.
 */

/*
 * LookupPortForNic
 *
 * 	In a router map find the port on which this
 *	NIC is connected.
 *
 * Return:
 *
 * 	port index on success.
 *	-1 on failure.
 */
static int
LookupPortForNic(rou_map_t *rmap, nic_t nic)
{
	int	k ;

        ForEachValidRouterPort(&rmap->portmap,k) {
		if (rmap->portnic[k-1] == nic)
			return k ;
	}
	return -1 ;
}

/* 
 * FillRmapLinkind
 *
 * 	Fill given linkind struct with part id, rmap id and port id
 * 	of the rmap struct, which has the dstnic. Do a global search
 *	of all roumaps. Use srcnic to get the port on the dst rou map.
 *	This function is overloaded. It can be used to verify that dstnic
 *	is within the partitions under consideration.
 *
 * Return:
 *
 * 	1 on success, -1 on failure.
 * 	Fill up rmi.
 *
 */
int
FillRmapLinkind(sys_rou_map_t 	*srm	,	/* Sys rou map */
		nic_t 		dstnic	,	/* Looking for NIC */
		rou_map_ind_t	*rmi	,	/* roumap indx stu to fill in */
		nic_t		srcnic  ,	/* our nic for port srch */
		int		flag)		/* Just search or fill */
{
        int             i, j ;
        part_rou_map_t  *pmap ;
        rou_map_t       *rmap ;

	if ((dstnic == -1) || (srcnic == -1))
		return -1 ;

        for(i=0; i<srm->cnt; i++) {
                pmap = srm->pr_map[i] ;
                for (j=0;j<pmap->cnt; j++) {
                        rmap = &pmap->roumap[j] ;
			if (rmap->nic == dstnic) {
				if (flag && rmi) {
					rmi->part_ind = i ;
					rmi->rmap_ind = j ;
					rmi->port_ind = 
						LookupPortForNic(rmap, srcnic) ;
					if (rmi->port_ind < 0) {
#if 0
printf("p = %d r = %d, %llx\n", i, j, srcnic) ;
#endif
						return -1 ;
					}
				}
				return 1 ;
			}
		}
	}

	return -1 ;
}

/*
 * link_all_roumap
 *
 *	Each router map has a list of nics that are
 * 	connected to each of its port in the portnic
 *	structure. Use this info to find the parameters
 * 	of the router map structure for this port.
 *	The parameters for locating any router map struct
 *	are - partition index, router map index, port id.
 *
 * Returns:
 *
 *	1 for success. -1 for failure.
 *	Gets the linkind struct for each port filled up.
 */
int
link_all_roumap(sys_rou_map_t *sys_rmap_p, char *errbuf)
{
        int     	i, j, k ;
	part_rou_map_t 	*pmap ;
        rou_map_t       *rmap ;
	nic_t		dstnic ;

	/* For all router's in the system ... */

        for(i = 0; i < sys_rmap_p->cnt; i++) {
                pmap = sys_rmap_p->pr_map[i] ;
		for (j = 0 ; j < pmap->cnt; j++) {
			rmap = &pmap->roumap[j] ;

			ForEachValidRouterPort(&rmap->portmap,k) {
			/* 
			 * XXX Check why it does not work without this.
			 * if port ind is not 0 then this port
			 * is connected locally. rmap ind and port
			 * ind are already valid. Just update
			 * part ind from own myid.
			 */
				if (rmap->linkind[k-1].port_ind == 0) {
					dstnic = rmap->portnic[k-1] ;
					/* check if dstnic is in our subset */
					if (FillRmapLinkind(	sys_rmap_p,
								dstnic,
							&rmap->linkind[k-1],
							rmap->nic, 0) < 0)
						continue ;

					if (FillRmapLinkind(sys_rmap_p, dstnic,
						&rmap->linkind[k-1],
						rmap->nic, 1) < 0) {
                                                	sprintf(errbuf,
							msglib[MSG_ROUMAP_ERR],
							rmap->part);
                                                	return -1 ;
                                	}
			     	} else {
					rmap->linkind[k-1].part_ind = 
						rmap->myid ;
			     	}
			}
		}
	}

	return 1 ;
}

/*
 * update_pmap
 *
 * 	pmap, when created - on individual partitions -  does 
 * 	not know where it will be
 * 	installed in sys rou map. This routine is called
 * 	after pmap is installed with the new index. It updates 
 * 	myid in each rou map. myid is later used to update part index.
 */
int
update_pmap(part_rou_map_t *pmap, int indx)
{
	int	i ;

	for (i=0; ((i<pmap->cnt) && (i<PART_ROU_MAP_SIZE)); i++) {
		pmap->roumap[i].myid = indx ;
	}

	if (i>=PART_ROU_MAP_SIZE)
		return -1 ;

	return 1 ;
}

/* support to check connectivity */

/*
 * is_metarouter
 *
 * 	check if the given router is a meta router.
 * 	now it is a kludge. check meta in router name.
 *	Need to add attribute in kernel to meta router
 *	node, export it and get attr.
 */
int
is_metarouter(rou_map_t	*rmap)
{
	if (strstr(rmap->name, "meta"))
		return 1 ;
	else
		return 0 ;
}

/*
 * isconnected
 *
 *	Do a breadth first search for rmdst. Starting from
 *	rmsrc examine only a path that lies in our partition
 * 	and is not visited before.
 *
 * Returns
 *
 *	1 on success, 0 on failure.
 */
int
isconnected(	sys_rou_map_t 	*srm, 		/* sys rou map - const */
		rou_map_t 	*rmsrc, 	/* from router - const */
		rou_map_t	*rmdst, 	/* to   router - const */
		rou_map_t	*rmcur, 	/* current from router */
		int		flag,		/* contiguous or interconn */
		int		mr_flag)	/* meta router system? */
{
	int 		i ;
	rou_map_t	*rmport ;
 
	db_printf("->%llx", rmcur->nic) ;

	rmcur->flags |= RMAP_VISITED ;

	/* Is our guy connected to this router? */

	ForEachValidRouterPort(&rmcur->portmap, i) {
		rmport = GetRouMapForPort(srm, rmcur, i) ;

		/* We found it. */

		if (rmport->nic == rmdst->nic) {
			db_printf("->%llx-|", rmport->nic) ;
			return 1 ;
		}
	}

	/* Continue searching all ports. Ignore ports that 
	 * are already visited and those that do not belong
	 * to the src part.
	 */
        ForEachValidRouterPort(&rmcur->portmap, i) {
		rmport = GetRouMapForPort(srm, rmcur, i) ;
		/* If we already visited this port continue. */
		if (rmport->flags & RMAP_VISITED) {
			db_printf("->%llx=1", rmport->nic) ;
			continue ;
		}

		/* New sanity checks for meta routers. */
		/* 
		 * If we are doing the contiguous check,
		 * make sure that there is no meta router 
		 * within a partition.
		 */
		if (!flag) {		/* if check_contiguous */
			if ((mr_flag) && (is_metarouter(rmport))) {
				/* found meta router within partition */
                                db_printf("->%llx=3", rmport->nic) ;
				return 0 ;
			}
		/*
		 * If we are checking inter connections, make sure
		 * that we have atleast 1 meta router in between
		 * partitions. If not, it means that a cube has
		 * been split further into sub partitions.
		 */
		} else { 		/* if check interconn */
			if ((mr_flag) && (is_metarouter(rmport))) {
                                db_printf("->%llx=4", rmport->nic) ;
				mr_interconn_flag = 1 ;
			}
		}

		/* If the new router does not belong to the 
		 * same part as the src continue. 
		 * If the new router is a meta router ignore
		 * its partition id.
		 */

		if (!is_metarouter(rmport)) {
			if (rmport->new_part != rmsrc->new_part) {
				if (flag && (	rmport->new_part != 
						rmdst->new_part)) {
					db_printf("->%llx=3", rmport->nic) ;
					continue ;
				}
				db_printf("->%llx=2", rmport->nic) ;
				continue ;
			}
		}

		if (isconnected(srm, rmsrc, rmdst, rmport, flag, mr_flag))
			return 1 ;
	}

	return 0 ;
}

/* Write 0 to all rmap->flags fields */

static void
init_rmap_flags(sys_rou_map_t *srm)
{
	int	i, j ;
        part_rou_map_t  *pmap ;
        rou_map_t       *rmap ;

        for(i=0; i<srm->cnt; i++) {
                pmap = srm->pr_map[i] ;
                for (j=0;j<pmap->cnt; j++) {
                        rmap = &pmap->roumap[j] ;
			rmap->flags = 0 ;
		}
	}
}

/*
 * check_rmap
 *
 * 	Given an array of rmap pointers in rmp.
 *	For all combinations 2 routers at a time,
 *	check if they are connected. Checks need
 *	to be done in one direction only.
 */
int
check_rmap(sys_rou_map_t *srm, rou_map_t **rmp, int rmcnt, char *errbuf, 
		int mr_flag)
{
	int 	i, j ;

	for(i=0; i< rmcnt ; i++) {
		for (j=0; j < rmcnt; j++) {

			/* Need to evaluate in only 1 direction. */
			/* Upper half of sparse matrix. */

			if (j<=i) continue ;

			db_printf("checking %llx to %llx\n", 
					rmp[i]->nic, rmp[j]->nic) ;

			/* Fill flag field of all rmap with 0 */
			/* The recursive routine uses this field
			 * to set a visited flag.
			 */

			init_rmap_flags(srm) ;
			if (isconnected(srm, rmp[i], rmp[j], rmp[i], 0, 
					mr_flag)) {
				db_printf("\nConnections OK\n") ;
			}
			else {
				char 	tmp1[MAX_HWG_PATH_LEN] ,
					tmp2[MAX_HWG_PATH_LEN] ;
				db_printf("\n") ;
				strcpy(tmp1, rmp[i]->name) ;
				strcpy(tmp2, rmp[j]->name) ;

				strip_mon(tmp1) ;
				strip_mon(tmp2) ;

				if (mr_flag)
					sprintf(errbuf, 
					"Path between %s and %s \n"
					"has a meta-router in between"
					" OR is not contained within"
					" a single partition.\n", tmp1, tmp2) ;
				else
					sprintf(errbuf, 
					"Path between %s and %s \n"
					"is not contained within"
					" a single partition.\n", tmp1, tmp2) ;
				return -1 ;
			}
			db_printf("------------------------------------\n") ;
		}
	}
	return 1 ;
}

/* 
 * add_roumap
 * 
 *	Add to array rmp, all the routers in module m.
 */
int
add_roumap(sys_rou_map_t *srm, moduleid_t m, rou_map_t **rmp, int ri)
{
	int 		i, j ;
	rou_map_t	*rm ;
	int		k=ri ;
	part_rou_map_t	*prm ;

	for (i=0; i< srm->cnt; i++) {
		prm = srm->pr_map[i] ;
		for (j=0; j< prm->cnt; j++) {
			rm = &prm->roumap[j] ;

			if ((rm->mod == m) && (!is_metarouter(rm))) {
				rmp[k++] = rm ;
			}
		}
	}
	return k ;
}

void
dump_new_part(sys_rou_map_t *srm)
{
        int     i, j ;
        part_rou_map_t  *pmap ;
        rou_map_t       *rmap ;

        for(i=0; i<srm->cnt; i++) {
                pmap = srm->pr_map[i] ;
                for (j=0;j<pmap->cnt; j++) {
                        rmap = &pmap->roumap[j] ;
                        printf("n = %llx, np = %d, ", rmap->nic,
						rmap->new_part) ;
                }
        }
	printf("\n") ;
}

/*
 * init_rmap_new_part
 *
 *	Init the new_part field of rmap with the partition
 * 	number it is going to get
 */
void
init_rmap_new_part(sys_rou_map_t *srm, partcfg_t *pch)
{
        int     	i, j ;
        part_rou_map_t  *pmap ;
        rou_map_t       *rmap ;
	partcfg_t	*pc ;

        for(i=0; i<srm->cnt; i++) {
                pmap = srm->pr_map[i] ;
                for (j=0;j<pmap->cnt; j++) {
                        rmap = &pmap->roumap[j] ;

			pc = partcfgLookupPart(pch, -1, rmap->mod) ;
			if (pc)
                        	rmap->new_part =  partcfgId(pc) ;
			else {
				/* We have a router that does not
				 * fit into the new partition scheme.
				 * Ignore it. If it is an error some
				 * other routine will catch it.
				 */
				/* XXX */
			}
                }
        }

	if (debug)
		dump_new_part(srm) ;
}

/*
 * check_contiguous
 *
 *	1. Is there atleast one route between all pairs of routers in a
 *	   partition where all interconnecting routers belong to the
 *	   same partition.
 */
int
check_contiguous(	sys_rou_map_t 	*srm,	/* current sys rou map */
			partcfg_t 	*pch, 	/* required part cfg */
			char 		*errbuf,/* error string */
			int		mr_flag)/* Meta routers ?? */
{
	int 		i, k = 0 ;
        partcfg_t 	*pc = pch ;
	rou_map_t	*rmp[MAX_ROU_PER_PART] ;/* routers in NEW partition */

	bzero((void *)rmp, sizeof(rmp)) ;

	/*
 	 * Fill all rmap->new_part with their new partition nos.
	 */

	init_rmap_new_part(srm, pch) ;

	/* 
	 * For all NEW required partitions get all the routers
	 * that WILL be in that new partition. Check all router
	 * combinations for connectivity.
	 */

	while (pc) {
		k = 0 ;
		for (i=0; i< partcfgNumMods(pc); i++) {
			k = add_roumap(srm, partcfgModuleId(pc, i), rmp, k) ;
		}

		db_printf("------------------------------------\n") ;
		db_printf("Routers in NEW Part: ") ;
		for (i=0;i<k;i++)
			db_printf("%llx, ", rmp[i]->nic) ;
		db_printf("\n") ;
			
		if (check_rmap(srm, rmp, k, errbuf, mr_flag) < 0)
			return -1 ;

		pc = partcfgNext(pc) ;
	}
	return 1 ;
}

/*
 * check_rmap_interconn
 *
 *	For all combinations of routers, one from each partition,
 *	is there atleast 1 route that goes only between these 2
 *	parts.
 *
 * Returns:
 *
 *	1 on success, 0 on failure.
 */
int
check_rmap_interconn(	sys_rou_map_t   *srm,
			rou_map_t	*rou_map_ptrs_A[],
			int		num_rou_A,
			rou_map_t	*rou_map_ptrs_B[],
			int		num_rou_B,
			int		mr_flag)
{
        int     i, j ;

	/* One from rmap A and one from rmap B */

        for(i=0; i< num_rou_A ; i++) {
                for (j=0; j < num_rou_B ; j++) {
			db_printf("checking %llx to %llx\n", 
					rou_map_ptrs_A[i]->nic, 
					rou_map_ptrs_B[j]->nic) ;

			/* all rmap flag fields to 0 */

			init_rmap_flags(srm) ;

			if (isconnected(srm, 	rou_map_ptrs_A[i],
						rou_map_ptrs_B[j],
						rou_map_ptrs_A[i], 
						1, mr_flag)) {
				db_printf("\n") ;

				/* We found atleast 1 route, go back */

				return 1 ;
			} else {
				db_printf("\n") ;
			}
		}
	}

	/* All combinations failed. No hope. */

	return 0 ;
}

/*
 * check_interconn_parts
 *
 *	Check if 2 partitions are interconnected by
 * 	finding a route between any 2 routers in them.
 *
 * Returns:
 *
 *	1 on success, -1 on failure.
 */
int
check_interconn_parts(	sys_rou_map_t   *srm,
			partcfg_t       *part1,
			partcfg_t       *part2,
			int		mr_flag)
{
	int		i, k ;
	int		num_rou_A, num_rou_B ;
        rou_map_t       *rou_map_ptrs_A[MAX_ROU_PER_PART] ;
        rou_map_t       *rou_map_ptrs_B[MAX_ROU_PER_PART] ;

        bzero((void *)rou_map_ptrs_A, sizeof(rou_map_ptrs_A)) ;
        bzero((void *)rou_map_ptrs_B, sizeof(rou_map_ptrs_B)) ;

	/* Collect all rou map pointers of Partition A and B separately. */

	k=0 ;
	for (i=0;i<partcfgNumMods(part1);i++) {
                k = add_roumap(srm, partcfgModuleId(part1, i), 
					rou_map_ptrs_A, k) ;
	}
	num_rou_A = k ;

	k=0 ;
	for (i=0;i<partcfgNumMods(part2);i++) {
                k = add_roumap(srm, partcfgModuleId(part2, i), 
					rou_map_ptrs_B, k) ;
	}
	num_rou_B = k ;

	/* Bunch of debug printfs */

	db_printf("------------------------------------\n") ;
	db_printf("Routers in Part A: ") ;
	for (i=0;i<num_rou_A;i++)
		db_printf("%llx, ", rou_map_ptrs_A[i]->nic) ;
	db_printf("\n") ;

	db_printf("Routers in Part B: ") ;
	for (i=0;i<num_rou_B;i++)
		db_printf("%llx, ", rou_map_ptrs_B[i]->nic) ;
	db_printf("\n") ;

	mr_interconn_flag = 0 ;
	if ((!check_rmap_interconn(srm, 	rou_map_ptrs_A, num_rou_A, 
					rou_map_ptrs_B, num_rou_B, mr_flag)) 
		 || ((mr_flag) && (!mr_interconn_flag))) {
		return -1 ;
	}

	return 1 ;
}

/*
 * check_interconn
 *
 *	For all pairs of partitions, check if there is
 *	atleast 1 route between 2 partitions that does not
 *	pass thro a 3 rd partition.
 *
 * Returns:
 *
 *	-1 on failure, 1 on success
 */
int
check_interconn(        sys_rou_map_t   *srm,   /* current sys rou map */
                        partcfg_t       *pch,   /* required part cfg */
                        char            *errbuf,/* error string */
			int		mr_flag)/* meta router flag */
{
	int		i, j ;
	int		num_part ;
	partcfg_t	*partcfg_ptrs[MAX_PARTITION_PER_SYSTEM] ;
	partcfg_t	*tmp_partcfg_ptr ;

        bzero((void *)partcfg_ptrs, sizeof(partcfg_ptrs)) ;

        init_rmap_new_part(srm, pch) ;

	/* Collect all partition config header ptrs in an array */

	i = 0 ;
	tmp_partcfg_ptr = pch ;
	while (tmp_partcfg_ptr) {
		partcfg_ptrs[i++] = tmp_partcfg_ptr ;
		tmp_partcfg_ptr = partcfgNext(tmp_partcfg_ptr) ;
	}
	num_part = i ;

	/* All combinations of partitions */

	for (i=0;i<num_part;i++) {
		for (j=0;j<num_part;j++) {

			/* they are same in both directions */

			if (j<=i) continue ;

			db_printf("Checking parts %d and %d\n", 
					partcfgId(partcfg_ptrs[i]), 
					partcfgId(partcfg_ptrs[j])) ;

			/* Are they interconnected? */

			if (check_interconn_parts(srm, 	partcfg_ptrs[i],
							partcfg_ptrs[j],
							mr_flag) < 0) {
				if (!mr_flag)
				sprintf(errbuf, "Partitions %d and %d "
						"are not inter-connected\n",
						partcfgId(partcfg_ptrs[i]),
						partcfgId(partcfg_ptrs[j])) ;
				else 
				sprintf(errbuf, "Partitions %d and %d "
						"are sub-divisions of a cube"
						" , an illegal config\n",
						partcfgId(partcfg_ptrs[i]),
						partcfgId(partcfg_ptrs[j])) ;

				return -1 ;
			}
		}
	}

	return 1 ;
}

#define powerof2(num)	(((num) & ((num)-1)) == 0)

/*
 * count_modules_partitions
 *
 *	counts the number of partitions and total modules in 
 *	a set of partitions given by partcfg_hdr_ptr.
 *
 * Returns:
 *
 *	Nothing.
 *	If status pointer is valid foll checks are made and
 * 	status is set to -1 if not satisfied, 1 if it did:
 *		- all partitions have atleast 1 module
 *		- Number of modules in a partition is 
 *		  a power of 2.
 */
void
count_modules_partitions(	partcfg_t 	*partcfg_hdr_ptr,
				int		*num_part	,
				int		*num_mod	,
				int		*status		,
				char		*errbuf)
{
	partcfg_t	*tmp_partcfg_ptr ;
	int		tmp_num_part = 0, tmp_num_mod = 0 ;
	int		num_mod_in_part = 0 ;

	if (status) *status = 1 ;
        tmp_partcfg_ptr = partcfg_hdr_ptr ;
        while (tmp_partcfg_ptr) {
                tmp_num_part++ ;
		num_mod_in_part = partcfgNumMods(tmp_partcfg_ptr) ;
		/* record it if some partition has no modules */
		if (status && (num_mod_in_part == 0)) {
			*status = -1 ;
			if (errbuf)
				sprintf(errbuf, "Empty Partition %d\n", 
					partcfgId(tmp_partcfg_ptr)) ;
		}
                tmp_num_mod += num_mod_in_part ;
		if ((status) && !powerof2(num_mod_in_part)){
			*status = -1 ;
			if (errbuf)
				sprintf(errbuf, 
				"The number of modules in Partition %d "					"is not a power of 2\n", 
				partcfgId(tmp_partcfg_ptr)) ;
		}
                tmp_partcfg_ptr = partcfgNext(tmp_partcfg_ptr) ;
        }
	*num_part = tmp_num_part ;
	*num_mod  = tmp_num_mod  ;
}

/*
 * check_numbers
 *
 * Assumes that partcfg_actual and partcfg_required are valid.
 * Count the number of modules we have in the actual and needed
 * config. 
 *
 * Returns:
 *
 * 	-1 if error 1 if OK.
 */
int
check_numbers(	partcfg_t 	*partcfg_actual, 
		partcfg_t 	*partcfg_required, 
		char 		*errbuf)
{
	int		num_part_required = 0, num_mod_required = 0 ;
	int		num_part_actual = 0, num_mod_actual = 0 ;
	int		status = 0 ;

	count_modules_partitions(partcfg_actual, &num_part_actual,
						 &num_mod_actual,
						 NULL, NULL) ;

	count_modules_partitions(partcfg_required, &num_part_required,
						   &num_mod_required,
						   &status,
						   errbuf) ;

#if 0
	if (num_mod_required != num_mod_actual) {
		strcpy(errbuf, msglib[MSG_ALL_MODULE]) ;
		return -1 ;
	}
#endif

	if (num_part_required == 1)
		return 1 ;

	if (status < 0)
		return -1 ;

	return 1 ;
}




