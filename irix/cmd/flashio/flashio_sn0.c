/***********************************************************************\
*	File:		flashio_ksn0.c					*
*									*
*	Command to flash Proms on IO6 and cpu boards of SN0 system.     *
*	Most of this code is borrowed from prom flash driver		*
*	Header from prom flashprom driver is included below.		*
*									*
\***********************************************************************/

#ident	"$Revision: 1.50 $"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/attributes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#define DEF_IP27_CONFIG_TABLE
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/IP31.h>
#include <sys/SN/promhdr.h>
#include <sys/SN/prom_names.h>
#include <sys/SN/SN0/arch.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/hubmd.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/fprom.h> 

#define CONV_OFF 10000

#define ANYKID		-1
#define MAX_HWG_DEPTH                     100
#define	MAXDEVNAME	1024
#define	IOPROM_OFFSET	0xC00000
#define CPU_PROM    0
#define IO_PROM    1
#define MAX_PROM_LENGTH 917504  /* 64K per segment * 14 segments */

extern int errno;

static int flash_copy_ioprom(char *, char *);
static int print_version_ioprom(char *prom_devpath);
static int print_version_cpuprom(char *prom_devpath);
static void erase_promlog_sectors(char *prom_devpath) ;

int hasmetarouter = 0;
int set_prcreqmax = 0;

int promimg_dump(void);
int fprom_write_buffer(fprom_t *, char *, int, char pref_str[16]);
int flash_writeprom(char *prom_devpath, char *prom_image,
		    size_t prom_mapoffset, int prom_code);
int flash_copy_cpuprom(char *prom_devpath, char *cpufile);
int program_cpu_prom(char *prom_devpath);
int program_io_prom(char *prom_devpath);
int check_if_any_proms_to_flash(char *prom_devpath);

ip27config_t    *verify_config_info;
ip27config_t    *force_config_info;
unsigned int	check_flag ;
unsigned int	full_check_flag ;
unsigned int	cache_size ;

#define	USAGE	\
"\nflash [-a] [-c|n] [-d] [-D] [-f] [-F] [-i] [-m module_id] [-o]\n\
      [-p dir_name] [-P img_name] [-s slot_id] [-s] [-v]\n\
   Flash all appropriate proms with images from /usr/cpu/firmware \n\
     If nothing is specified all cpu and io proms in all modules \n\
     will be flashed.  If just cpu (-c or -n) or io (-i) prom is specified \n\
     all of those type will be flashed.  If -m is specified all proms \n\
     in that module will be flashed.  If both the module and slot (-s) are \n\
     specified only the prom at that location will be flashed.\n\
    -a flashes all proms in parallel rather than in two parallel batches\n\
    -d dumps the prom header from a file to standard output\n\
    -D dumps the whole prom to a file\n\
    -f -F asks for overlay of config information any config info overlay\n\
    -o overrides automatic version checking\n\
    -p takes images from directory in pathname rather than default\n\
       (/usr/cpu/firmware)\n\
    -P takes images directly from file specified in full_pathname\n\
       requires -n or -c for cpu prom image file, or -i for io prom\n\
       image file\n\
    -r nn  set PrcReqMax to nn\n\
    -S flashes proms sequentially rather than in two parallel batches\n\
    -v turns on verbose output\n\
    -V print currently loaded flash PROM version\n"


/* we'll gather up all the proms we want to flash into two groups
   even and odd.  With this we can flash in parallel, but not
   all at once.  This was in case something bad happens when we're
   in the middle of a flash all the prom won't be fried. */

typedef struct prom_elem{
    char hwg_path_name[256];
    struct prom_elem *next;
} prom_list_t;

prom_list_t *even_io_prom = NULL;
prom_list_t *odd_io_prom = NULL;
prom_list_t *even_cpu_prom = NULL;
prom_list_t *odd_cpu_prom = NULL;
int io_prom_turn = 0;
int cpu_prom_turn = 0;

typedef	struct child_info_elem	{
    char	*hwg_path_name;
    pid_t	pid;
    int		exit_status;
} *child_info_elem_t;

typedef	struct	child_info_array	{
	int 			ne;
	int 			responded;
	struct	child_info_elem	child_info[];
} *child_info_array_t;

child_info_array_t	child_info_ap;

#define	sanitize_header(ph)	\
		(((ph)->magic == PROM_MAGIC) && ((ph)->numsegs < PROM_MAXSEGS))

#define MODULE_IDENTIFIER "module/%d/"
#define SLOT_IDENTIFIER "slot/%s/"

#define make_slot_ident(p, s) sprintf(p,  SLOT_IDENTIFIER, s);
#define make_module_ident(p, i) sprintf(p,  MODULE_IDENTIFIER, i);
	
int flash_all_cpuproms = 1;
int flash_all_ioproms = 1;
int flash_which= -1;
char flash_slot[32];
int sn0_verbose = 0;
int all_at_once = 0;
int sequential = 0;
int print_version = 0;
int check_for_prom_to_flash = 0;
int check_version = 0;
int from_alarm_sig = 0;
int override_vers = 0;
int dump_mode = 0;
int full_dump_mode = 0;
int flash_module = -1;
int erase_promlog = 0 ;
char path_name[256];
char img_name[256];
uint PrcReqMax;

void _get_all_config_vals(ip27config_t *);
void _get_subset_config_vals(ip27config_t *, int);
int _verify_config_info(ip27config_t *);
int _fill_in_config_info(ip27config_t *, int);
__uint64_t memsum(void *, size_t);
char *flash_read_imagefile(char *prom_image);

#define ADD_IO_PROM \
new_elem = ((prom_list_t *) malloc(sizeof (prom_list_t))); \
strcpy(new_elem->hwg_path_name, canonical_name); \
if ((io_prom_turn % 2) == 0) { \
	new_elem->next = even_io_prom; \
        even_io_prom = new_elem; \
} \
else { \
	new_elem->next = odd_io_prom; \
        odd_io_prom = new_elem; \
} \
io_prom_turn++; 

#define ADD_CPU_PROM \
new_elem = ((prom_list_t *) malloc(sizeof (prom_list_t))); \
strcpy(new_elem->hwg_path_name, canonical_name); \
if ((cpu_prom_turn % 2) == 0) { \
	new_elem->next = even_cpu_prom; \
        even_cpu_prom = new_elem; \
} \
else { \
	new_elem->next = odd_cpu_prom; \
        odd_cpu_prom = new_elem; \
} \
cpu_prom_turn++; 


/* walk_fn is called by nftw - in it we will check to see if we have 
   found a prom node */

/*ARGSUSED*/
int
walk_fn_and_flash_prom(const char 		*canonical_name,
	const struct stat 	*stat_p,
	int			obj_type,
	struct 	FTW		*ftw_info) 
{

	inventory_t	inv[10];
	int             i;
	int		inv_size = 10 * sizeof(inventory_t);
	prom_list_t     *new_elem;

	for (i=0; i<10; i++) {
		inv[i].inv_class = -1;
	}

	if (strstr(canonical_name, "metarouter")) {
		hasmetarouter++;
	}

	if (attr_get(canonical_name, _INVENT_ATTR,
		     (char *)inv, &inv_size, 0) == -1) {
		perror("error");
		exit(0);
	}

	for (i=0; i<10; i++) {
		if (inv[i].inv_class == INV_PROM) {
			if ((inv[i].inv_type == INV_IO6PROM) &&
			    (flash_which == IO_PROM)) {
				if (flash_module != -1) {
					char slot_ident[256], module_ident[256];
					make_module_ident(module_ident, flash_module);
					if (strlen(flash_slot) > 0) {
						make_slot_ident(slot_ident, flash_slot);
						if ((strstr(canonical_name, slot_ident) != NULL) &&
						    (strstr(canonical_name, module_ident) != NULL)) {
							ADD_IO_PROM
						}
					}
					else {
						if (strstr(canonical_name, module_ident) != NULL) {
							ADD_IO_PROM
						}
					}
				}
				else {
					ADD_IO_PROM
				}
			}
			if ((inv[i].inv_type == INV_IP27PROM) &&
			    (flash_which == CPU_PROM)) {
				if (flash_module != -1) {
					char slot_ident[256], module_ident[256];

					make_module_ident(module_ident, flash_module);
					if (strlen(flash_slot) > 0) {
						make_slot_ident(slot_ident, flash_slot);
						if ((strstr(canonical_name, slot_ident) != NULL) &&
						    (strstr(canonical_name, module_ident) != NULL)) {
							ADD_CPU_PROM
						}
					}
					else {
						if (strstr(canonical_name, module_ident) != NULL) {
							ADD_CPU_PROM
						}
					}
				}
				else {
					ADD_CPU_PROM
				}
			}
		}
		
	}
	return 0;
}


static	void
sig_chld_handler( void )
{
	child_info_elem_t	ciep;
	int			exit_stat;
	int			i;
	pid_t			pid;

	do {
		pid = waitpid ( ANYKID, &exit_stat, WNOHANG );
		if ( pid > 0 ) {

			/*
			 *   a child process has terminated: record
			 *   its exit status and wait for more
			 *   children to finish flashing.
			 */
			for ( i = 0; i < child_info_ap->ne; i++ ) {
				ciep = &(child_info_ap->child_info[i]);
				if ( ciep->pid != pid ) {
					continue;
				}
				ciep->exit_status = WEXITSTATUS ( exit_stat );
				child_info_ap->responded++;
			}
		} else if ( ( pid < 0 ) && ( errno == ECHILD )  && 
			   ( child_info_ap->responded != child_info_ap->ne ) ) {
			printf("%d child process(es) did not respond.\n",
				child_info_ap->ne - child_info_ap->responded);
			child_info_ap->responded = child_info_ap->ne;
		}
	} while ( pid > 0 );
}


/*
 *   allow for an alarm call when waiting for input
 *   from the operator when an error has occurred
 *   during a flash operation.
 */

static void
sig_alrm_handler()
{
	from_alarm_sig++;
}


/* ignore a ctrl-c otherwise if we're in the middle of flashing, 
   the machine will not boot after reset */
static void
sig_int_handler()
{
	printf("\nIf flash is killed in the middle of execution, the machine\n");
	printf("  will freeze after it is reset.  continuing...\n");
}


int
flashio_sn0(int argc, char **argv)
{
	int	c;
	int	count;
	int	error = 0;
	int	exit_stat;
	int	i;
	int     only_io;
	int	option = 0;
	int	size;
	pid_t	pid;
	child_info_elem_t	ciep;
	prom_list_t     *search_elem;
	sigset_t		emptyset;
	sigset_t		set;
	struct sigaction act;
	int exists_prom_to_flash = 0;
	
	act.sa_flags = 0;
	act.sa_handler = sig_int_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	strcpy (path_name, "");
	strcpy (img_name, "");
	strcpy (flash_slot, "");
	check_flag = 0;
	full_check_flag = 0;
	force_config_info = ((ip27config_t *) malloc(sizeof(ip27config_t)));

	while((c = getopt(argc, argv, "acdeDifFm:nop:P:r:s:SvV")) != -1) {
		switch(c){
		case 'a':
			all_at_once = 1;
			break;
		case 'c':
			if (flash_which != -1) {
				fprintf(stderr,
					"Error: choice of proms already specified\n");
				error++;
				break;
			}
			flash_which = CPU_PROM;
			break;
		case 'd':
			sn0_verbose = 1;
			dump_mode = 1;
			break;
		case 'D':
			sn0_verbose = 1;
			full_dump_mode = 1;
			break;
		case 'e':
			sn0_verbose = 1;
			erase_promlog = 1 ;
			break ;
		case 'f':
			check_flag = 1;
			break;
		case 'F':
			full_check_flag = 1;
			break;
		case 'i':
			if (flash_which != -1) {
				fprintf(stderr,
					"Error: choice of proms already specified\n");
				error++;
				break;
			}
			flash_which = IO_PROM;
			break;
		case 'm':
			flash_module = atoi(optarg);
			break;
		case 'n':
			if (flash_which != -1) {
				fprintf(stderr,
					"Error: choice of proms already specified\n");
				error++;
				break;
			}
			flash_which = CPU_PROM;
			break;
		case 'o':
			override_vers = 1;
			break;
		case 'p':
			strcpy(path_name,optarg);
			break;
		case 'P':
			if ( strncmp( optarg, "/", 1 ) ) {
				fprintf(stderr,
					"Error: -P requires a pathname to "
					"start with a slash (/).\n");
				error++;
				break;
			}
			strcpy(img_name,optarg);
			break;
		case 'r':
			set_prcreqmax++;
			PrcReqMax = atoi(optarg);
			if ((PrcReqMax & ~3) != 0) {
				fprintf(stderr,
					"PrcReqMax=%#x is out of range 0..3\n",
					PrcReqMax);
				error++;
			}
			break;
		case 's':
			strcpy(flash_slot,optarg);
			break;
		case 'S':
			sequential = 1;
			break;
		case 'v':
			sn0_verbose = 1;
			break;
		case 'V':
			print_version = 1;
			break;
		case '?':
		default:
			error++;
			break;
		}
	}

	if ( ( strlen(img_name) ) && ( flash_which < 0 ) ) {
		fprintf(stderr,"Error: when you specify -P you "
			"also have to specify either\n-c or -n, "
			"or -i.\n");
		error++;
	}

	if ( (flash_module == -1) && ( strlen(flash_slot) ) ) {
		fprintf(stderr, "Error: you specified slot information\n"
			"(-s %s) without module number (-m).\n",
			flash_slot);
		error++;
	}

	if ( error ) {
		fprintf(stderr, USAGE);
		return ( error );
	}

	if (strlen(path_name) == 0) {
		if (sn0_verbose)
			printf("setting default path_name to %s\n", DEFAULT_PATHNAME);
		strcpy(path_name, DEFAULT_PATHNAME);
	}

	verify_config_info = ((ip27config_t *) malloc(sizeof(ip27config_t)));

	if (dump_mode) {
		promimg_dump();
		return 0;
	}

	/*
 	 * If erase promlog option is chosen, just erase all promlogs
	 * in series, and exit.
	 */
	if (erase_promlog) {

		fprintf(stderr, 
"WARNING: You need to do initlog at the PROM or sn0log -i in the kernel \n"
"to boot the system next time.\n") ;

		/* set flags that help nftw search */
		only_io = 0 ;
		/* only cpu proms have promlogs. */
		flash_which = CPU_PROM ;

		/* Gather all hwg path names of proms in 2 lists. */

        	if (nftw("/hw", walk_fn_and_flash_prom, 
				MAX_HWG_DEPTH, FTW_PHYS)) {
                	perror("error in hw graph walk");
			return 0 ;
		}

		/* Flash promlogs in both lists. */

        	search_elem = even_cpu_prom;
        	while (search_elem != NULL) {
                	erase_promlog_sectors(search_elem->hwg_path_name);
                	search_elem = search_elem->next;
        	}

        	search_elem = odd_cpu_prom;
        	while (search_elem != NULL) {
                	erase_promlog_sectors(search_elem->hwg_path_name);
                	search_elem = search_elem->next;
        	}

		/* Dont bother about anything else now. */

		return 0;
	}

	only_io = 0;
	if ((flash_which == IO_PROM) || (flash_which == -1)) {
		if (flash_which == IO_PROM)
			only_io = 1;
		flash_which = IO_PROM;

		/* easy case - don't need to fix configuration bits */
		/* Program the selected boards */
		if (nftw("/hw", walk_fn_and_flash_prom, MAX_HWG_DEPTH, FTW_PHYS))
			perror("error in hw graph walk");
	
		if (only_io)
			goto start_flash;
	}
	
	/* if we're here that means we need a valid cpu image thus we
	   will need to obtain valid configuration bits 
	   if we're flashing all the proms we'll pick up the specific 
	   config info for each board in flash_all_nodes */
	
	flash_which = CPU_PROM;

	if (nftw("/hw", walk_fn_and_flash_prom, MAX_HWG_DEPTH, FTW_PHYS))
		perror("error in hw graph walk");

	/* go ahead before asking the user for input and see if there's
	   actually going to be anything to flash */
	if ((full_check_flag || check_flag) && (! override_vers)) {
		char *promdata;
		char prom_image[256];
		promhdr_t *ph;

		sprintf(prom_image, "%s/%s.dump", path_name, CPU_PROM_NAME);
		promdata = flash_read_imagefile(prom_image);

		ph = (promhdr_t *)promdata;
		check_version = CONV_OFF*ph->version + ph->revision;

		check_for_prom_to_flash = 1;

		search_elem = even_cpu_prom;
		while (search_elem != NULL) {
			if (program_cpu_prom(search_elem->hwg_path_name))
				exists_prom_to_flash = 1;
			search_elem = search_elem->next;
		}
		search_elem = odd_cpu_prom;
		while (search_elem != NULL) {
			if (program_cpu_prom(search_elem->hwg_path_name))
				exists_prom_to_flash = 1;
			search_elem = search_elem->next;
		}

		check_for_prom_to_flash = 0;
		if (! exists_prom_to_flash) {
			printf("No proms need flashing\n");
			exit(0);
		}
	}

	if (full_check_flag) {
		_get_all_config_vals(force_config_info);
	}
	else if (check_flag) {
		_get_subset_config_vals(force_config_info, 1);
		if (! _verify_config_info(force_config_info)) {
			printf("invalid configuration values\n");
			return 1;
		}
	}
	else ;

    start_flash:

	count = io_prom_turn + cpu_prom_turn;
	if ( count ) {
		if ( !sequential ) {

			/*
			 *   set up the interrupt handler
			 *   to keep track of the completion
			 *   of the child processes.
			 */

			act.sa_flags = 0;
			act.sa_handler = sig_chld_handler;
			sigemptyset(&act.sa_mask);
			sigaddset(&act.sa_mask, SIGCHLD);
			sigaction(SIGCHLD, &act, NULL);
		
			sigemptyset(&set);
			sigemptyset(&emptyset);
			sigaddset(&set, SIGCHLD);
			sigprocmask(SIG_BLOCK, &set, NULL);
	
		}

		size  = sizeof ( struct child_info_elem ) * ( count );
		size += sizeof ( struct child_info_array );

		act.sa_flags   = 0;
		act.sa_handler = sig_alrm_handler;
		sigemptyset(&act.sa_mask);
		sigaction(SIGALRM, &act, NULL);
	}

      wait_for_children:
	
	if ( count ) {
		child_info_ap = ( struct child_info_array * ) calloc (1, size );
		if ( !child_info_ap ) {
			printf("unable to obtain memory for child_info_array.\n");
			return 1;
		}
	}
	
	/* okay we have gathered up the proms we need to flash into two
	   groups flash the even then the odd ones */

	search_elem = even_io_prom;
	while (search_elem != NULL) {
		program_io_prom(search_elem->hwg_path_name);
		search_elem = search_elem->next;
	}

	search_elem = even_cpu_prom;
	while (search_elem != NULL) {
		program_cpu_prom(search_elem->hwg_path_name);
		search_elem = search_elem->next;
	}

	if ( !all_at_once && !sequential ) { 
		while ( ( child_info_ap->ne - child_info_ap->responded ) > 0 ) {
			sigsuspend(&emptyset);
		}
	}
	
	search_elem = odd_io_prom;
	while (search_elem != NULL) {
		program_io_prom(search_elem->hwg_path_name);
		search_elem = search_elem->next;
	}

	search_elem = odd_cpu_prom;
	while (search_elem != NULL) {
		program_cpu_prom(search_elem->hwg_path_name);
		search_elem = search_elem->next;
	}

	if ( !sequential ) {
		while ( (child_info_ap->ne - child_info_ap->responded) > 0 ) {
			sigsuspend(&emptyset);
		}
	}

	/*
	 *   all child process have finished: take stock of each
	 *   of their exit status and if one or more completed with
	 *   an error, provide the user with intervention options.
	 */
	
	error = 0;
	for ( i = 0; i < child_info_ap->ne; i++ ) {
		ciep = &(child_info_ap->child_info[i]);
		if ( ciep->exit_status ) {
			if ( !error ) {
				printf( "\nAn error was encountered "
					"during programming the "
					"following PROM(s):\n");
			}
			printf( "%s> Error programming PROM: "
				"errno: %d.\n", ciep->hwg_path_name,
				ciep->exit_status );
			error++;
		}
	}

	if ( !error ) {
		return ( 0 );
	}
	
	/*
	 *   one or more errors have been encountered.
	 */

	printf("\nenter \"[r]etry\" or \"[i]gnore\" "
	       "to continue: [r] ");

	option = 0;
	while ( !option ) {
		alarm ( 60 ); 
		option = getchar();
		alarm ( 0 );
		if ( from_alarm_sig ) {
			from_alarm_sig = 0;
			option = 'i';
			printf("\ntimed out after 60 seconds: "
				"[i]gnore assumed.\n");
		} else if ( option != '\n' ) {
			alarm ( 60 );
			while ( getchar() != '\n' );
			alarm ( 0 );
		} else {
			option = 'r';
		}
		switch ( option ) {
		case 'r':
			free ( child_info_ap );
			goto wait_for_children;
		case 'i':
			break;
		default:
			printf("\nYour selection %c is not valid.\n",
				option);
			option = 0;
			continue;
		}
		break;
	}
	free ( child_info_ap );
	return ( 0 );
}


int
program_io_prom(char *prom_devpath) {
	char prom_image[256];


	if (print_version) {
		return print_version_ioprom(prom_devpath);
	}
	if (full_dump_mode) {
		sprintf(prom_image, "%s/%s.dump", path_name, IO_PROM_NAME);

		if (sn0_verbose)
			printf("going to read from %s and write to %s\n", 
			       prom_devpath,prom_image);

		return flash_copy_ioprom(prom_devpath, prom_image);

	}
	if (strlen(img_name) > 0)
		sprintf(prom_image, "%s", img_name);
	else 
		sprintf(prom_image, "%s/%s", path_name, IO_PROM_NAME);

	return flash_writeprom(prom_devpath, prom_image, 
				IOPROM_OFFSET, FPROM_DEV_IO6_P1);

}

int
program_cpu_prom(char *prom_devpath) {
	char prom_image[256];

	if (check_for_prom_to_flash) {
		return check_if_any_proms_to_flash(prom_devpath);
	}
	if (print_version) {
		return print_version_cpuprom(prom_devpath);
	}
	if (full_dump_mode) {
		sprintf(prom_image, "%s/%s.dump", path_name, CPU_PROM_NAME);
		if (sn0_verbose)
			printf(" going to read from %s and write to %s\n", 
			       prom_devpath,prom_image);

		return flash_copy_cpuprom(prom_devpath, prom_image);
	}
	if (strlen(img_name) > 0)
		sprintf(prom_image, "%s", img_name);
	else 
		sprintf(prom_image, "%s/%s", path_name, CPU_PROM_NAME);

	return flash_writeprom(prom_devpath, prom_image, 
				0, FPROM_DEV_HUB);

}



void
dump_promseg(promseg_t *seg, char pref_str[16])
{

	if (sn0_verbose) {
		printf("%s  Name:        %s\n", pref_str, seg->name);
		printf("%s  Flags:       0x%llx\n", pref_str, seg->flags);
		printf("%s  Offset:      0x%llx\n", pref_str, seg->offset);
		printf("%s  Entry:       0x%llx\n", pref_str, seg->entry);
		printf("%s  Ld Addr:     0x%llx\n", pref_str, seg->loadaddr);
		printf("%s  True Length: 0x%llx\n", pref_str, seg->length);
		printf("%s  True sum:    0x%llx\n", pref_str, seg->sum);
		
		if (seg->flags & SFLAG_COMPMASK) {
			printf("%s  Cmprsd len:  0x%llx\n", pref_str, seg->length_c);
			printf("%s  Cmprsd sum:  0x%llx\n", pref_str, seg->sum_c);
		}
	}
}

void
dump_promhdr(promhdr_t *ph, char pref_str[16])
{
	int		i;

	if (sn0_verbose) {
		printf("%sPROM Header contains:\n", pref_str);
		printf("%s  Magic:    0x%llx\n", pref_str, ph->magic);
		printf("%s  Version:  %lld.%lld\n", pref_str, ph->version, ph->revision);
		printf("%s  Length:   0x%llx\n", pref_str, ph->length);
		printf("%s  Segments: %lld\n", pref_str, ph->numsegs);
		
		if (ph->numsegs > 6) {
			printf("%serror: corrupt segment count in segldr header (%lld)\n", 
			       pref_str, ph->numsegs);
			return ;
		}
		
		for (i = 0; i < ph->numsegs; i++) {
			printf("%sSegment %d:\n", pref_str, i);
			dump_promseg(&ph->segs[i], pref_str);
		}
	}

}

void *
map_prom(char *path, off_t offset, size_t size)
{
	void	*mapaddr;
	int	fd;
	char	errbuf[MAXDEVNAME];

	fd = open(path, O_RDWR);
	if (fd < 0 ) {
		sprintf(errbuf, "Opening %s", path);
		perror(errbuf);
		if (getuid() != 0)
			printf("  try executing as superuser\n");
		exit ( EACCES );
	}

	mapaddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);

	if (mapaddr == (void *)-1){
		sprintf(errbuf, "Mapping %s file", path);
		perror(errbuf);
		close(fd);
		return 0;
	}

	return mapaddr;
}

void
prom_check(fprom_t *f)
{
	promhdr_t	phdr;

	if (fprom_read(f, 0, (char *)&phdr, sizeof(promhdr_t))) {
		printf("Error reading prom header.\n") ;
		return ;
	}
	dump_promhdr(&phdr, "") ;
}

char *prom_types[] = {"Hub", "P0 IO6", "P1 IO6"};
int
flash_readprom(char *prom_devpath, char *promdata, size_t prom_mapoffset,
	       size_t prom_size, int prom_code, int read_verbose, char pref_str[16])
	       
{
	int		retval;
	fprom_t		f;
	promhdr_t	*ph;
	int		segnum;
	int		r,i;
	int		manu_code, dev_code;
	ip27config_t    *new_config_info;


	f.base = map_prom(prom_devpath, prom_mapoffset, prom_size);

	if (!f.base)
		return 0;
	/*
	 * afn and aparm are abort function and its args.
	 * They are null for now.
	 */
	f.afn = 0; 
	for (i=0;i<8;i++) {
		f.aparm[i] = 0;
	}
	f.dev = prom_code;

	r = fprom_probe(&f, &manu_code, &dev_code);

	if (r < 0 && r != FPROM_ERROR_DEVICE) {
		printf("Error initializing %s PROM: %s\n",
			prom_types[prom_code], fprom_errmsg(r));
		return 0 ;
	}

	if (r == FPROM_ERROR_DEVICE)
		printf("*** Warning: Unknown manufacture/device code\n");

	if (prom_code == FPROM_DEV_HUB) {
		/*
		 * CPU Prom has no special segments or any such info.
		 * Just read the prom into the buffer,
		 * and return 
		 */

		retval = fprom_read(&f, 0, promdata, prom_size);

		if ((sn0_verbose && read_verbose) || print_version) {
			new_config_info = (ip27config_t *) 
				(promdata + CONFIG_INFO_OFFSET);
			printf(" Prom version %d.%d\n", new_config_info->pvers_vers,
			       new_config_info->pvers_rev);
		}

		return (retval == FPROM_ERROR_NONE) ? prom_size : 0 ;
	}

	/*
	 * Sequence of operation for the IOprom case.
	 * 	- First read prom header and check its sanity.
	 * 	- 
	 * First read in the prom header. 
	 */
	retval = fprom_read(&f, 0, promdata,  sizeof(promhdr_t));

	if (retval != FPROM_ERROR_NONE) {
		printf("Error in reading Flash prom retval %d\n", retval);
		return  0;
	}

	/*
	 * Now scan each segment, and read in the individual segments.
	 */
	ph = (promhdr_t *)promdata;
	if (sanitize_header(ph) == 0) {
		printf("Invalid Header \n");
		dump_promhdr(ph, "");
		return 0;
	}

	if ((sn0_verbose && read_verbose) || print_version)
		printf(" Prom version %lld.%lld\n", ph->version, ph->revision);

	for (segnum = 0; segnum < ph->numsegs; segnum++) {
		promseg_t	*seg;

		seg = &ph->segs[segnum];


		if (sn0_verbose && read_verbose) {
			printf("%s Segment \n", pref_str);
			dump_promseg(seg, pref_str);
		}

		retval = fprom_read(&f, seg->offset, 
					&promdata[seg->offset], seg->length);

		if (retval != FPROM_ERROR_NONE) {
			printf("Error in reading Segment retval %d\n", retval);
			return 0;
		}

	}

	return ph->length;
}

char *
flash_read_imagefile(char *prom_image)
{
	struct stat 	buf;
	char		*promdata;
	int		fd;

	/*
	 * Check if the data file exists.
	 */

	if (stat(prom_image, &buf) < 0){
		printf("File %s does not exist\n", prom_image);
		return 0;
	}

	if (!S_ISREG(buf.st_mode)){
		printf("File %s is not a regular file\n", prom_image);
		return 0;
	}

	promdata = malloc(FPROM_SIZE);
	if (!promdata) {
		printf("Unable to allocate Memory size %d\n", FPROM_SIZE);
	}

	/*
	 * Read in the prom imgage. 
	 */
	fd = open(prom_image, 0);
	if (fd < 0) {
		perror("open");
		return 0;
	}
	read(fd, promdata, FPROM_SIZE);
	close(fd);

	return promdata;
}

int
flash_writeprom(char *prom_devpath, char *prom_image,
			size_t prom_mapoffset, int prom_code)
{
	child_info_elem_t	ciep;
	fprom_t		f;
	int		rtn_val = 0;
	promhdr_t	*ph;
	int		r,i;
	int		manu_code, dev_code;
	char		*promdata;
	char		*in_core_promdata;
	int		promlen;
	ip27config_t    *config_info;
	ip27config_t    *new_config_info;
	int             pid;
	int             compare_bytes, pref_index;
	char            pref_str[16];
	char            locate_str[64];
        __uint64_t      old_sum, new_sum;
	int             version, in_version;
	int             flash_count;
	hubstat_t	*buf;
	int		fd;
	char 		*p;
	char		hub_devpath[256];
	int		pimm_psc;

	strcpy(pref_str, "");
	pid = -17;
	if (! sequential) {
		switch ((pid = fork())) {
		      case 0:
			/* child process - will go ahead and do the work */
			break;
		      case -1:
			if (sn0_verbose)
				printf("could not complete fork - all work being done by parent\n");
			break;
		      default:
			/* 
			 *   we have a child process, save its pid and 
			 *   return with the parent to flash next prom.
			 */
			ciep = &(child_info_ap->child_info[child_info_ap->ne]);
			ciep->hwg_path_name = prom_devpath;
			ciep->pid           = pid;
			ciep->exit_status = ECHILD;
			child_info_ap->ne++;
			return 0;
		}
	} else {
		ciep = &(child_info_ap->child_info[child_info_ap->ne]);
		ciep->hwg_path_name = prom_devpath;
		ciep->exit_status = ECHILD;
		child_info_ap->ne++;
	}

	/* we want to identify all the printfs with a unique preface */

	pref_str[0] = 'm';
	pref_index = 1;
	if (strstr(prom_devpath, "module") != NULL) {
		strcpy(locate_str, strstr(prom_devpath, "module"));
	}
	i=7;
	while (i<strlen(locate_str)) {
		pref_str[pref_index] = locate_str[i];
		pref_index ++;
		i++;
		if (locate_str[i] == '/')
			break;
	}
	pref_str[pref_index] = ' ';
	pref_index ++;
	if (strstr(prom_devpath, "slot") != NULL) {
		strcpy(locate_str, strstr(prom_devpath, "slot"));
	}
	i=5;
	while (i<strlen(locate_str)) {
		pref_str[pref_index] = locate_str[i];
		pref_index ++;
		i++;
		if (locate_str[i] == '/')
			break;
	}
	pref_str[pref_index] = ':';
	pref_str[pref_index+1] = ' ';
	pref_str[pref_index+2] = '\0';			

	promdata = flash_read_imagefile(prom_image);
	new_sum = memsum((void *)(promdata+PROM_DATA_OFFSET), 891584)%256;

	if (prom_code == FPROM_DEV_HUB) {
		ph = (promhdr_t *)promdata;
		if (strcmp(ph->segs[0].name, "ip27prom") != 0) {
			printf("%serror: attempted flash of cpu prom with something other than a cpu prom image\n",pref_str);
			rtn_val = EINVAL;
			goto end_no_verify_flash_write;
		}

		new_config_info = (ip27config_t *) (promdata + PROM_DATA_OFFSET + CONFIG_INFO_OFFSET);

		old_sum = memsum((void *)(new_config_info),
				 sizeof(ip27config_t))%256;

		in_core_promdata = malloc(FPROM_SIZE);

		if (!in_core_promdata) {
			printf("%sUnable to allocate Memory size %d\n", pref_str, FPROM_SIZE);
		}
 		bzero(in_core_promdata, FPROM_SIZE);
		flash_readprom(prom_devpath, in_core_promdata,  
			       0, FPROM_SIZE, FPROM_DEV_HUB, 0, pref_str);

		config_info = (ip27config_t *) (in_core_promdata + CONFIG_INFO_OFFSET);
		
		in_version = CONV_OFF*config_info->pvers_vers + config_info->pvers_rev;

		config_info->flash_count = config_info->flash_count + 1;
		config_info->pvers_vers = new_config_info->pvers_vers;
		config_info->pvers_rev = new_config_info->pvers_rev;

		flash_count = config_info->flash_count;

		if ((full_check_flag) || (check_flag)) {
			bcopy(force_config_info, new_config_info, 
			      sizeof(ip27config_t));
		} else {
			bcopy(config_info, new_config_info,
				sizeof(ip27config_t));
		/* here try to get the node info to check for ip31 boards */
		buf = (hubstat_t *)malloc(sizeof(hubstat_t));
		strcpy(hub_devpath, prom_devpath);
		if((p = strstr(hub_devpath,"prom")) == NULL)
			goto cont;
		strcpy(p,"hub/mon");
		
		fd = open(hub_devpath, O_RDONLY);
		if(fd <= 0)
			goto cont;
		if((pimm_psc = ioctl(fd, SN0DRV_GET_PIMM_PSC)) <0) {
			;
		}
		else
		{
			if(pimm_psc != PIMM_NOT_PRESENT)
			{
			/* load the config bits from table */
				ip31_get_config(pimm_psc, new_config_info); 
			}
		}
		}
cont: 
		if (set_prcreqmax == 0) {
			PrcReqMax = 3;
			set_prcreqmax++;
		}
		if (set_prcreqmax) {
			new_config_info->r10k_mode =
				(new_config_info->r10k_mode & ~(3 << 7))
					| (PrcReqMax << 7);
		}

		new_config_info->check_sum_adj = 0;
		new_sum = memsum((void *)(new_config_info),
				 sizeof(ip27config_t))%256;

		/* now set the appropiate check sum adjust value so
		   that we're back to summing to 0 % 256 */
		if (old_sum>new_sum) {
			new_config_info->check_sum_adj = (uint) (old_sum - new_sum);
		}
		else {
			new_config_info->check_sum_adj = (uint) (256 - (new_sum - old_sum));
		}
		new_config_info->check_sum_adj = new_config_info->check_sum_adj & 0xff;
		
		bcopy(new_config_info, verify_config_info, 
		      sizeof(ip27config_t));

		new_sum = memsum((void *)(new_config_info),
				 sizeof(ip27config_t))%256;

		if  (sn0_verbose)
			printf("%sfreq cpu %lld freq hub %lld sn0/0 bit %d\n", pref_str,
			       new_config_info->freq_cpu,
			       new_config_info->freq_hub,
			       new_config_info->mach_type);

	}
	else {
		ph = (promhdr_t *)promdata;
		if (strcmp(ph->segs[0].name, "io6prom") != 0) {
			printf("%serror: attempted flash of io prom with something other than a io prom image\n",pref_str);
			rtn_val = EINVAL;
			goto end_no_verify_flash_write;
		}

		in_core_promdata = malloc(FPROM_SIZE);

		if (!in_core_promdata) {
			printf("%sUnable to allocate Memory size %d\n", pref_str, FPROM_SIZE);
		}
 		bzero(in_core_promdata, FPROM_SIZE);
		flash_readprom(prom_devpath, in_core_promdata, 
			       IOPROM_OFFSET, FPROM_SIZE, 
			       FPROM_DEV_IO6_P1, 0, pref_str);
		ph = (promhdr_t *)in_core_promdata;
		in_version = CONV_OFF*ph->version + ph->revision;

	}

	ph = (promhdr_t *)promdata;
	version = CONV_OFF*ph->version + ph->revision;


	/* the old revision numbering scheme was broke.  For version
	 * 3.2 day they cheated and just used one number 320 so we need
	 * to check for this case and make sure we still flash even
	 * if the version is something like 320.320  I suppose at some
	 * we might need to be careful if the prom version numbering
	 * gets above 100.
	 */

	if (in_version > 100*CONV_OFF) 
		goto skip_check;

	if ((in_version == version) && (! override_vers)) {
		printf("NOTE: %sprom file version %d.%d is the same as "
			"current version %d.%d: prom not flashed\n",
			pref_str, version/CONV_OFF, version%CONV_OFF,
			in_version/CONV_OFF, in_version%CONV_OFF);
		rtn_val = 0;
		goto end_no_verify_flash_write;
	}

	if ((in_version >= version) && (! override_vers)) {
		printf("%sprom file version %d.%d is older than current "
			"version %d.%d: prom not flashed\n",
			pref_str, version/CONV_OFF, version%CONV_OFF,
			in_version/CONV_OFF, in_version%CONV_OFF);
		rtn_val = 0;
		goto end_no_verify_flash_write;
	}

      skip_check:;
	if (prom_code == FPROM_DEV_HUB)
		printf("%sFlashed this prom %d times\n", pref_str, flash_count);

	if (!promdata) {
		rtn_val = EINVAL;
		goto end_flash_write;
	}

	if (sn0_verbose) {
		printf("%sFlashing prom data in file %s\n", pref_str, prom_image);
		printf("%s                 to device %s\n", pref_str, prom_devpath);
	}

	if (prom_code == FPROM_DEV_HUB)
		f.base = map_prom(prom_devpath, prom_mapoffset, 8*FPROM_SIZE);
	else
		f.base = map_prom(prom_devpath, prom_mapoffset, FPROM_SIZE);

	if (!f.base) {
		rtn_val = ENODEV;
		goto end_no_verify_flash_write;
	}

	/*
	 * Check the number of segments in the prom, and flash
	 * accordingly.
	 * Note that the prom header starts from the beginning of the
	 * file. So, pointing ph to promdata should be sufficient
	 * to get to the prom header.
	 */

	if ((ph->length  - ph->segs[0].offset) > MAX_PROM_LENGTH) {
		printf("%serror: prom too large %lld - programming failed.\n", pref_str,ph->length);
		rtn_val = EFBIG;
		goto end_flash_write;
	}

	
	/*
	 * afn and aparm are abort function and its args.
	 * They are null for now.
	 */
	f.afn = 0; 
	for (i=0;i<8;i++) {
		f.aparm[i] = 0;
	}
	f.dev = prom_code;

	r = fprom_probe(&f, &manu_code, &dev_code);

	if (r < 0 && r != FPROM_ERROR_DEVICE) {
		printf("%sError initializing IO6 PROM: %s\n", pref_str,
			fprom_errmsg(r));
		goto done;
	}

	if (sn0_verbose) {
		printf("%s> Manufacturer code: 0x%02x\n", pref_str, manu_code);
		printf("%s> Device code      : 0x%02x\n", pref_str, dev_code);
	}

	if (r == FPROM_ERROR_DEVICE) {
		printf("%s*** Warning: Unknown manufacture/device code\n", pref_str);
		rtn_val = EINVAL;
		goto end_flash_write;
	}

    	printf("%s> Erasing code sectors (15 to 20 seconds)\n", pref_str);

    	if ((r = fprom_flash_sectors(&f, 0x3fff)) < 0) {
        	printf("%sError erasing remote PROM: %s\n", pref_str, fprom_errmsg(r));
        	goto done;
    	}

    	printf("%s> Erasure complete and verified\n", pref_str);

	dump_promhdr(ph, pref_str);

    	/*
     	* Program the PROM
     	*/

    	printf("%s> Programming %s PROM\n", pref_str, prom_types[prom_code]);

	promlen =  ph->length;

	if (prom_code == FPROM_DEV_HUB) {
		promdata += PROM_DATA_OFFSET;
		promlen = promlen - PROM_DATA_OFFSET;
	}

	new_sum = memsum((void *)(promdata), promlen)%256;

	printf("%s> Writing %d bytes of data ...\n", pref_str, promlen);

	if (!(fprom_write_buffer(&f, promdata, promlen, pref_str)))
		goto done ;


	if (sn0_verbose)
		printf("\n");
	printf("%s> Programmed and verified\n", pref_str);
	rtn_val= 0;
	goto end_flash_write;

done:
    	printf("%s> PROM programming completed with error.\n", pref_str);
	rtn_val = EINTR;

      end_flash_write:
	if (prom_code == FPROM_DEV_HUB) {
		bzero(in_core_promdata, FPROM_SIZE);
		flash_readprom(prom_devpath, in_core_promdata,  
			       0, FPROM_SIZE, FPROM_DEV_HUB, 
			       1, pref_str);
		config_info = (ip27config_t *) (in_core_promdata + CONFIG_INFO_OFFSET);
		printf("%sVerifying:\n",pref_str);

		printf("%s          cpu speed %lld\n", 
		       pref_str, config_info->freq_cpu);

		printf("%s          hub speed %lld\n", pref_str, 
		       config_info->freq_hub);
		printf("%s          SN0/0 bit %d\n", pref_str,  
		       config_info->mach_type);
		if (bcmp(verify_config_info, config_info, 
			 sizeof(ip27config_t)) == 0) {
			printf("%s          other configuration information also verified\n", pref_str);
		}
		else {
			printf("\n%sWarning: configuration information was not\n", pref_str);
			printf("%s  properly set - reflash recommended\n", pref_str);
		}
			
		/* compare only valid sector - not prom log
		   subtract out possibly different header and config info */

		compare_bytes = promlen - CONFIG_INFO_OFFSET - sizeof(ip27config_t);

		if (bcmp(in_core_promdata + CONFIG_INFO_OFFSET + 
			 sizeof(ip27config_t),
			 promdata + CONFIG_INFO_OFFSET + 
			 sizeof(ip27config_t),
			 compare_bytes)
		    == 0) {
			printf("%sCompare of file data to in core prom data succeeded\n", pref_str);

		}
		else {
			printf("\n%sCompare of file data to in core prom data failed\n", pref_str);
			printf("%s  reflash recommended\n", pref_str);
#ifdef ZERO
			{char *core; core =(char *)17; core[0] = '\0';}
			{ int fd;

			fd = open("data-in", O_WRONLY|O_CREAT);
			write(fd, in_core_promdata + CONFIG_INFO_OFFSET, compare_bytes);
			close(fd);

			fd = open("data-file", O_WRONLY|O_CREAT);
			write(fd, promdata  + CONFIG_INFO_OFFSET, compare_bytes);
			close(fd);
			}
#endif
		}

	}
	else {
		in_core_promdata = malloc(FPROM_SIZE);
		bzero(in_core_promdata, FPROM_SIZE);
		flash_readprom(prom_devpath, in_core_promdata, 
			       IOPROM_OFFSET, FPROM_SIZE, 
			       FPROM_DEV_IO6_P1, 1, pref_str);
		if (bcmp(in_core_promdata, promdata, promlen) == 0) {
			printf("%sCompare of file data to in core data succeeded\n", pref_str);

		}
		else {
			printf("%sCompare of file data to core data failed\n", pref_str);
		}
	}
      end_no_verify_flash_write:
	if (pid == 0)
		exit(rtn_val);
	
	if ( sequential ) {
		ciep = &(child_info_ap->child_info[child_info_ap->responded++]);
		ciep->exit_status = rtn_val;
	}

	return rtn_val;
}

int
check_if_any_proms_to_flash(char *prom_devpath)
{
	char		*in_core_promdata;
	ip27config_t    *config_info;
	int             in_version;

	in_core_promdata = malloc(FPROM_SIZE);
	
	if (!in_core_promdata) {
		printf("Unable to allocate Memory size %d\n", FPROM_SIZE);
	}
	bzero(in_core_promdata, FPROM_SIZE);
	flash_readprom(prom_devpath, in_core_promdata,  
		       0, FPROM_SIZE, FPROM_DEV_HUB, 0, "");
	
	config_info = (ip27config_t *) (in_core_promdata + CONFIG_INFO_OFFSET);
	
	in_version = CONV_OFF*config_info->pvers_vers + config_info->pvers_rev;

	if (in_version < check_version) 
		return 1;
	else			
		return 0;
}


int
print_version_cpuprom(char *prom_devpath)
{
	char		*promdata;
	int		length;

	promdata = malloc(FPROM_SIZE);
	if (!promdata) {
		printf("Unable to allocate Memory size %d\n", FPROM_SIZE);
	}
	bzero(promdata, FPROM_SIZE);

	printf("Info for prom at %s\n", prom_devpath);
	length = flash_readprom(prom_devpath, promdata,  
				0, FPROM_SIZE, FPROM_DEV_HUB, 1, "");
	if (length) {
		;
	} else {
		printf("Failed to read prom data.. \n");
		return EIO;
	}
	return 0;
}

int
flash_copy_cpuprom(char *prom_devpath, char *cpufile)
{
	struct stat	buf;
	int		fd;
	char		*promdata;
	int		length;

	/*
	 * Check if the data file exists.
	 */

	if (stat(cpufile, &buf) > 0){
		if (!S_ISREG(buf.st_mode)){
			printf("File %s is not a regular file\n", cpufile);
			return EIO;
		}
		printf("File %s already exists. Overwriting the file\n",
		       cpufile);
	}


	fd = creat(cpufile, 0644);

	if (fd < 0 ) {
		printf("Unable to create file %s", cpufile);
		perror("");
		return errno;
	}


	promdata = malloc(FPROM_SIZE);
	if (!promdata) {
		printf("Unable to allocate Memory size %lld\n", buf.st_size);
	}
	bzero(promdata, FPROM_SIZE);

	length = flash_readprom(prom_devpath, promdata,  
				0, FPROM_SIZE, FPROM_DEV_HUB, 0, "");
	if (length) {
		write(fd, promdata, length);
	} else {
		printf("Failed to read prom data.. \n");
	}
	close(fd);

	return 0;
}
int
flash_write_cpuprom(char *prom_devpath, char *prom_image)
{
	return flash_writeprom(prom_devpath, prom_image,
				0, FPROM_DEV_HUB);
}



/*
 * read contents of the flash prom and print the header info
 */
static int
print_version_ioprom(char *prom_devpath)
{
	char		*promdata;
	int		length;


	promdata = malloc(FPROM_SIZE);
	if (!promdata) {
		printf("Unable to allocate Memory size %d\n", FPROM_SIZE);
	}
	bzero(promdata, FPROM_SIZE);
	
	printf("Info for prom at %s\n", prom_devpath);
	length = flash_readprom(prom_devpath, promdata, 
				IOPROM_OFFSET, FPROM_SIZE, FPROM_DEV_IO6_P1, 1, "");

	if (length) {
		;
	} else {
		printf("Unable to read Prom data..\n");
		return EIO;
	}
	return 0;
}


/*
 * read contents of the flash prom given by the path 'prompath
 * and place the data in the given buffer. 
 * This routine zeroes the buffer before copying. 
 * buflen should be greater than the size reported
 * by the prom header.
 */
static int
flash_copy_ioprom(char *prom_devpath, char *prom_image)
{
	struct stat	buf;
	char		*promdata;
	int		fd;
	int		length;

	/*
	 * Check if the data file exists.
	 */

	if (stat(prom_image, &buf) > 0){
		if (!S_ISREG(buf.st_mode)){
			printf("File %s is not a regular file\n", prom_image);
			return EIO;
		}
		printf("File %s already exists. Overwriting the file\n",
		       prom_image);
	}


	fd = creat(prom_image, 0644);

	if (fd < 0 ) {
		printf("Unable to create file %s\n", prom_image);
		perror("");
		return errno;
	}


	promdata = malloc(FPROM_SIZE);
	if (!promdata) {
		printf("Unable to allocate Memory size %lld\n", buf.st_size);
	}
	bzero(promdata, FPROM_SIZE);
	
	length = flash_readprom(prom_devpath, promdata, 
				IOPROM_OFFSET, FPROM_SIZE, FPROM_DEV_IO6_P1, 0, "");

	if (length) {
		write(fd, promdata, length);
	} else {
		printf("Unable to read Prom data..\n");
		return EIO;
	}
	close(fd);
	return 0;
}

/*
 * Flash the specified prom paths.
 * if prom_devpath is null, then flash all io proms in the system.
 */
int
flash_write_ioprom(char *prom_devpath, char *prom_image)
{
	return flash_writeprom(prom_devpath, prom_image, 
				IOPROM_OFFSET, FPROM_DEV_IO6_P1);
}

#define PRINT_ADDRESS	      0x8000
#define PRINT_DOT	      0x1000

int
fprom_write_buffer(fprom_t *f, char *buffer, int count, char pref_str[16])
{
	char	   *src, *src_end;
	off_t	    dst_off;
	int	    r, done, first, n ;
	int	    pra, prd ;

	src = (char *)buffer;
	src_end = src + count ;
	dst_off = 0 ;

	pra = 0 ;
	prd = 0 ;
	done = 0 ;
	first = 1 ;

	while (1) {
		if ((n = src_end - src) > 256)
			n = 256 ;
		if (done >= pra || n == 0) {
			if (!first)
				if (sn0_verbose)
					printf("\n") ;
			first = 0 ;
			if (sn0_verbose)
				printf("%s> %5lx/%05lx ",pref_str, done, count) ;
			if (n == 0)
				break ;
			pra += PRINT_ADDRESS ;
		}

		if ((r = fprom_write(f, dst_off, src, n)) < 0) {
			printf("\n%s> Error programming PROM: %s\n", pref_str,
					fprom_errmsg(r));
			printf("%sbase = %lx, off = %llx, src = %x\n", pref_str,
				f->base, dst_off, src) ;
			return 0 ;
		}

		if (done >= prd) {
			if (sn0_verbose)
				printf(".") ;
			prd += PRINT_DOT ;
		}

		src += n;
		dst_off += n;
		done += n;

	}
	return 1 ;
}

/*
 * Trivial function to dump the header of a prom image.
 * Used primarily for checking out a prom image.
 */
int
promimg_dump(void)
{
	int		fd;
	promhdr_t 	ph;
	/*
	 * Dump the prom headers in the given file.
	 * Primarily a debugging tool
	 */

	if (strlen(img_name) < 1) {
		printf("specify full pathname with -P\n");
		return 1;
	}

	fd = open(img_name, 0);
	if (fd < 0 ) {
		perror("open");
		return errno;
	}

	/*
	 * Opened the file. Now read the required header, and dump
	 */
	if (read(fd, (char *)&ph, sizeof(promhdr_t)) < sizeof(promhdr_t)){
		perror("read");
		return errno;
	}
	dump_promhdr(&ph, "");

	close(fd);

	return 0;

}


int parse_megahertz(char *s)
{
        int f = 1000000, n = atoi(s) * f;    

        if ((s = strchr(s, '.')) != 0)
                while (*++s)
                        n += (*s - '0') * (f /= 10);

        return n;
}

/* returns 1 if there's enough info in the header to set the entire
   configuration header - else return 0 */
int
_verify_config_info(ip27config_t *config_info)
{
	uint cache_code;
	int i;

	for (i=0; i<NUMB_IP27_CONFIGS; i++) {

		cache_code = ip27config_table[i].r10k_mode;
		/* SCS in bit 16, 17, and 18 */
		cache_code = cache_code&0x00070000;
		cache_code = cache_code >> IP27C_R10000_SCS_SHFT;
		/* we have the SCS code convert it to MB (what user entered) */
		if (cache_code == 1)
			cache_code = 1;
		if (cache_code == 3)
			cache_code = 4;

		if ((cache_size == cache_code) &&
		    (ip27config_table[i].mach_type == config_info->mach_type) &&
		    (ip27config_table[i].freq_cpu == config_info->freq_cpu) && 
		    (ip27config_table[i].freq_hub == config_info->freq_hub))
			return _fill_in_config_info(config_info, i);
	}
	printf("unknown configuration use set manually with -F\n");
	return 0;
}

int
_fill_in_config_info(ip27config_t *config_info, int index)
{
	config_info->time_const = (uint) CONFIG_TIME_CONST;
	config_info->magic = (__uint64_t) CONFIG_MAGIC;
	config_info->freq_rtc = (__uint64_t) IP27C_KHZ(IP27_RTC_FREQ);
	config_info->ecc_enable = (uint) CONFIG_ECC_ENABLE;
	config_info->check_sum_adj = (uint) 0;

	/* freq_cpu, freq_hub, and mach_type have already been set */
	config_info->r10k_mode = ip27config_table[index].r10k_mode;
	config_info->fprom_cyc = ip27config_table[index].fprom_cyc;
	config_info->fprom_wr = ip27config_table[index].fprom_wr;

	return 1;
}

void
_get_subset_config_vals(ip27config_t *config_info, int from_where)
{
	int     input_val;
	char    input_str[8];

	config_info->freq_cpu = (__uint64_t) IP27C_MHZ(195);
	printf("Enter CPU frequency (MHZ): [195] ");
	gets(input_str);
	if (strlen(input_str) > 0) {
		config_info->freq_cpu = (__uint64_t) parse_megahertz(input_str);
	}

	config_info->freq_hub = (__uint64_t) IP27C_KHZ(97500);
	printf("Enter Hub frequency (MHZ): [97.5] ");
	gets(input_str);
	if (strlen(input_str) > 0) {
		config_info->freq_hub = (__uint64_t) parse_megahertz(input_str);
	}
	
	config_info->mach_type = (uint) 0;
	printf("Enter machine type (0) SN0 (1) Sn00: [0] ");
	gets(input_str);
	if (strlen(input_str) > 0) {
		input_val = atoi(input_str);
		config_info->mach_type = (uint) input_val;
	}

	config_info->check_sum_adj = (uint) 0;

	if (from_where) {
		cache_size = 4;
		printf("Enter cache size (in MBs): [4] ");
		gets(input_str);
		if (strlen(input_str) > 0)
			cache_size = atoi(input_str);
	}

	config_info->fprom_cyc =
		(config_info->mach_type == SN00_MACH_TYPE ? 15 : 8);
	config_info->fprom_wr =
		(config_info->mach_type == SN00_MACH_TYPE ? 4 : 1);

}	

void
_get_all_config_vals(ip27config_t *config_info)
{
	int     kseg0ca, devnum, cprt, per, prm, scd, scbs, me;
	int     scs, sccd, scct, scce, odsc, odsys, ctm;
	int     input_val;
	char    input_str[8];

      re_enter:
	_get_subset_config_vals(config_info, 0);

	config_info->freq_rtc = (__uint64_t) IP27C_KHZ(IP27_RTC_FREQ);
	printf("\nEnter the RTC frequency (MHZ): [1.25] ");
	gets(input_str);
	if (strlen(input_str) > 0) {
		config_info->freq_rtc = (__uint64_t) parse_megahertz(input_str);
	}

	printf("Mode bit setting definitions may be found in the MIPS R10000 manual\n\n");

	/* set up best guess defaults for bits we're going to request */
	scd = 3;
	scs = 3;
	sccd = 2;
	scct = 9;

	/* put in values for all fixed settings */
	kseg0ca = 5;
	devnum = cprt = per = 0;
	prm = 3;
	scbs = 1;
	me = 1;
	scce = odsc = odsys = ctm =0;

	/* most of these settings are fixed - no need to ask the user
	   uncomment out in future if need arises 

	   printf("Enter the mode settings for the:\n");
	   printf(" Kseg0CA cache algorithm (KSEG0CA): ");
	   gets(input_str);
	   kseg0ca = atoi(input_str);
	   
	   printf(" processor DevNum (DEVNUM): ");
	   gets(input_str);
	   devnum = atoi(input_str);
	   
	   printf(" Coherent Processor Request Target (CPRT): ");
	   gets(input_str);
	   cprt = atoi(input_str);
	   
	   printf(" Processor Eliminate Request (PER): ");
	   gets(input_str);
	   per = atoi(input_str);

	   printf(" Processor Request Maximum (PRM): ");
	   gets(input_str);

	   prm = atoi(input_str);
	*/

	printf(" PClk to SysClk ratio (in decimal) (SCD): [%d] ", scd);
	gets(input_str);
	if (strlen(input_str) > 0)
		scd = atoi(input_str);

	/* most of these settings are fixed - no need to ask the user
	   uncomment out in future if need arises 

	   printf(" Secondary Cache Block Size (SCBS): ");
	   gets(input_str);
	   scbs = atoi(input_str);
	
	   printf(" Correction Method for the Secondary Cache: ");
	   gets(input_str);
	   scce = atoi(input_str);
	
	   printf(" Memory System Endians (ME): ");
	   gets(input_str);
	   me = atoi(input_str);
	*/

	printf(" Secondary Cache Code (e.g., 1 for 1MB, 3 for 4MB) (SCS): [%d] ", scs);
	gets(input_str);
	if (strlen(input_str) > 0)
		scs = atoi(input_str);

	printf(" PClk to SCClk ratio (SCCD): [%d] ", sccd);
	gets(input_str);
	if (strlen(input_str) > 0)
		sccd = atoi(input_str);

	printf(" alignment of SysClk to internal Sec Cache Clock (SCCT): [0x%x] ", scct);
	gets(input_str);
	if (strlen(input_str) > 0)
		scct = (int) strtoul(input_str, NULL, 16);

	/* most of these settings are fixed - no need to ask the user
	   uncomment out in future if need arises 

	   printf(" : ");
	   gets(input_str);
	   odsc = atoi(input_str);
	

	   printf(" Configuration of Open Drain (ODSYS): ");
	   gets(input_str);
	   odsys = atoi(input_str);
	   
	   printf(" Enabling of the Cache Test (CTM): ");
	   gets(input_str);
	   ctm = atoi(input_str);
	   */

	printf("\nThere is no checking that occurs when entering the full set of\n");
	printf("  configuration bits.  Please verify the following is correct:\n");
	printf("CPU frequency (MHZ) %lld\nHub frequency (MHZ) %lld\nMachine type        %d\n",
	       config_info->freq_cpu/1000000, 
	       config_info->freq_hub/1000000,
	       config_info->mach_type);
	printf("RTC frequency (KHZ)  %lld\n",config_info->freq_rtc/1000);
	/*
	   printf(" Kseg0CA            \t\t(KSEG0CA) %d\n", kseg0ca);
	   printf(" DevNum              \t\t(DEVNUM) %d\n", devnum);
	   printf(" Coh Proc Req Targ       \t(CPRT) %d\n", cprt);
	   printf(" Proc Elim Req            \t(PER) %d\n", per);
	   printf(" Proc Req Max             \t(PRM) %d\n", prm);
	   printf(" PClk/SysClk              \t(SCD) %d\n", scd);
	   printf(" Sec Cache Blk Sz        \t(SCBS) %d\n", scbs);
	   printf(" Mem Sys End               \t(ME) %d\n", me);
	   printf(" Sec Cache Size           \t(SCS) %d\n", scs);
	   printf(" PClk/SCClk              \t(SCCD) %d\n", sccd);
	   printf(" algn SClk Sec Cache Clk \t(SCCT) 0x%x\n", scct);
	   printf(" Open Drain Conf      \t\t(ODSYS) %d\n", odsys);
	   printf(" Cache Test Enable        \t(CTM) %d\n", ctm);
	*/

	printf(" PClk/SysClk              \t(SCD) %d\n", scd);
	printf(" Sec Cache Size           \t(SCS) %d\n", scs);
	printf(" PClk/SCClk              \t(SCCD) %d\n", sccd);
	printf(" algn SClk Sec Cache Clk \t(SCCT) 0x%x\n", scct);

	printf("\nIf these value are correct enter y else n: ");
	gets(input_str);
	if (input_str[0] != 'y')
		goto re_enter;

	config_info->r10k_mode = (uint) (IP27C_R10000_KSEG0CA(kseg0ca) + \
				 IP27C_R10000_DEVNUM(devnum) + \
				 IP27C_R10000_CPRT(cprt) + \
				 IP27C_R10000_PER(per)	 + \
				 IP27C_R10000_PRM(prm)	 + \
				 IP27C_R10000_SCD(scd)	 + \
				 IP27C_R10000_SCBS(scbs) + \
				 IP27C_R10000_SCCE(scce) + \
				 IP27C_R10000_ME(me)	 + \
				 IP27C_R10000_SCS(scs)	 + \
				 IP27C_R10000_SCCD(sccd) + \
				 IP27C_R10000_SCCT(scct) + \
				 IP27C_R10000_ODSC(odsc) + \
				 IP27C_R10000_ODSYS(odsys) + \
				 IP27C_R10000_CTM(ctm));
	
	config_info->time_const = (uint) CONFIG_TIME_CONST;
	config_info->magic = (__uint64_t) CONFIG_MAGIC;
	config_info->ecc_enable = (uint) CONFIG_ECC_ENABLE;
	config_info->check_sum_adj = (uint) 0;

	config_info->config_type = (uint) 0;
	printf("Enter configuration type (0) Normal (1) 12P4I: [0] ");
	gets(input_str);
	if (strlen(input_str) > 0) {
		input_val = atoi(input_str);
		config_info->config_type = (uint) input_val;
	}
}

#define LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

__uint64_t
memsum(void *base, size_t len)
{
    uchar_t    *src	= base;
    __uint64_t	sum, part, mask, v;
    int		i;

    mask	= 0x00ff00ff00ff00ff;
    sum		= 0;

    while (len > 0 && (__psunsigned_t) src & 7) {
	sum += LBYTEU(src);
	src++;
	len--;
    }

    while (len >= 128 * 8) {
	part = 0;

	for (i = 0; i < 128; i++) {
	    v = *(__uint64_t *) src;
	    src += 8;
	    part += v	   & mask;
	    part += v >> 8 & mask;
	}

	sum += part	  & 0xffff;
	sum += part >> 16 & 0xffff;
	sum += part >> 32 & 0xffff;
	sum += part >> 48 & 0xffff;

	len -= 128 * 8;
    }

    while (len > 0) {
	sum += LBYTEU(src);
	src++;
	len--;
    }

    return sum;
}

/*
 * erase_promlog_sectors
 *
 * Fill up sectors 14 and 15 with 0xff.
 */

static void
erase_promlog_sectors(char *prom_devpath)
{
        fprom_t         f;
        int             manu_code, dev_code;
	int		r, i ;
	int		prom_mapoffset ;

	printf("Erasing prom log of %s 	........ ", prom_devpath) ;

	/* Need to erase sectors 14 and 15 */

	prom_mapoffset = 14 * FPROM_SECTOR_SIZE ;
        f.base = map_prom(prom_devpath, prom_mapoffset, 8*FPROM_SIZE);
        if (!f.base) {
		printf("FAIL\n") ;
		return ;
        }

	/*
	 * afn and aparm are abort function and its args.
	 * They are null for now.
	 */
	f.afn = 0; 
	for (i=0;i<8;i++) {
		f.aparm[i] = 0;
	}
	f.dev = FPROM_DEV_HUB;

	r = fprom_probe(&f, &manu_code, &dev_code);

	if (r < 0 && r != FPROM_ERROR_DEVICE) {
		printf("FAIL Init\n") ;
		return ;
	}

	if (r == FPROM_ERROR_DEVICE) {
		printf("FAIL Bad Mfg code\n") ;
		return ;
	}

    	if ((r = fprom_flash_sectors(&f, 0xC000)) < 0) {
        	printf("FAIL Erase\n") ;
		return ;
    	}

	printf("DONE\n") ;
}

int
ip31_get_config(int pimm_psc, ip27config_t *c)
{
        int     index;

        if ((index = ip31_psc_to_flash_config[pimm_psc]) == IP27_CONFIG_UNKNOWN)
                return -1;

        c->time_const = (uint) CONFIG_TIME_CONST;
        c->r10k_mode = ip27config_table[index].r10k_mode;
        c->magic = (__uint64_t) CONFIG_MAGIC;
        c->freq_cpu = ip27config_table[index].freq_cpu;
        c->freq_hub = ip27config_table[index].freq_hub;
        c->freq_rtc = (__uint64_t) IP27C_KHZ(IP27_RTC_FREQ);
        c->ecc_enable = (uint) CONFIG_ECC_ENABLE;
        c->fprom_cyc = ip27config_table[index].fprom_cyc;
        c->mach_type = ip27config_table[index].mach_type;
        c->check_sum_adj = (uint) 0;
        c->fprom_wr = ip27config_table[index].fprom_wr;

        return 0;
}

