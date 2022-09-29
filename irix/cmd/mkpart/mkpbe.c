
#ident "irix/cmd/mkpbe.c: $Revision: 1.5 $"

/*
 * mkpbe.c - mkpart utility back end
 *
 * 	A command run by mkpart on all partitions using rexec.
 *
 * Syntax:
 *
 * 	mkpbe <modid> <partid> <modid> <partid> ...
 *
 * where 
 *
 * 	modid 	is the module id 
 * 	partid 	is the partition id to be written to modid's MSC
 *
 * Description:
 *
 *	This command changes the contents of the MSC nvram of 
 *	each module in a partition. On the next reboot the module
 *	modid will belong to partition partid. The PROM reads this
 *	value in the MSC nvram and tries to put modid in partid.
 */

#define SN0 1

#include <sys/types.h>
#include <ctype.h>
#include <sys/SN/promlog.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/conf.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>

#include "mkpart.h" 		/* for MAX_MODS_PER_PARTITION */
#include "mkpbe.h"

extern int errno ;

#if 0
#define DEBUG 1
#endif

static int write_partid_to_msc(pcbe_t *, char *, int) ;

char *pcbe_msgs[] = {
#define PCBE_SUCCESS		0
	"",
#define ERR_PCBE_MEM_ALLOC	1
	"mkpbe: No memory.\n",
#define ERR_PCBE_SYNTAX   	2
	"mkpbe: Syntax error.\n",
#define ERR_PCBE_WRITE_NVRAM	3
	"mkpbe: Write NVRAM error.\n",
NULL
} ;

/*
 * args_to_pcbe
 * convert the argv string to pcbe struct.
 * ac is the count of modid partid ... args.
 * av points to the first modid.
 */

static int
args_to_pcbe(int ac, char **av, pcbe_t *p)
{
	int i, j ;

	/* arg count should be even */

	if ((ac%2) || (ac == 0))
		return -1 ;

	for (i=0; i<ac; i+=2) {
		j=i/2 ;
		if (j >= p->nmp)
	    		return -1 ;
		if (!isdigit(av[i][0])) 
	    		return -1 ;
		pcbe_mid(p,j) = atoi(av[i]);
		if (!isdigit(av[i+1][0])) 
	    		return -1 ;
		pcbe_pid(p,j) = atoi(av[i+1]) ;
	}
	p->cnt = j + 1 ;

	return 1 ;
}

/* Format a status string and write it to FILE f */

static void
fmt_put_status(FILE *f, int code, char *s, pcbe_rdata_t *pr)
{
	int			msglen ;

	if (s == NULL)
		s = "" ;

	/* check for msg overflow */

	msglen = 	(strlen(s) > PCBE_RMSG_LEN_MAX) ?
                	PCBE_RMSG_LEN_MAX : strlen(s) ;
	msglen++ ; 	/* for the trailing 0 */

	/* Format return msg */

	pcbe_ret_status(pr) = code ;
	strncpy(pcbe_ret_msg(pr), s, msglen) ;

	fwrite((void *)pr, 1, (msglen + PCBE_RSTAT_LEN), f) ;
}

static void
failure(int code, char *s)
{
	pcbe_rdata_t	pr ;

	bzero(&pr, sizeof(pr)) ;
	fmt_put_status(stderr, code, s, &pr) ;

	exit (1) ;
}

static void
passed(void)
{
	pcbe_rdata_t        pr ;

	bzero(&pr, sizeof(pr)) ;
	fmt_put_status(stdout, PCBE_SUCCESS, NULL, &pr) ;

	exit (0) ;
}

#ifdef DEBUG
static void
pcbeDump(pcbe_t *pcbe)
{
	int i ;

	printf("pcbeCnt = %d\n", pcbe_cnt(pcbe)) ;
	for (i = 0 ; i < pcbe_cnt(pcbe); i++)
		printf("%d %d ", pcbe_mid(pcbe,i), pcbe_pid(pcbe,i)) ;
	printf("\n") ;
}
#endif

static pcbe_t *
pcbeInit(pcbe_t *pcbe, int nmp)
{
	pcbe->cnt = 0 ;
	bzero(pcbe->mp, nmp * sizeof(modpar_t)) ;
	pcbe->nmp = nmp ;
	return pcbe ;
}

static pcbe_t *
pcbeCreate(int nmodpar)
{
	pcbe_t *pcbe ;

	if (pcbe = (pcbe_t *)malloc(sizeof(pcbe_t))) {
		if (!(pcbe->mp = (modpar_t *)malloc(
					nmodpar * sizeof(modpar_t)))) {
	    		free(pcbe) ;
	    		return NULL ;
		}
		pcbeInit(pcbe, nmodpar) ;
	}
	return pcbe ;
}

static void
pcbeFree(pcbe_t *pcbe)
{
	if (!pcbe)
		return ;

	if (pcbe->mp)
		free(pcbe->mp) ;
	free(pcbe) ;
}

/* failure() and passed() exits the program */

void
main(int argc, char **argv)
{
	pcbe_t	*pcbe ;
	char 	errbuf[MAX_ERRBUF_LEN] ;
	int	scan_flag = 0 ;

	if (argc == 1)
		passed() ;

	*errbuf = 0 ; 

	if ((argc > 1) && (!strcmp(argv[1], "-s"))) {
		scan_flag = 1 ;
		argc -= 2 ;
		argv = &argv[2] ;
	} else {
		argc -= 1 ;
		argv = &argv[1] ;
	}

	if ((pcbe = pcbeCreate(MAX_MODS_PER_PART)) == NULL)
		failure(ERR_PCBE_MEM_ALLOC, pcbe_msgs[ERR_PCBE_MEM_ALLOC]) ;

	if (args_to_pcbe(argc, argv, pcbe) < 0) {
		pcbeFree(pcbe) ;
		failure(ERR_PCBE_SYNTAX, pcbe_msgs[ERR_PCBE_SYNTAX]) ;
	}

#ifdef DEBUG
    	pcbeDump(pcbe) ;
#endif

	if (write_partid_to_msc(pcbe, errbuf, scan_flag) < 0) {
		pcbeFree(pcbe) ;
		failure(ERR_PCBE_WRITE_NVRAM, errbuf) ;
	}

	pcbeFree(pcbe) ;

	passed() ;
}

/*
 * mk_nv_name
 * make a hwgraph string for the nvram device driver
 * for this module.
 */

static void
mk_nv_name(pcbe_t *p, int indx, char *n)
{
	*n = 0 ;
	sprintf(n, HWG_MSC_NVRAM_PATH, pcbe_mid(p,indx)) ;
}

/* Read and Write the actual device. */

static int
do_elsc_nvram(int fd, int op, off_t offset, int *data)
{
	sn0drv_nvop_t       nvop;

	nvop.flags = op ;
	nvop.addr  = offset ;

	if (op == SN0DRV_NVOP_WRITE)
        	nvop.data  = *data ;

	if (ioctl(fd, SN0DRV_ELSC_NVRAM, &nvop)) {
		return -1 ;
	}

	if (op == SN0DRV_NVOP_READ)
        	*data = nvop.data ;

	return 1 ;
}

static char 	*errmsg = 
	"Could not %s MSC NVRAM on Module %d.\n\0";

static void
mk_err_msg(char *errbuf, char *op, pcbe_t *p, int i, int *rvp)
{
	if (*rvp >= 0) {
		sprintf(errbuf, errmsg, op, pcbe_mid(p, i)) ;
		*rvp = -1 ;
	}
}

/*
 * write_partid_to_msc
 * 	
 * Input
 *
 * 	list of modid and the partid to be written.
 *
 * Description
 *
 *	Tries to write all msc nvrams.
 * 	Err msg points to first failure
 */

static int 
write_partid_to_msc(pcbe_t *p, char *errbuf, int scan_flag)
{
	int 		i ;
	int		nvfd ;
	char 		nvdev_name[MAX_DEV_NAME_LEN] ;
	int		pid ;
	int		rv = 1 ;

	for (i = 0; i < p->cnt; i++) {
		mk_nv_name(p, i, nvdev_name) ;

 		nvfd = open(nvdev_name, O_RDONLY) ;
		if (nvfd < 0) {
			mk_err_msg(errbuf, "open", p, i, &rv) ;
			continue ;
		}

		if (do_elsc_nvram(nvfd, SN0DRV_NVOP_READ, 
			NVRAM_PARTITION, &pid) < 0) {
			mk_err_msg(errbuf, "read", p, i, &rv) ;
			goto close_cont ;
		}

		if (scan_flag) {
			goto close_cont ;
		}

		pid = pcbe_pid(p,i) ;
        	if (do_elsc_nvram(nvfd, SN0DRV_NVOP_WRITE, 
        		NVRAM_PARTITION, &pid) < 0) {
			mk_err_msg(errbuf, "write", p, i, &rv) ;
        	}
close_cont:
		close(nvfd) ;
	}

	return rv ;
}



