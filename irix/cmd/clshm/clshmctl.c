/* 
 * clshmctl.c: cmd-line configure for clshm
 *
 */


#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>

#include <sys/SN/clshm.h>
#include <sys/SN/clshmd.h>		/* for MAX_OUTSTAND_MSGS def */



/* we support 1 ctl device -- device 0 -- for now */
#define	MIN_CTL_DEV	'0'
#define	MAX_CTL_DEV	'0'

#define CLSHMD_FILE		"clshmd"
#define CLSHMCTL_PATH		"/hw/clshm/"

extern int errno;

char *usage = "Usage: %s <cmd> <parameters> \n"
	"\t <cmd> must be one of:\n"
        "\t startup   (start up clshm)\n"
        "\t shutdown  (shut down clshm, and clshmd)\n"
	"\t getcfg    (get the current configuration)\n"
	"\t maxpgs    (set max 16K shared pages for this partition)\n"
        "\t maxdpgs   (set max 16K shared pages between daamon & driver)\n"
        "\t timeout   (set timeout for daem to check for driver msgs)\n";
	
static void 		clshm_startup(int fd, int argc, char **argv);
static void 		clshmd_startup(int fd);
static void 		clshm_shutdown(int fd, int argc, char **argv);
static clshm_config_t 	*clshm_getcfg(int fd);
static void		clshm_printcfg(int fd, int argc, char **argv);
static void		clshm_setcfg(int fd, clshm_config_t *cfg);
static void		clshm_maxpgs(int fd, int argc, char **argv);
static void		clshmd_maxpgs(int fd, int argc, char **argv);
static void		clshmd_timeout(int fd, int argc, char **argv);

static	struct	cmd {
	char	*name;
	void	(*func) (int, int, char **);
} cmdtbl[] = {
        "startup",	clshm_startup,	/* start up clshm and clshmd */
	"shutdown",	clshm_shutdown,	/* shut down clshm and clshmd */
	"getcfg",	clshm_printcfg,	/* print clshmctl config */
	"maxpgs",	clshm_maxpgs,   /* set pg limit on each clshm */
	"maxdpgs",	clshmd_maxpgs,  /* set pg limit for clshmd daemon */
	"timeout",	clshmd_timeout, /* set timeout on clshmd daemon */
	0,		0
};

int	gunit = 0;


/* clshm_startup
 * -------------
 * Parameters:
 *	fd:	pipe to the driver
 * 	argc:	make sure there aren't any other parameters
 *	argv:	unused
 *
 * Send the startup command to the driver
 */
/*ARGSUSED*/
static void
clshm_startup(int fd, int argc, char **argv)
{
    clshm_config_t      *cfg;

    if(argc) {
	fprintf(stderr, "Usage: clshmctl startup\n");
	fprintf(stderr, usage, "clshmctl");
	exit(1);	
    }
    cfg = clshm_getcfg(fd);
    if(ioctl(fd, CL_SHM_STARTUP, cfg) < 0)  { 
	/* multiple startups not allowed */
	fprintf(stderr, "clshmctl: couldn't complete startup: shutdown "
		"needed\n");
	perror("clshmctl");
	exit(1);
    }

    clshmd_startup(fd);
}


/* clshmd_startup
 * --------------
 * Parameters:
 *	fd:	pipe to driver
 * 
 * exec off a clshmd.
 */
static  void
clshmd_startup(int fd)
{
    char   arg_unit[2];
    
    sprintf(arg_unit, "%d", gunit);	/* gunit is a global */

    /* need to make sure fd is closed before child is forked */
    /* else, problems matching up open counts */
    close(fd);
    
    if(! fork())  {
	/* child process: exec the clshmd */
	if(execlp(CLSHMD_FILE, CLSHMD_FILE, arg_unit, NULL)) { 
	    fprintf(stderr, "clshmctl startup: failed to exec clshmd\n");
	    perror("clshmctl");
	    exit(1);
	}
    }
}


/* clshm_shutdown
 * --------------
 * Paramters:
 *	fd:	pipe to driver
 * 	argc: 	make sure no more parameters
 * 	argv: 	unsed
 *
 * send off shutdown to the driver and it will worry about shutting down
 * the clshmd daemon.
 */
/*ARGSUSED*/
static void
clshm_shutdown(int fd, int argc, char **argv)
{
    if(argc) {
	fprintf(stderr, "Usage: clshmctl shutdown\n");
	fprintf(stderr, usage, "clshmctl");
	exit(1);	
    }
    if(ioctl(fd, CL_SHM_SHUTDOWN) < 0)  {
	fprintf(stderr, "clshmctl: couldn't complete shutdown\n");
	perror("clshmctl");
	exit(1);
    }
}


/* clshm_getcfg
 * ------------
 * Parameters:
 *	fd: 	pipe to driver
 */
static clshm_config_t *
clshm_getcfg(int fd)
{
    static clshm_config_t cfg;
    
    if(ioctl(fd, CL_SHM_GET_CFG, &cfg) < 0)  {
	fprintf(stderr, "clshmctl: couldn't get current configuration\n");
	perror("clshmctl");
	return NULL;
    }
    return &cfg;
}


/* clshm_printcfg
 * --------------
 * Paramters:
 * 	fd:	pipe to driver
 *	argc:	make sure no more paramters
 *	argv:	unused
 */
/*ARGSUSED*/
static	void
clshm_printcfg(int fd, int argc, char *argv[])
{
    clshm_config_t	*cfg;

    if(argc) {
	fprintf(stderr, "Usage: clshmctl getcfg\n");
	fprintf(stderr, usage, "clshmctl");
	exit(1);	
    }
    cfg = clshm_getcfg(fd);
    if(cfg)   {
	printf("Clshm Configuration Information:\n");
	printf("\t clshm page size: %d\n", cfg->page_size);
	printf("\t clshm major version number: %d\n", cfg->major_version);
	printf("\t clshm minor version number: %d\n", cfg->minor_version);
    	printf("\t clshm max pages: %d\n", cfg->max_pages);
	printf("\t clshm daemon pages for driver communication: %d\n", 
	       cfg->clshmd_pages);
	printf("\t clshm daemon timeout in ms: %d\n", cfg->clshmd_timeout);
    } 
}


/* clshm_setcfg
 * ------------
 * Parameters:
 *	fd: 	pipe to driver
 *	cfg:	pointer to config info to pass to driver.
 */
static void
clshm_setcfg(int fd, clshm_config_t *cfg)
{
    if(ioctl(fd, CL_SHM_SET_CFG, cfg) < 0)  {
	fprintf(stderr, "clshmctl: couldn't set clshm configuration "
		"after startup: shutdown to set\n");
	perror("clshmctl");
	exit(1);
    }
}


/* clshm_maxpgs
 * ------------
 * Parameters:
 *	fd:	pipe to driver
 *	argc:	should be 1
 *	argv:	should have new max pages to set to
 *
 * Get the old config and set new max pages and then set the config in the
 * driver.
 */
static  void
clshm_maxpgs(int fd, int argc, char *argv[])
{
    clshm_config_t      *cfg;
    int         maxpgs;

    if(argc < 1 || (maxpgs = atoi(argv[0])) < 1) {
	fprintf(stderr, "Usage: clshmctl maxpgs <num_pages>\n");
	fprintf(stderr, usage, "clshmctl");
	exit(1);
    }

    cfg = clshm_getcfg(fd);
    if(cfg)    {
    	cfg->max_pages = maxpgs;
    	clshm_setcfg(fd, cfg);
    }
    else  {
        fprintf(stderr, "clshmctl maxpgs: couldn't get current cfg\n");
    }
}


/* clshmd_maxpgs
 * -------------
 * Parameters:
 *	fd:	pipe to driver
 *	argc:	should be 1
 *	argv:	should have new max pages to set to
 *
 * Get the old config and set new max pages for the clshmd daemon and then 
 * set the config in the driver.
 */
static  void
clshmd_maxpgs(int fd, int argc, char *argv[])
{
    clshm_config_t      *cfg;
    unsigned  int       maxpgs;

    if(argc < 1 || (maxpgs = atoi(argv[0])) < 1) {
	fprintf(stderr, "Usage: clshmctl maxdpgs <num_pages>\n");
	fprintf(stderr, usage, "clshmctl");
	exit(1);
    }

    cfg = clshm_getcfg(fd);
    if(cfg)    {
    	cfg->clshmd_pages = maxpgs;
    	clshm_setcfg(fd, cfg);
    }
    else  {
	fprintf(stderr, "clshmctl maxdpgs: couldn't get current cfg\n");
    }
}


/* clshmd_timeout
 * --------------
 * Parameters:
 *	fd:	pipe to driver
 *	argc:	should be 1
 *	argv:	should have new timeout to set to
 *
 * Get the old config and set new daemon timeout value between checking 
 * for driver messages and then set the config in the driver.
 */
static  void
clshmd_timeout(int fd, int argc, char *argv[])
{
    clshm_config_t      *cfg;
    unsigned  int       timeout;

    if(argc < 1 || (timeout = atoi(argv[0])) < 1) {
        fprintf(stderr, "Usage: clshmctl timeout <value in msecs>\n");
	fprintf(stderr, usage, "clshmctl");
        exit(1);
    }

    cfg = clshm_getcfg(fd);
    if(cfg)   {
        cfg->clshmd_timeout = timeout;
        clshm_setcfg(fd, cfg);
    }
    else {
        fprintf(stderr, "clshmctl timeout: couldn't get current cfg\n");
    }
}


main(int argc, char *argv[])
{
    int	    	fd, i, arg = 1;
    char    	n;
    char	devname[32];

    /* get the unit number if there is one included in the form:
     * "clshmctl clshmX ..." where X is the unit number -- shouldn't 
     * really be used right now cause there is only unit number "0" */
    if(arg < argc && strncmp(argv[arg], "clshm", 5) == 0) {
	n = argv[arg][strlen(argv[arg]) - 1];
	
	if(n < MIN_CTL_DEV || n > MAX_CTL_DEV)  {
	    fprintf(stderr, "%s: bad clshm unit number\n", argv[0]);
	    exit(1);
	}

	gunit = n - MIN_CTL_DEV;
	arg++;
    }

    /* not enough args */
    if(arg >= argc)  {
	fprintf(stderr, usage, argv[0]);
	exit(1);
    }


    /* find the correct command */
    for(i = 0; cmdtbl[i].name && strcmp(argv[arg], cmdtbl[i].name); i++)
	;

    arg++;
    

    /* open pipe to driver */
    sprintf(devname, "%s%d/ctl", CLSHMCTL_PATH, gunit);

    fd = open(devname, 0);
    if(fd < 2)  {
        fprintf(stderr, "clshmctl: couldn't open clshm device %s\n",
            devname);
        perror("clshmctl");
	fprintf(stderr, "clshmctl: clshm may not be turned on or may not "
		"be configured correctly\n");
        exit(1);
    }


    if(cmdtbl[i].func) {
	(*cmdtbl[i].func)(fd, argc - arg, &argv[arg]);
     	close(fd);
	exit(0);
    }

    fprintf(stderr, usage, argv[0]);
    close(fd);
    return(1);
}


