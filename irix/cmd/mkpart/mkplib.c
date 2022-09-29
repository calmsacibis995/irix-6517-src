#ident "irix/cmd/mkplib.c: $Revision: 1.29 $"

/*
 * mkplib.c
 *
 * partition config utility library.
 *
 */

#define SN0 1 

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/conf.h>
#include <sys/syssgi.h>
#include <fcntl.h>

#include <sys/SN/router.h>
#include <sys/SN/klpart.h>
#include <sys/SN/SN0/sn0drv.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <syslog.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/stat.h>

#include "mkpart.h"
#include "mkpbe.h"
#include "mkprou.h"
#include "mkpd.h"

int 	debug ;
int	force_option ;
extern	int errno ;
static	part_ipaddr_t	part_ipaddr[MAX_PARTITION_PER_SYSTEM] ;

valid_config_t  valid_config[MAX_TOTAL_MODULES] ;
valid_config_t  *vc_hdr_p ;

extern void dump_sys_rmap(sys_rou_map_t *) ;
extern int check_numbers(partcfg_t *, partcfg_t *, char *) ;
extern int check_contiguous(sys_rou_map_t *, partcfg_t *, char *, int) ;
extern int check_interconn(sys_rou_map_t *, partcfg_t *, char *, int) ;
extern int is_metarouter(rou_map_t *) ;

static int lock_all_part(becmd_t *, char *) ;
static int unlock_all_part(becmd_t *, char *, int) ;
static int is_my_act_part(partcfg_t *, partcfg_t *, partid_t) ;
static int is_xpc_up(partid_t) ;
static int meta_routers_present(void) ;
static int count_partitions(partcfg_t *pc) ;
static int check_hub_version(void) ;
static void check_for_partid0(partcfg_t *) ;

/* separator string for parsing */

char *sep = " ,:=\n" ;

/* keywords in cfgfile */

char *file_keyw[] = {
#define FKEYW_PARTITION 	0
	"partition", 
#define FKEYW_MODULE 		1
	"module",
NULL
} ;

char *shutdown_self_msg = 
"Shutting down current partition to re-configure ...\n" ;

char *exit_now_msg = 
"Exiting the command now.\n" ;

/*
 * lib err msgs
 */
char *msglib[] = {
#define MSG_MEM_ALLOC 0
	"Cannot allocate any more Memory\n",
#define MSG_MOD_EXCEED_LIMIT 		1
	"%d Modules on Partition %d exceeds limit."
	" Check cfgfile or Command line.\n",
#define MSG_LIB_SYNTAX_ERROR 		2
	"Syntax error.\n",
#define MSG_PARTIAL_SYNTAX_ERROR 	3
	"Warning: Ignoring syntax error in part of command line.\n",
#define MSG_NO_PARTITIONS 		4
	"Machine not partitioned.\n",
#define MSG_INVALID_CONFIG 		5
	"Invalid Configuration specified.\n",
#define MSG_REMOTE_EADDR 		6
	"Unable to get remote addr. Check remote daemon.\n",
#define MSG_REXEC_FAILED 		7
	"Cannot run remote command on partition %d. Config unchanged.\n",
#define MSG_LOCK_ERR 			8
	"Cannot lock partition %d. Try later.\n",
#define MSG_UNLOCK_ERR 			9
	"Cannot unlock partition %d. Use -F option next time.\n",
#define MSG_ROUMAP_ERR 			10
	"Invalid router map on partition %d.\n"\
        "Please do a manual check of craylink connectivity and use -F \n"\
        "option to override sanity check.\n",
#define MSG_ALL_MODULE 			11
	"Module %d is in partition %d, which is also a new partition.\n",
#define MSG_ZERO_ROUTER 		12
	"Failed to obtain proper router map info from Partition %d.\n"\
	"Please do a manual check of craylink connectivity and use -F \n"\
	"option to override sanity check.\n",
NULL
} ;

/*
 * partcfg data structure support routines.
 */

static partcfg_t *
partcfgInit(partcfg_t *pc, int nmods)
{
	int i ;

	if (pc) {
		partcfgNext(pc) 		= NULL ;
		partcfgId(pc)			= INVALID_PARTID ;
    		for (i = 0; i < nmods; i++)
	    		partcfgModuleId(pc, i) 	= 0 ;
		partcfgNumMods(pc) 		= 0 ;
		partcfgModsSize(pc)		= nmods ;
    	}
    	return pc ;
}

partcfg_t *
partcfgCreate(int nmods)
{
	partcfg_t *pc ;

	if (pc = (partcfg_t *)malloc(sizeof(partcfg_t))) {
		if (!(partcfgModules(pc) = 
			(moduleid_t *)malloc(nmods * sizeof(moduleid_t)))) {
	    		free(pc) ;
	    		return NULL ;
		}
		partcfgInit(pc, nmods) ;
    	}

    	return pc ;
}

/*
 * Free the linked list recursively.
 */

void
partcfgFree(partcfg_t *pc)
{
	if (partcfgModules(pc))
        	free(partcfgModules(pc)) ;
	free(pc) ;
}

void
partcfgFreeList(partcfg_t *pc)
{
	if (pc) {
		if (partcfgNext(pc))
			partcfgFreeList(partcfgNext(pc)) ;
		partcfgFree(pc) ;
	}
}

static void
partcfgInsertList(partcfg_t **pch, partcfg_t *pc)
{
	if (pc)
		partcfgNext(pc) = *pch ;
	*pch = pc ;
}

void
partcfgDump(partcfg_t *pc)
{
	int i ;

	if (!pc)
		return ;

	printf("        partition: %d = module: ", partcfgId(pc)) ;
	for (i = 0; i < partcfgNumMods(pc); i++)
        	printf("%d ", partcfgModuleId(pc, i)) ;
	printf("\n") ;
}

void
partcfgDumpList(partcfg_t *pch)
{
	partcfg_t *pc = pch ;

	while (pc) {
		partcfgDump(pc) ;
		pc = partcfgNext(pc) ;
	}
}

/*
 * Parsing support.
 */

/* Is the given string, full of digits only? */

static int
isnumeric(char *s)
{
	char *st = s ;

	if (st == NULL)
		return 0 ;

	while (*st)
		if (!isdigit(*st++))
			return 0 ;
	return 1 ;
}

/* 
 * get_part_from_string
 *
 *	get partition number from arg string.
 * 	It assumes that strtok pointer is initted.
 *	It is called in a loop in parse_string.
 *	Checks if the string is made up of digits only.
 * 	Returns the integer value of the string.
 */
 
static int
get_part_from_string(void)
{
	char *tmp;

	tmp = strtok(NULL, sep) ;
	if (isnumeric(tmp))
		return(atoi(tmp)) ;
	return -1 ;
}

/* Calculate offset of next string while parsing using strtok.
 * This macro is local to parse_string.
 */
#define	NXT_STR_PTR  \
	(str + ((__psunsigned_t)tok_ptr - (__psunsigned_t)tok_buf))

/*
 * parse_string
 *
 * 	Parse a string of the form
 *  	partition : <pno> = module : <modnum> <modnum> [,] <modnum>
 *  	Assign partition and module values to partcfg struct.
 *
 * Input:
 *
 * 	One line of a config file, as given above or a concatenation
 *	of many such lines, built from the command line.
 *
 * Return : 
 *
 * 	Pointer to the next character to be parsed. If the input
 * 	string were from a file, this would be the \0 at the end
 *	of the line. If this is a concatenated string, the return
 *	value would be the pointer to next logical part of the 
 *	string. A logical part is composed of partition and module
 *	keywords and a string of module numbers. 
 *	This routine can be called in a loop to parse the concatenated
 *	string.
 *	status gets the SUCCESS of FAILURE of parsing.
 */

static char *
parse_string(char *str, partcfg_t *pc, int *status, char *errbuf)
{
	int 		mod_indx = 0, tmp = 0,
			keyw_indx = 0 ;
	char 		tok_buf[MAX_CMDLINE_LEN], *tok_ptr = tok_buf ;
	int		mod_keyw_found = 0, mod_num_found = 0 ;
	int		par_keyw_found = 0, par_num_found = 0 ;
	moduleid_t  	*m = partcfgModules(pc) ;
 
	if ((!str) || (!pc) || (!status))
		return NULL ;
	*status = -1 ;
	if (errbuf)
		*errbuf = 0 ;

	/* Make local copy and setup strtok buffer */

	if (strlen(str) >= sizeof(tok_buf))
		return NULL ;
	strcpy(tok_ptr, str) ;
	tok_ptr = strtok(tok_ptr, sep) ;

	/* parse every token in a loop */

	while (tok_ptr) {
		db_printf("%s ", tok_ptr) ;

		/* check if we got a keyword */

		keyw_indx = get_string_index(file_keyw, tok_ptr) ;

		switch (keyw_indx) {
	    		case FKEYW_PARTITION:

				/* If we already got a partition keyw
				 * we are ready to parse the next string
				 */
				if (par_keyw_found)
                    			goto keyw_repeat ;
				par_keyw_found = 1 ;

				/* uses strtok to get next token */
				if ((partcfgId(pc) = 
						get_part_from_string()) >= 0)
		     			par_num_found = 1 ;
 	    			break ;

	    		case FKEYW_MODULE:

                                /* If we already got a partition keyw
                                 * we are ready to parse the next string
                                 */
	 			if (mod_keyw_found)
                    				goto keyw_repeat ;
				mod_keyw_found = 1 ;
	    			break ;

  	    		default:

				/* These should be module numbers.
				 * check if it is gone beyond limit 
				 * and if the value looks good.
 				 */
				if (partcfgModLimit(pc, mod_indx)) {
					if (!isnumeric(tok_ptr)) {
						strcpy(errbuf,
						msglib[MSG_LIB_SYNTAX_ERROR]) ;
						return NULL ;
					}
		    			m[mod_indx] = tmp = atoi(tok_ptr) ;
				} else {
		    			sprintf(errbuf, 
						msglib[MSG_MOD_EXCEED_LIMIT], 
			    			mod_indx, partcfgId(pc)) ;
		    			return NULL ;
				}

				if (!tmp)	/* must be another string */
	    	    			goto keyw_repeat ;
				else
		    			mod_num_found = 1 ;

				partcfgNumMods(pc) = ++mod_indx ;
	    			break ;
		}
		tok_ptr = strtok(NULL, sep) ;	/* Get next token */
    	}

keyw_repeat:
	/* Was the last sequence logically complete? */
    	if (par_keyw_found && par_num_found &&
        		mod_keyw_found && mod_num_found)
		*status = 1 ;
    	else
		strcpy(errbuf, msglib[MSG_LIB_SYNTAX_ERROR]) ;

    	db_printf("\n") ;

	/* Still got a valid token? Return ptr to next logical string */
    	if (tok_ptr)
        	return(NXT_STR_PTR) ;
    	else 	/* Must be a single logical line, return ptr to the \0 */
        	return(str+strlen(str)) ;
}

/*
 * parse_file 
 *
 *	parse the lines in the cfgfile
 *
 * Return:
 *
 * 	ptr to a linked list of partcfg_t which contains info
 *		about all lines.
 * 	NULL, if file has no valid lines in it.
 */

partcfg_t   *
parse_file(FILE *fp, char *errbuf)
{
	char 		buf[MAX_CMDLINE_LEN] ;
	char 		*bufp ;
	partcfg_t   	*pc = NULL, *pch= NULL ;
	int		status ;

	if (errbuf)
		*errbuf = 0 ;

	while (bufp = fgets(buf, MAX_CMDLINE_LEN, fp)) {

		/* Skip comments */

		if ((*bufp == '#') || (*bufp == '*'))
			continue ;

		pc = partcfgCreate(MAX_MODS_PER_PART) ;
		if (pc == NULL) {
			if (errbuf)
    	    			strcpy(errbuf, msglib[MSG_MEM_ALLOC]) ;
	    		goto fail;
		}
		parse_string(bufp, pc, &status, errbuf) ;
        	if (status < 0) {
	    		partcfgFree(pc) ;
	    		pc = NULL ;
	    		goto fail ;
		}
        	partcfgInsertList(&pch, pc) ;
	}
fail:
	return pch;
}

/*
 * parse_multi_string 
 *
 * 	parse a string which is concactenated strings in a file.
 */

partcfg_t   *
parse_multi_string(char *str, char *errbuf)
{
	char 		*cp = str ;
	partcfg_t   	*pc = NULL, *pch= NULL ;
	int         	status ;

	if (errbuf)
		*errbuf = 0 ;

	/* As long as cp does not point to a end of string \0 ... */
	while ((cp) && *cp) {
		pc = partcfgCreate(MAX_MODS_PER_PART) ;
		if (pc == NULL) {
			if (errbuf)
            			strcpy(errbuf, msglib[MSG_MEM_ALLOC]) ;
            		goto fail;
		}
		/* cp should point to the next logical part of the string. */
		cp = parse_string(cp, pc, &status, errbuf) ;
		if (status < 0) {
            		partcfgFree(pc) ;
            		pc = NULL ;

			/* We have found atleast 1 logical element */

	    		if (pch)
				fprintf(stderr, msglib[MSG_PARTIAL_SYNTAX_ERROR]) ;

			goto fail ;
        	}
        	partcfgInsertList(&pch, pc) ;
	}
fail:
	return pch;
}

/*
 * cmd_to_file_string
 *
 *     convert command line to a string
 *     that looks like a concatenation of
 *     strings from the cfgfile.
 */

char *
cmd_to_file_string(int argc, char **argv, int argi)
{
	char 	*fsp ;
	char	local_buf[MAX_KEYW_LEN] ;

	fsp = (char *)malloc(MAX_CMDLINE_LEN) ;
	if (fsp == NULL) {
		goto fail ;
	}
	*fsp = 0 ;
	while (argi < argc) {
		*local_buf = 0 ;
		if (!strcmp(argv[argi], "-p")) {
	    		strcat(local_buf, file_keyw[FKEYW_PARTITION]) ;
	    		strcat(local_buf, " : ") ;
		} else if (!strcmp(argv[argi], "-m")) {
            		strcat(local_buf, file_keyw[FKEYW_MODULE]) ;
	    		strcat(local_buf, " : ") ;
		} else {
	    		strcat(local_buf, argv[argi]) ;
	    		strcat(local_buf, " , ") ;
		}
		if ((strlen(fsp) + strlen(local_buf)) < MAX_CMDLINE_LEN) 
	    		strcat(fsp, local_buf) ;
		else
	    		return fsp ;
		argi++ ;
	}
	db_printf("make_cline_string: %s\n", fsp) ;
fail:
	return fsp ;
}

/*
 * Search for a string str in the given table tab
 * and return the index. tab should be an array of
 * strings.
 * Returns -1 if failure, index of string str in
 * array tab if success.
 */

int
get_string_index(char **tab, char *str)
{
	int i = 0 ;
	char *cp ;

	cp = tab[i] ;
	while(cp) {
        	if (!strncmp(cp, str, strlen(cp)))
            		return i ;
        	cp = tab[++i] ;
	}
	return -1 ;
}

/*
 * Support for mkpart -l option
 */

/* Search for a module in the given partcfg_t element. */

static int
partcfgLookupModule(partcfg_t *pc, moduleid_t m)
{
	int i ;
	for (i = 0; i < partcfgNumMods(pc) ; i++)
        	if (partcfgModuleId(pc, i) == m)
            		return 1 ;
	return -1 ;
}

/*
 * Search for partition p in the partcfg_t linked list.
 * If moduleid < 0 search for partition only.
 * If partid < 0 search for moduleid only.
 */

partcfg_t *
partcfgLookupPart(partcfg_t *pch, partid_t p, moduleid_t m)
{
	partcfg_t *pc = pch;

	while (pc) {
        	if ((m < 0) && (partcfgId(pc) == p))
            		return pc ;
		if ((p < 0) && (partcfgLookupModule(pc, m) >= 0))
	    		return pc ;
        	pc = partcfgNext(pc) ;
	}
	return NULL ;
}

/*
 * addPnToPcList
 * Add the new module m to partition p.
 * If partition is new, create it first.
 */

partcfg_t *
addPnToPcList(partcfg_t **pch, partid_t p, moduleid_t m)
{
	partcfg_t   *pc ;

	if ((pc = partcfgLookupPart(*pch, p, (moduleid_t) -1)) == NULL) {
        	pc = partcfgCreate(MAX_MODS_PER_PART) ;
        	partcfgId(pc) = p ;
		partcfgInsertList(pch, pc) ;
	}

	if (partcfgLookupModule(pc, m) < 0) {
        	partcfgModuleId(pc, partcfgNumModsIncr(pc)) = m ;
	}

	return(*pch) ;
}

partcfg_t *
pnToPcList(pn_t *pnh, int cnt)
{
	int 		i ;
	pn_t		*pn = pnh ;
	partcfg_t 	*pch= NULL ;
	int 		lcnt = (cnt > MAX_MODS_PER_PART) ? 
				MAX_MODS_PER_PART : cnt ;

	for (i = 0; i < lcnt ; i++, pn++) {
		if (	!IS_PARTID_VALID(pn->pn_partid) || 
			(pn->pn_state != KLP_STATE_KERNEL) ||
			/* There is no point in advertising a partition
			 * if its XPC is not up. The only way to do it
			 * now is to check if its admin file is created.
			 */
			(!is_xpc_up(pn->pn_partid)))
			continue ;
		addPnToPcList(&pch, pn->pn_partid, pn->pn_module) ;
	}
	return pch;
}

/*
 * part_scan
 *
 * 	Build a list of all partitions discovered by the kernel
 * 	and the modules that it consists of.
 */

partcfg_t *
part_scan(int dump_flag, char *errbuf)
{
	partcfg_t   	*pc = NULL ;
	pn_t		*pn ;
	int 		cnt = -1 ;

	if (errbuf)
		*errbuf = 0 ;

	if (!(pn = (pn_t *)malloc(MAX_PN * sizeof(pn_t))))
		return NULL ;

	cnt = syssgi(	SGI_PART_OPERATIONS, 
			SYSSGI_PARTOP_NODEMAP, MAX_PN, pn) ;

	if (cnt < 0)
		return NULL ;

	pc = pnToPcList(pn, cnt) ;

	if (debug || dump_flag) {
		printf("Current partition config ... \n") ;
		partcfgDumpList(pc) ;
		printf("\n") ;
	}

	return(pc) ;
}

int
part_list(char *errbuf)
{
	partcfg_t	*pc ;

	pc = part_scan(0, errbuf) ;
	if (!pc) {
		strcpy(errbuf, msglib[MSG_NO_PARTITIONS]) ;
		return -1 ;
	}

	partcfgDumpList(pc) ;

	partcfgFreeList(pc) ;

	return 1 ;
}

int
part_reinit(char *errbuf)
{
	partcfg_t	*pc_act, *pc ;
	int		i ;
	int		mod_indx ;
	moduleid_t	*m ;

	if (!yes_or_no("Reset all partition ids to 0? [y/n]: "))
		return 1 ;

        pc_act = part_scan(0, errbuf) ;
        if (!pc_act) {
                strcpy(errbuf, msglib[MSG_NO_PARTITIONS]) ;
                return -1 ;
        }

        pc = partcfgCreate(MAX_MODS_PER_PART) ;
        if (pc == NULL) {
                if (errbuf)
                        strcpy(errbuf, msglib[MSG_MEM_ALLOC]) ;
                return -1 ;
        }

	mod_indx = 0 ;
	m = partcfgModules(pc) ;
	partcfgId(pc) = 0 ;
        while (pc_act) {
                for (i=0; i < partcfgNumMods(pc_act); i++) {
                        m[mod_indx] = partcfgModuleId(pc_act, i) ;
                        partcfgNumMods(pc) = ++mod_indx ;
		}
                pc_act = partcfgNext(pc_act) ;
        }

	if (debug)
		partcfgDump(pc) ;

	part_make(pc, errbuf) ;

        return 1 ;

}

/*
 * mkpbe data structure becmd_t support.
 */

void
becmdDump(becmd_t *bec)
{
	printf("        part %d:", bec->bec_partid) ;
	printf(" %s\n", bec->bec_becl) ;
}

void
becmdDumpList(becmd_t *bech)
{
	becmd_t *bec = bech ;

	while (bec) {
		becmdDump(bec) ;
		bec = bec->bec_next ;
	}
}

static becmd_t *
becmdInit(becmd_t *bec, int len, char *cmd)
{
	if (bec) {
        	bec->bec_next 		= NULL ;
        	bec->bec_partid 	= INVALID_PARTID ;
        	bec->bec_becl[0] 	= 0 ;
		if (cmd) ;
        		strcpy(bec->bec_becl, cmd) ;
        	bec->bec_becl_len 	= len ;

        	bec->bec_full_cl[0] 	= 0 ;
        	strcpy(bec->bec_full_cl, RSHCMD_NAME) ;
        	bec->bec_becl_len 	= len + 256 ;
	}

	return bec ;
}

becmd_t *
becmdCreate(int cl_len, char *cmd)
{
	becmd_t	*bec ;

	if (cl_len < 0)
		return NULL ;

	if (bec = (becmd_t *)malloc(sizeof(becmd_t))) {
        	if (((bec->bec_becl = (char *)malloc(cl_len)) == NULL) ||
            	((bec->bec_full_cl  = (char *)malloc(cl_len+256)) == NULL)) {
	    		if (bec->bec_becl)
				free(bec->bec_becl) ;
	    		free(bec) ;
	    		return NULL ;
		}
    		becmdInit(bec, cl_len, cmd) ;
	}
	return bec ;
}

/* Free list recursively */

void
becmdFree(becmd_t *bec)
{
	if (bec->bec_becl)
        	free(bec->bec_becl) ;
	if (bec->bec_full_cl)
        	free(bec->bec_full_cl) ;
	free(bec) ;
}

void
becmdFreeList(becmd_t *bec)
{
	if (bec) {
        	if (bec->bec_next)
            		becmdFreeList(bec->bec_next) ;
        	becmdFree(bec) ;
	}
}

static void
becmdInsertList(becmd_t **bech, becmd_t *bec)
{
	if (bec)
        	bec->bec_next = *bech ;
    	*bech = bec ;
}

becmd_t *
becmdLookupList(becmd_t *bech, partid_t p)
{
	becmd_t *bec = bech ;

	while (bec) {
		if (bec->bec_partid == p)
	    		return bec ;
   		bec = bec->bec_next ;
	}

	return NULL ;
}

becmd_t *
becmdAllocate(becmd_t **bech, partid_t p, char *cmd)
{
	becmd_t *bec ;

	if (!(bec = becmdLookupList(*bech, p))) {
		bec = becmdCreate(MAX_BECMD_LEN, cmd) ;
        	if (bec == NULL)
	    		return NULL ;
    		becmdInsertList(bech, bec) ;
	}

	return bec ;
}

void
becmdAddModPart(becmd_t *bec, moduleid_t m, partid_t p)
{
	char tmpbuf[MAX_MODPAR_LEN] ;

	sprintf(tmpbuf, "%d %d ", m, p) ;
	if ((strlen(tmpbuf) + strlen(bec->bec_becl)) > bec->bec_becl_len)
		return ;
	strcat(bec->bec_becl, tmpbuf) ;
}

/*
 * Sanity checks support.
 */

int 
checkModUnique(partcfg_t *pch, int m)
{
	partcfg_t 	*pc = pch ;
	int 	found = 0 ;
	int 	i ;

	while (pc) {
        	for (i=0; i < partcfgNumMods(pc) ; i++) {
	    		if (m == partcfgModuleId(pc, i)) {
	        		if (!found)
	            			found = 1 ;
	        		else
		    			return -1 ;
	    		}
		}
		pc = partcfgNext(pc) ;
	}

	return 1 ;
}

/*
 * Check if all module numbers are unique.
 */
int
isModNumUnique(partcfg_t *pch)
{
	partcfg_t	*const_pch	= pch ;
	partcfg_t	*pc      	= pch ;
	int 	i ;

	while (pc) {
		for (i=0; i < partcfgNumMods(pc) ; i++)
	    		if (checkModUnique(const_pch, 
					partcfgModuleId(pc, i)) < 0)
				return -1 ;
    			pc = partcfgNext(pc) ;
	}
	return 1 ;
}

/* ARGSUSED */

int
partcfgValid(partcfg_t *pc, char *errbuf)
{
	/* Is the partid valid? Does it have atleast 1 module */

	if (!IS_PARTID_VALID(partcfgId(pc))) {
                sprintf(errbuf, "Partition id %d is invalid.\n",
                        partcfgId(pc)) ;
		return -1 ;
	}
	if (partcfgNumMods(pc) == 0) {
		sprintf(errbuf, "Partition %d has no modules in it.\n",
				partcfgId(pc)) ;
		return -1 ;
	}
    	return 1 ;
}

/*
 * Check if each of the required partition and module config
 * is OK. The per partition tests are yet to be determined.
 */
int
partcfgValidList(partcfg_t *pc, char *errbuf)
{
	if (isModNumUnique(pc) < 0)
		return -1 ;

	while (pc) {
        	if (partcfgValid(pc, errbuf) < 0)
	    		return -1 ;
		pc = partcfgNext(pc) ;
	}
	return 1 ;
}

/*
 * mkpd_read_req
 *
 *	Read a request packet from fd, check for sanity.
 *
 * Return 
 *
 * 	count of bytes needed or opcode if success.
 *	A -ve code indicating the place of failure.
 */
int
mkpd_read_req(int fd, mkpd_packet_t *packet, char type)
{
        mkpd_cnt_t     cnt ;
	char 	*buf = (char *)packet ;

	if (packet == NULL)
		return -1 ;

        cnt = read(fd, buf, PACKET_REQ_LEN) ;
        if ((cnt != PACKET_REQ_LEN) || (packet->version != VERSION)) {
		if (debug)
			perror("mkpd_read_req") ;
                return -2 ;
	}

        if (packet->ptype != type)
                return -3 ;

	if (packet->ptype == PACKET_TYPE_OPCODE)
		return(packet->op) ;
		
        return (packet->cnt) ;
}

/*
 * get_raw_path
 *
 * 	Get the path of the raw device file to be opened for
 * 	partid p. /hw/xplink/admin/01/partition
 * 	Check if length of the partid field like 01 exceeds
 * 	2 digits. This is needed for padding the partid's ascii
 * 	version to the number of digits used in the hwgraph path
 *	name of the file. The kernel just creates a 2 digit value
 * 	for each number. This scheme breaks if we are going to
 *	have mixed field widths.
 */
int
get_raw_path(char *path, partid_t p)
{
	char 	tmp[MKPD_MAX_XRAW_FNAME_LEN] ;
	int	len ;
	int	i ;

	*path = 0 ;
	*tmp  = 0 ;

	sprintf(path, "%x\000", p) ;
	len = (MKPD_PART_FNAME_LEN-strlen(path)) ;
	if (len < 0)
		return -1 ;

	for (i=0;i<len;i++)
		tmp[i] = '0';
	tmp[i] = 0 ;

	strcat(tmp, path) ;

	strcpy(path, MKPD_XPLINK_RAW_PATH) ;
	strcat(path, tmp) ;
	strcat(path, MKPD_XPLINK_RAW_PATH_1) ;

	return 1 ;
}

/*
 * get_connect_socket
 *
 * 	create a socket and try to connect to it. This is for the
 * 	client mkpart.
 */
int
get_connect_socket(void)
{
	int			sock = -1 ;
        struct sockaddr_un      sname ;

        sock = socket(PF_UNIX, SOCK_STREAM, 0) ;
        if (sock < 0) {
		if (debug) perror("socket") ;
		goto get_conn_done ;
	}

        sname.sun_family = AF_UNIX ;
        strcpy(sname.sun_path, MKPD_SOCKET_NAME) ;

        if (connect(sock, (caddr_t)&sname, sizeof(sname)) < 0) {
		if (debug) perror("connect") ;
        	close(sock) ;
		sock = -1 ;
        }
get_conn_done:
	return sock ;
}

/* 
 * get_data_from_local
 *
 *	The mkpart client uses this routine to contact the
 * 	local mkpd daemon and get its work done.
 *
 * Parameters
 *
 *		op	opcode
 * 		p	Partition id of the target partition.
 *		buf	buffer to receive data
 * 		buflen	length of this buffer
 */
int
get_data_from_local(char op, partid_t p, char *buf, int buflen)
{
        int     	sock ;
	mkpd_reqbuf_t	req ;
	mkpd_packet_t	packet ;
	mkpd_cnt_t	cnt ;
	char 		*tmpbuf ;
	int		rv = 1 ;

	/* Let's go to our daemon. */
	/* XXX Try to get in errbuf and print the actual reason of failure. */

	if ((sock = get_connect_socket()) < 0)
		return -1 ;

	/* 
	 * Prepare a request buf to the local daemon. This is different
	 * from the request packet that goes between daemons. 
 	 */
 	
	req.version	= VERSION ;
	req.op 		= op ;
	req.part 	= p ;
	req.cnt		= buflen ;
	req.flag	= force_option ;

	/* Write it to the socket */

        if (write(sock, &req, sizeof(mkpd_reqbuf_t)) != sizeof(mkpd_reqbuf_t)) {
		rv = -2 ;
		goto close_n_ret ;
	}

	/* 
	 * Read back from the socket, a packet of type COUNT. This
	 * gives the length of the data the daemon is having for us.
 	 */
	if ((cnt = mkpd_read_req(sock, &packet, PACKET_TYPE_COUNT)) < 0) {
		db_printf("get_data_from_local: mkpd_read_req fail %d\n", cnt) ;
		rv = -3 ;
		goto close_n_ret ;
        }

	/*
 	 * We need to pull out all the data from the daemon.
	 * Try to alloc a bigger buf if needed.
 	 */
	if (cnt > buflen) {
		tmpbuf = (char *)malloc(cnt) ;
		if (!tmpbuf) { 	/* too bad, read what we can */
			tmpbuf = buf ;
			cnt = buflen ;
		}
	}
	else	
		tmpbuf = buf ;

        if (read(sock, tmpbuf, cnt) != cnt) {
		db_printf("get_data_from_local: read data fail %d\n", errno) ;
		rv = -4 ;
		goto close_n_ret;
        }

	/* If we had allocated new buffer, copy data back. */

        if (cnt > buflen) {
		bcopy(tmpbuf, buf, buflen) ;
		free(tmpbuf) ;
	}

close_n_ret:
        close(sock) ;

	return rv ;
}

/*
 * mkpbe support
 */

/* 
 * Convert the 4 number ipaddr to an ascii string. This is used
 * to build the rexec command. rexec 192.XXXX ... 
 */

void
make_ipaddr(char *ipaddr)
{
	char tmp[MKPD_MAX_IPADDR_ASCII_LEN] ;

	sprintf(tmp, "%d.%d.%d.%d",
		ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]) ;
	strcpy(ipaddr, tmp) ;

	if (debug)
		printf("ipaddr : %s\n", ipaddr) ;
}

/*
 * Execute the mkpbe command locally. This does not need rexec.
 * System will do.
 */
int
becmdExecuteLocal(becmd_t *bec)
{
	if (bec) {
		if (debug)
	 		printf("system %s\n", bec->bec_becl) ;
		system(bec->bec_becl) ;
	}
	return 1 ;
}

/*
 * Execute a mkpbe command remotely.
 */
int
becmdExecute(becmd_t *bec, char *errbuf)
{
	char ipaddr[MKPD_MAX_IPADDR_ASCII_LEN] ;

	/* check if it is a command for the local partition. */

	if (bec->bec_partid == get_my_partid()) {
		becmdExecuteLocal(bec);
		return 1 ;
	}

	/* Get the IPADDR of the remote partition. */

	if ((get_data_from_local(OPCODE_GET_IPADDR, 
			bec->bec_partid, ipaddr, sizeof(ipaddr)) < 0)
				|| (*ipaddr == 0)) {
		strcpy(errbuf, msglib[MSG_REMOTE_EADDR]) ;
		return -1 ;
	}

	/* Convert ipaddr to ascii string. */

	make_ipaddr(ipaddr) ;

	strncpy(	part_ipaddr[bec->bec_partid].ipaddr, ipaddr, 
			MKPD_MAX_IPADDR_ASCII_LEN) ;
	
	/* 
	 * rexec the command. If rexec fails due to any reason,
	 * rexec itself will print an err msg. If the command
 	 * fails at the remote end, it will send back a msg
	 * which is printed out by the printf below.
 	 */
	if (rexec_mkpbe(ipaddr, bec->bec_becl, errbuf, 1) < 0) {
                fprintf(stderr, "%s\n", errbuf) ;
		return -1 ;
	}

	return 1 ;
}

void
mk_scan_mkpbe(becmd_t *bec, char *cmd)
{
        strcpy(cmd, bec->bec_becl) ;
        /* Insert the -s option as the becl is already setup correctly. */
        cmd[17] = '-' ;
        cmd[18] = 's' ;
}

/*
 * becmdRexecScanLocal
 *
 *	Run the mkpbe scan command locally.
 */
becmdRexecScanLocal(becmd_t *bec)
{
        char    cmd[MAX_BECMD_LEN]   ;
	FILE	*fh ;
	char 	buf[BUFSIZ] ;

	*buf = 0 ;
        if (bec) {
		mk_scan_mkpbe(bec, cmd) ;
                if (debug)
                        printf("popen %s\n", cmd) ;
        	if ((fh = popen(cmd, "r")) != NULL) {
			fgets(buf, BUFSIZ, fh) ;
			if (*buf) {
				fprintf(stdout, "%s\n", (buf+1)) ;
				return -1 ;
			}
		}
        }
        return 1 ;
}

/*
 * becmdRexecScan
 *
 *	Run a dummy rexec command on all partitions first.
 *	If one of them fails we know that the next rexec
 *	to the same partition is bound to fail. We will 
 * 	not be able to completely change the config. Abort
 *	the command, keeping the system config intact.
 */
int
becmdRexecScan(becmd_t *bec, char *errbuf, int local_flag)
{
        char 	ipaddr[MKPD_MAX_IPADDR_ASCII_LEN] ;
	char	cmd[MAX_BECMD_LEN] ;

        /* check if it is a command for the local partition. */

	if ((bec->bec_partid == get_my_partid()) || (local_flag))
                return(becmdRexecScanLocal(bec)) ;

        if ((get_data_from_local(OPCODE_GET_IPADDR,
                        bec->bec_partid, ipaddr, sizeof(ipaddr)) < 0)
				|| (*ipaddr == 0)) {
                strcpy(errbuf, msglib[MSG_REMOTE_EADDR]) ;
                return -1 ;
        }

	make_ipaddr(ipaddr) ;

	/* 
	 * Remote exec the mkpbe cmd, with the -s arg. -s will
	 * just scan all the modules and try to read the NVRAM
	 * on them. If any one of them fails, the scan is deemed
 	 * to be a failure.
	 */

	mk_scan_mkpbe(bec, cmd) ;

        if (rexec_mkpbe(ipaddr, cmd, errbuf, 1) < 0) {
		fprintf(stderr, "%s\n", errbuf) ;
		return -1 ;
	}

	return 1 ;
}

/*
 * becmdExecuteList
 * 
 * 	Execute the given commands on the respective partitions
 *	as specified in the becmd_t list.
 */
int
becmdExecuteList(becmd_t *bech, char *errbuf, int local_flag)
{
	becmd_t 	*bec = bech ;
	int		rv = 1 ;
	int		errmsg_flag = 1 ;	/* Yes on error reporting */

	/* 
	 * Try to lock all partitions. If any one of them fail,
	 * unlock all of them. Supress error reporting in unlock.
	 * We are bound to encounter some errors that have to be
	 * ignored.
	 */

	if ((!force_option) && (!local_flag) && 
			(lock_all_part(bech, errbuf) < 0)) {
		rv = -1 ;
		errmsg_flag = 0 ; 	/* supress unlock err msg */
		goto unlock_ret ;
	}

	/* 
	 * Scan all the partitions to see if rexec works
	 * successfully before running the actual command.
	 * If rexec fails now, we retain the old state of 
	 * the partitions.
	 */
	while (bec) {
		if (becmdRexecScan(bec, errbuf, local_flag) < 0) {
			if (!*errbuf)
			sprintf(errbuf, msglib[MSG_REXEC_FAILED],
				bec->bec_partid) ;
			rv = -1 ;
			goto unlock_ret ;
		}
                bec = bec->bec_next ;
	}

	if (debug)
		printf("Rexec scan done\n") ;

	/* Execute the given mkpbe command on the respective partitions. */

	bec = bech ;
	while (bec) {
		if (local_flag) {
                        if (becmdExecuteLocal(bec) < 0) {
				rv = -1 ;
				goto unlock_ret ;
			}
		} else {
  			if (becmdExecute(bec, errbuf) < 0) {
				rv = -1 ;
				goto unlock_ret ;
			}
		}
		bec = bec->bec_next ;
	}

unlock_ret:
	if ((!force_option)  && (!local_flag) 
			&& (unlock_all_part(bech, errbuf, errmsg_flag)) < 0)
		rv = -1 ;

	return rv ;
}

/*
 * mkpart -p X -m Y Z support.
 */

int 
part_make(partcfg_t *pc_req, char *errbuf)
{
	partcfg_t   	*pc_act ;
	partcfg_t	*pc = pc_req ;
	partid_t	part ;
	partcfg_t	*pc_tmp ;
	becmd_t		*bech = NULL, *bec ;
	int 		i ;
	int		local_flag = 0 ;
	int		rv = PART_MAKE_NORMAL ;

	/* Get a list of partitions found and their constituent modules. */

	pc_act = part_scan(0, errbuf) ;
	if (!pc_act) {
	/* 
	 * No partitions found. Assigning partids to modules will
	 * not involve any remote partitions.
	 */
		fprintf(stderr, "WARNING: Assigning Partition ids now.\n") ;
		local_flag = 1 ;
		part = INVALID_PARTID ;
		rv = PART_MAKE_UNPARTITIONED ;
	}

	/* 
	 * For all the partitions/modules in the required config
	 * lookup the module in the actual config. Build a mkpbe
 	 * command line for the existing partition. Add the required
	 * module number and its new partition number to this 
 	 * command line. This will be ultimately remotely executed.
	 */
	while (pc) {
		for (i=0; i < partcfgNumMods(pc); i++) {
			if (!local_flag) {
	    			pc_tmp = partcfgLookupPart(pc_act, 
						(partid_t) -1,
						partcfgModuleId(pc, i)) ;
	    			if (pc_tmp)
	        			part = partcfgId(pc_tmp) ;
	    			else { 	/* Module not found. */
					strcpy(	errbuf, 
						msglib[MSG_INVALID_CONFIG]) ;
					rv = -1 ;
					goto free_n_ret ;
	    			}
			}

			/* Alloc a becmd_t for the partition part */
			/* On a non-partitioned system, only one
			 * list is created which can be execed on 
			 * the local machine.
			 */
	    		bec = becmdAllocate(&bech, part, BECMD_NAME) ;
	    		if (bec == NULL) {
				rv = -1 ;
				goto free_n_ret ;
			}

			/* Add new partition and mod id to command line */

	    		bec->bec_partid = part ;
	    		becmdAddModPart(bec, partcfgModuleId(pc, i), 
				partcfgId(pc)) ;
 		}
		pc = partcfgNext(pc) ;
    	}

    	if (debug) {
		printf("Remote commands to be executed ... \n") ;
		becmdDumpList(bech) ;
	}

	bzero(part_ipaddr, sizeof(part_ipaddr)) ;

	/* Execute the commands built up for each existing partition. */

    	if (becmdExecuteList(bech, errbuf, local_flag) < 0) {
		rv = -1 ;
	}

free_n_ret:
	becmdFreeList(bech) ;
    	return rv ;
}

/*
 * part_activate
 *
 * 	Re-discover all partitions now.
 */

/* ARGSUSED */

int
part_activate(char *errbuf)
{
        if (0 > syssgi( SGI_PART_OPERATIONS,
                        SYSSGI_PARTOP_ACTIVATE, 0, 0)) {
                sprintf(errbuf, "Partition activate: %s", strerror(errno)) ;
                return -1 ;
        }

	return 1 ;
}

/*
 * part_kill
 *
 *	Dissociate from a specific partition.
 *
 * 	TBD using sys call XXX
 */
/* ARGSUSED */

int
part_kill(partcfg_t *pc, char *errbuf)
{
        if (0 > syssgi( SGI_PART_OPERATIONS,
                        SYSSGI_PARTOP_DEACTIVATE, partcfgId(pc), 0)) {
		sprintf(errbuf, "Partition kill: %s", strerror(errno)) ;
		return -1 ;
	}

	return 1 ;
}

/*
 * get_my_partid
 *	Gets own partition id.
 */
partid_t
get_my_partid(void)
{
	partid_t            mypid ;

	mypid = syssgi(	SGI_PART_OPERATIONS,
                        SYSSGI_PARTOP_GETPARTID, 0, 0);
	if (mypid < 0) {
        	return INVALID_PARTID ;
	}
	return mypid ;
}

/*
 * check_all_modules
 *
 * Check if all modules are accounted for.
 *
 * Return:
 *
 *	-1 if error
 * 	NULL if all mods are accounted for
 *	valid ptr to partcfg list if some mods are left out.
 */
partcfg_t *
check_all_modules(partcfg_t *pc_req, partcfg_t *pc_act, char *errbuf, int *stat)
{
	partcfg_t	*pc = pc_act ;
	partcfg_t	*pc_new = NULL ;
	int		i ;

	*stat = 0 ;

	/* for all modules in pc_act */

	while (pc) {
		for (i = 0; i < partcfgNumMods(pc); i++) {
			/* if it is in pc_req, go on to the next */
			if (	partcfgLookupPart(pc_req, (partid_t)-1, 
						partcfgModuleId(pc,i)))
				continue ;

			/* We found a module not accounted for, but has
			 * the same partition id as in pc_req. The user
			 * is breaking an existing partition, but has
			 * used a existing partition number.
			 * For ex, part 3 had modules 4,5,6,7.
			 * User wants 4 and 5 in part 3 but has not
			 * specified what to do with 6 and 7
			 */
			if (	partcfgLookupPart(pc_req, partcfgId(pc),
							(moduleid_t) -1)) {
				sprintf(errbuf, msglib[MSG_ALL_MODULE],
					partcfgModuleId(pc,i), partcfgId(pc)) ;
				*stat = -1 ;
				return NULL ;
			}

			addPnToPcList(&pc_new, partcfgId(pc),
						partcfgModuleId(pc,i)) ;
		}
		pc = partcfgNext(pc) ;
	}

	if (pc_new) {
		fprintf(stderr, "Some modules were left out. ") ;
		fprintf(stderr, "The complete partition config looks like ...\n") ;
		partcfgDumpList(pc_new) ;
		partcfgDumpList(pc_req) ;
		if (yes_or_no("Is this OK? [y/n]: "))
			return pc_new ;
		else {
			strcpy(errbuf, exit_now_msg) ;
			*stat = -1 ;
			return NULL ;
		}
	}

	return pc_new ;
}

/*
 * Check if the given command input satisfies some basic checks.
 * Build a complete map of the routers in the system.
 */
int
check_sanity(partcfg_t	*pch, char *errbuf)
{
	part_rou_map_t	*pmap ;
	sys_rou_map_t	sys_rmap ;
	partcfg_t	*pc_act, *pc ;
	partcfg_t	*pc_req = pch ;
	partcfg_t	*pc_new = NULL ;
	int		i = 0 ;
	int		status ;
	int		mr_flag = 0 ;

	if (pch == NULL)
		return -1 ;

	if (debug)
		printf("Checking sanity ... \n") ;

	if (check_hub_version() < 0)
		return -1 ;

	pc_act 	= part_scan(0, NULL) ;
	if (!pc_act) {
		fprintf(stderr, "WARNING: This is a unpartitioned system.\n"
			"Perform manual sanity check of required config\n") ;
		return 1 ;
	}

	/* Basic checks. */

	*errbuf = 0 ;
	if (partcfgValidList(pch, errbuf) < 0) {
		if (!(*errbuf))
			strcpy(errbuf, msglib[MSG_INVALID_CONFIG]) ;
		return -1 ;
	}

	if (debug)
		printf("        List is valid.\n") ;

	/*
	 * pc_req may be a subset of pc_act.
	 * Find the diff and add it to pc_req.
	 * pc req must include all modules found in pc_act.
	 */

	pc_new = check_all_modules(pc_req, pc_act, errbuf, &status) ;
	if (status < 0)
		return -1 ;

	if (debug)
		printf("        All modules included now.\n") ;

	/*
	 * Check if a partition if of 0 has sneaked in.
	 * We cannot allow a partition id of 0 on partitioned 
 	 * systems. The router workaround is not performed
	 * on unpartitioned systems which is now assumed to be
	 * indicated by a partition id of 0.
	 */

	check_for_partid0(pc_req) ;
	check_for_partid0(pc_new) ;

	/*
 	 * Sanity checks for meta routers are different.
	 */

	if (meta_routers_present()) {

                /* If all modules are in 1 partition it is OK. */

                if ((!pc_new) && (count_partitions(pc_req) == 1))
                        return 1 ;

                mr_flag = 1 ;
	} else if (debug)
                        printf("        Meta routers not present.\n") ;

	/*
 	 * Check if total Number of modules and number of
	 * modules look like a mktg/array permitted
	 * config.
	 * For now we know that array permits only 4 module
	 * and 8 and 16 module configs??
	 * Also, we know that due to router table distribution
	 * methods, the number of mods in a part should be a 
	 * power of 2.
 	 */

	if (check_numbers(pc_act, pc_req, errbuf) < 0)
		return -1 ;

        /* Check if the required config matches any of the
         * marketing support valid configs. If not, warn the
         * user for now.
         */
        if (check_valid_config(pc_act, pc_req, pc_new, vc_hdr_p) < 0) {
                printf("WARNING: The desired partition config is not "
                        "one of the standard\n"
			" supported configs. "
                        "Continuing to check if other requirements are met.\n");        }

	if (debug)
		printf("        Mktg permitted config.\n") ;

	/* Build the router map of the whole system. */
	/* Get router map info from all partitions. */

	if (debug)
		printf("        Building complete router map\n") ;

	pc	= pc_act ;
	sys_rmap.sze	= MAX_PARTITION_PER_SYSTEM ;

        while (pc) {

		/* Allocate partition router map */

		pmap = (part_rou_map_t *)malloc(sizeof(part_rou_map_t)) ;
		if (pmap == NULL)
			return -1 ;
		part_rou_map_init(pmap) ; 	/* Init it to 0s */

		sys_rmap.pr_map[i] = pmap ; 	/* Install in sys rou map */

		if (partcfgId(pc) == get_my_partid()) { 	/* Local */
			create_my_rou_map(partcfgId(pc),
				pmap, sizeof(part_rou_map_t)) ;
			if (debug)
				printf("        local rou map created\n") ;
		} else {
			if (debug)
				printf("        Getting router map of "
					"partition %d\n", partcfgId(pc)) ;
			get_data_from_local(OPCODE_GET_ROUCFG, 	/* Remote */
                        			partcfgId(pc), 
					(char *)pmap, sizeof(part_rou_map_t)) ;
		}

		if (pmap->cnt == 0) {
			sprintf(errbuf, msglib[MSG_ZERO_ROUTER], partcfgId(pc));
			return -1 ;
		}

		/* 
		 * Update the partition index i into all router structs
		 * in pmap. This is used to xref a router struct
		 * from the port info of another router struct. 
 		 */
		if (update_pmap(pmap, i) < 0) {
			sprintf(errbuf, msglib[MSG_ROUMAP_ERR], partcfgId(pc)) ;
			return -1 ;
		}

                pc = partcfgNext(pc) ;
		i++ ;
        }
	sys_rmap.pr_map[i] 	= NULL ;
	sys_rmap.cnt 		= i ;

	/* Interlink the ports of each router to the respective
	   router map structure. */

	if (link_all_roumap(&sys_rmap, errbuf) < 0)
		return -1 ;

	if (debug)
		dump_sys_rmap(&sys_rmap) ;

	/* 
	 * check if the route between any 2 routers in the
	 * proposed system is contained within a proposed
	 * partition.
	 */

	if (check_contiguous(&sys_rmap, pch, errbuf, mr_flag) < 0)
		return -1 ;

	if (pc_new)
        	if (check_contiguous(&sys_rmap, pc_new, errbuf, mr_flag) < 0)
                	return -1 ;

	/*
 	 * Check if there exists atleast 1 route between
 	 * any 2 partitions, that does not go through a
 	 * 3 rd partition.
 	 */
	if (check_interconn(&sys_rmap, pch, errbuf, mr_flag) < 0)
		return -1 ;

	if (pc_new)
        	if (check_interconn(&sys_rmap, pc_new, errbuf, mr_flag) < 0)
                	return -1 ;

	return(1) ;
}


/*
 * rexec_mkpbe
 *
 *	Run the mkpart back end command remotely using rexec.
 */
int
rexec_mkpbe(char *host, char *cmd, char *errbuf, int flag)
{
	struct servent 	*se ;
	int	 	    	rstm ;
	char		buf[1024] ;

	se = getservbyname("exec", "tcp") ;
	/* XXX change user name ?? */

	/* rexec prints an err msg in case of problems. */

	if ((rstm = rexec(&host, se->s_port, "root", "\n", cmd, NULL)) < 0)
		return -1 ;

	if (debug)
		printf("%s, %d\n", cmd, rstm) ;

	*buf = 0 ;
	read(rstm, buf, 1024) ;

	/* In this case, a non-zero value in the first byte
 	 * means error. It is also an error code.
 	 */
	if ((flag) && (*buf)) {
		if (debug) {
			printf("%d: ", *buf) ;
			printf("%s\n", ((char *)buf)+1) ;
		}
		strcpy(errbuf, ((char *)buf)+1) ;
		return -1 ;
	}
    
	return 1 ;
}

/*
 * lock_all_part
 *
 *	set the lock variable to the locking part id.
 * 	Return -1 if we fail on the first partid.
 */
static int
lock_all_part(becmd_t *bech, char *errbuf)
{
	becmd_t 	*bec 	= bech ;
	char		status_buf[16] ;
	int		rc ;

        while (bec) {
        	rc = get_data_from_local(OPCODE_LOCK_PART,
                        	bec->bec_partid, status_buf, 16) ;
		if (rc < 0) {
			if (rc == -1)
			sprintf(errbuf, "Cannot contact local mkpd daemon."
					" Please check if mkpd is on.\n") ;
			else
                	sprintf(errbuf, msglib[MSG_LOCK_ERR], 
			((bec->bec_partid == INVALID_PARTID)?
				0:bec->bec_partid)) ;
                       	return -1 ;
		}
                bec = bec->bec_next ;
        }
	return 1 ;
}

/* 
 * unlock_all_part
 *
 * 	tries to unlock all partitions, even if some of them fail
 *	reports err if flag is set.
 */
static int
unlock_all_part(becmd_t *bech, char *errbuf, int errmsg_flag)
{
	becmd_t 	*bec 	= bech ;
	int		rv 	= 1 ;
	char		status_buf[16] ;

        while (bec) {
        	if (get_data_from_local(OPCODE_UNLOCK_PART,
                        	bec->bec_partid, status_buf, 16) < 0) {
			if ((rv >= 0) && (errmsg_flag)) {
                		sprintf(errbuf, msglib[MSG_UNLOCK_ERR], 
					bec->bec_partid) ;
                        	rv = -1 ;
			}
		}
                bec = bec->bec_next ;
        }
	return rv ;
}

#include <signal.h>

static void
sigint_handler(void)
{
	/* fprintf(stderr, "Ignoring interrupt.\n") ; */
}

void
ignore_signal(int signal, int flags)
{
	struct sigaction	act ;

	act.sa_flags 	= flags ;
	act.sa_handler 	= sigint_handler ;
	sigemptyset(&act.sa_mask) ;
	sigaction(signal, &act, NULL) ;
}

void
logmsg(char *msg, ...)
{
	va_list	args ;
	
	va_start(args, msg) ;
	vsyslog(LOG_ERR, msg, args) ;
}

int
isModuleRebooted(moduleid_t m, moduleid_t *mlist)
{
	int	i = 0 ;

	while (mlist[i])
		if (mlist[i++] == m)
			return 1 ;
	return 0 ;
}

/* ARGSUSED */

int
check_part_reboot_state(pn_t *pn, int cnt, moduleid_t *mlist, char *errbuf)
{
	int	i ;
	int	found = 0 ;

	for (i = 0; i < cnt; i++) {
		if (isModuleRebooted(pn[i].pn_module, mlist)) {
			found = 1 ;
			if (pn[i].pn_state != KLP_STATE_PROM)
				return 0 ;
		}
	}
	if (found)
		return 1 ;
	else
		return 0 ;
}

void
dump_mlist(moduleid_t *mlist)
{
	int	i = 0 ;

	printf("Module List >-> ") ;
	while (mlist[i])
		printf("%d ", mlist[i++]) ;
	printf("\n") ;
}

void
fill_mlist(partid_t p, partcfg_t *pc_act, moduleid_t *mlist, int *new)
{
	int		i, j ;
	partcfg_t	*pc_tmp ;

        pc_tmp = partcfgLookupPart(pc_act, p, (moduleid_t)-1) ;
	if (pc_tmp) {
		for(i = 0, j = *new ; i < partcfgNumMods(pc_tmp) ; i++)
			mlist[j+i] = partcfgModuleId(pc_tmp, i) ;
		*new = j+i ;
	}
}

void
shutdown_self(void)
{
        fprintf(stderr, "%s", shutdown_self_msg) ;
        system("echo 1|init 0") ;
}

void
shutdown_self_query(char *errbuf)
{
        if (yes_or_no("Want to reboot now? [y/n]: ")) {
		shutdown_self() ;
        } else {
               	strcpy(errbuf, exit_now_msg) ;
               	return ;
        }
}

/* ARGSUSED */

#define	TOTAL_WAIT_MINS		3
#define TOTAL_WAIT_SECS		60*TOTAL_WAIT_MINS
#define WAIT_INTERVAL_SECS	10

int
wait_reboot_self(	partcfg_t 	*pc, 
			partcfg_t 	*pc_act, 
			moduleid_t 	*mlist, 
			char 		*errbuf,
			int		mr_flag)
{
	int	done = 0 ;
	int	wait_time = 0 ;
	pn_t	*pn ;
	int	cnt ;

	/* On meta router systems we do not do sync reboot. If 
	 * partitions are combined, they have to be combined to
	 * a single partition. Just shutdown self, dont wait.

	if (mr_flag)
		goto shut_self ;
	 */

        pn = (pn_t *)malloc(MAX_PN * sizeof(pn_t)) ;

	while (!done) {
        	if (0 > syssgi( SGI_PART_OPERATIONS,
                        	SYSSGI_PARTOP_ACTIVATE, 0, 0, 0)) {
			continue ;
        	}

		sleep(WAIT_INTERVAL_SECS) ;

        	cnt = syssgi(   SGI_PART_OPERATIONS,
                        	SYSSGI_PARTOP_NODEMAP, MAX_PN, pn, NULL) ;
		{
			int i ;
			printf("cnt = %d >-> ", cnt) ;
			for(i=0;i<cnt;i++) 
				printf("%d:%d,", 
					pn[i].pn_state, pn[i].pn_module) ;
			printf("\n") ;
		}
#if 0 /* DEBUG */
#endif

        	if (cnt < 0)
                	continue ;

		if (check_part_reboot_state(pn, cnt, mlist, errbuf))
			break ;

#ifdef MANUAL_WAIT
		if (yes_or_no("Are other parts done yet? [y/n]: "))
			done = 1 ;
		else
			exit(1) ;
#endif

		wait_time += WAIT_INTERVAL_SECS ;
		if (wait_time >= TOTAL_WAIT_SECS)
			return -1 ;
	}

	/* shutdown self */

	shutdown_self() ;

	return 1 ;
}

void
part_reboot(	partcfg_t 	*pc, 
		partcfg_t 	*pc_act, 
		moduleid_t 	*mlist, 
		int		flag,
		char 		*errbuf)
{
	int		i ;
	partcfg_t	*pc_tmp ;
	int		index = 0 ;

	/* Flag all partitions to be rebooted */

        for (i=0; i < partcfgNumMods(pc); i++) {
               pc_tmp = partcfgLookupPart(pc_act, (partid_t) -1,
                       	partcfgModuleId(pc, i)) ;
               if (pc_tmp) 
			part_ipaddr[partcfgId(pc_tmp)].flags = PART_REBOOT ;
        }

	/* shutdown all affected partitions */

	for (i=0; i<MAX_PARTITION_PER_SYSTEM; i++) {
		if (i == get_my_partid())
			continue ;
		if (part_ipaddr[i].flags == PART_REBOOT) {
			fill_mlist(i, pc_act, mlist, &index) ;
			fprintf(stderr, "Rebooting partition %d\n", i) ;
			if (flag)
				rexec_mkpbe(part_ipaddr[i].ipaddr, 
					PART_REBOOT_CMD, errbuf, 0) ;
			else {
				rexec_mkpbe(part_ipaddr[i].ipaddr, 
					PART_REBOOT_SHUTDOWN_CMD, errbuf, 0) ;
			}
			part_ipaddr[i].flags = PART_REBOOT_DONE ;
		}
	}
	dump_mlist(mlist) ;
#if 0
#endif
}

void
inquire_and_reboot(partcfg_t *pc_req, char *errbuf)
{
	partcfg_t	*pc_act, *pc_tmp ;
	partcfg_t	*pc = pc_req, *mypc = NULL ;
	partid_t	partid ;
	int		i, found ;
	int		combine = 0 ;
	moduleid_t	mlist[MAX_MODS_PER_PART] , mymlist[MAX_MODS_PER_PART] ;
	int		mr_flag ;
	int		reboot_flag = 0 ;
	char 		*reboot_other_msg = 
"NOTE: Rebooting partitions that do not affect current partition.\n\
They have to be checked manually.\n" ;

	bzero(mlist, sizeof(mlist)) ;

	pc_act = part_scan(0, errbuf) ;
	if (!pc_act)
		return ;

	/* If we have meta routers on this system,
	 * do not do this synchronized reboot.
	 */

	mr_flag = meta_routers_present() ;

	/* Find all NEW partitions that will be combined. */

	while (pc) {
                pc_tmp = partcfgLookupPart(pc_act, (partid_t) -1,
                                           partcfgModuleId(pc, 0)) ;
		if (pc_tmp)
			partid = partcfgId(pc_tmp) ;

		for (i=0; i < partcfgNumMods(pc); i++) {
                        pc_tmp = partcfgLookupPart(pc_act, (partid_t) -1,
                                                partcfgModuleId(pc, i)) ;
                        if ((pc_tmp) && (partcfgId(pc_tmp) != partid)) {
				pc->flags |= PART_COMBINE ;
				combine = 1 ;
			}
		}
                pc = partcfgNext(pc) ;
	}

	if (!combine) {
		shutdown_self_query(errbuf) ;
		return ;
#if 0
		if (yes_or_no("Want to reboot now? [y/n]: ")) {
			printf(shutdown_self_msg) ;
			system("echo 1|init 0") ;
		} else {
			strcpy(errbuf, exit_now_msg) ;
			return ;
		}
#endif
	}

	/* Ask the user for a reboot */

	pc = pc_req ;
	while(pc) {
		if (pc->flags & PART_COMBINE) {
			fprintf(stderr, "Partition %d combines 2 or more "
				"existing partitions\n", partcfgId(pc)) ;

			if (reboot_flag = 
				yes_or_no(	"Reboot them now? [y/n]: ")) {
				if (is_my_act_part(pc, pc_act, get_my_partid()))
				{
					mypc = pc ;
					found = 1 ;
				} else {
					found = 0 ;
					fprintf(stderr, reboot_other_msg) ;
				}

				/* If this is a meta router system, do not
				 * do the nvram command. The IOprom need not
				 * wait, for a reboot.

				if (mr_flag)
					found = 0 ;
				 */

				part_reboot(pc, pc_act, mlist, found, errbuf) ;
				if (mypc && found) {
					bcopy(mlist, mymlist, sizeof(mlist)) ;
				}
			}
		}

		/* XXX re-init globals if any */

                pc = partcfgNext(pc) ;
	}

	if (mypc && reboot_flag) {
		dump_mlist(mymlist) ;
#if 0
#endif
		wait_reboot_self(mypc, pc_act, mymlist, errbuf, mr_flag) ;
	}
}

int
yes_or_no(char *msg)
{
	char buf[256] ;

        *buf = 0 ;
        while ((*buf != 'y') && (*buf != 'n')) {
                fprintf(stderr, "%s", msg) ;
                gets(buf) ;
        }

        if (*buf == 'y')
                return 1 ;
	else
		return 0 ;
}

#if 0

void
dump_kldir_state(void)
{
	pn_t	*pn ;
	int 	i ;
	int	cnt ;

        pn = (pn_t *)malloc(MAX_PN * sizeof(pn_t)) ;

        if (0 > syssgi(SGI_PART_OPERATIONS,
                       SYSSGI_PARTOP_ACTIVATE, 0, 0, 0)) {
                return ;
        }

        sleep(5) ;

        cnt = syssgi(SGI_PART_OPERATIONS,
                     SYSSGI_PARTOP_NODEMAP, MAX_PN, pn, NULL) ;

	{
	printf("cnt = %d >-> \n", cnt) ;
	for(i=0;i<cnt;i++) printf("s.%d:m.%d:p.%d:n:%d\n", 
		pn[i].pn_state, pn[i].pn_module, 
		pn[i].pn_partid, pn[i].pn_nasid) ;
	printf("\n") ;
	}

	free(pn) ;
}
#endif

static int
is_my_act_part(partcfg_t *pc, partcfg_t *pc_act, partid_t partid)
{
	int		i ;
	partcfg_t 	*pc_tmp = NULL ;

        for (i=0; i < partcfgNumMods(pc); i++) {
                pc_tmp = partcfgLookupPart(pc_act, (partid_t) -1,
                                           partcfgModuleId(pc, i)) ;
                if ((pc_tmp) && (partcfgId(pc_tmp) == partid)) {
			return 1 ;
                }
	}
	return 0 ;
}

static int
is_xpc_up(partid_t p)
{
	int	fd ;
        char    name[MKPD_MAX_XRAW_FNAME_LEN] ;
	struct stat	buf ;

        if (get_raw_path(name, p) < 0)
                return 0 ;

        fd = stat(name, &buf) ;
        if (fd < 0) {
                if (debug) {
                        perror("stat") ;
                }
		return 0 ;
	}
	return 1 ;
}

static int
meta_routers_present(void)
{
        part_rou_map_t  *pmap ;
	int		i ;
	rou_map_t	*rm ;

        pmap = (part_rou_map_t *)malloc(sizeof(part_rou_map_t)) ;
        if (pmap == NULL)
               return 0 ;

        part_rou_map_init(pmap) ;       /* Init it to 0s */

        create_my_rou_map(get_my_partid(),
                                pmap, sizeof(part_rou_map_t)) ;

        for (i=0; i< pmap->cnt; i++) {
                rm = &pmap->roumap[i] ;

                if (is_metarouter(rm)) {
			if (pmap)
				free(pmap) ;
			if (debug)
				printf("        Meta routers present.\n") ;
			return 1 ;
                }
        }

	if (pmap)
		free(pmap) ;
	if (debug)
		printf("        Meta routers not present.\n") ;

	return 0 ;
}

int
count_partitions(partcfg_t *pc)
{
	int 		count = 0 ;
	partcfg_t 	*pc_tmp = pc ;

	while (pc_tmp) {
		count ++ ;
		pc_tmp = partcfgNext(pc_tmp) ;
	}
	return count ;
}

void
inquire_and_reboot_init(char *errbuf)
{
	partcfg_t	*pc ;
	partcfg_t	*pc_req ;
	int		i ;

        if (pc_req = partcfgCreate(MAX_MODS_PER_PART)) {
                partcfgId(pc_req) = 0 ;
        } else
		return ;

        pc = part_scan(0, errbuf) ;
        if (!pc)
		return ;

	while (pc) {
		for (i=0;i<partcfgNumMods(pc);i++)
			addPnToPcList(&pc_req, 0, partcfgModuleId(pc, i)) ;
		pc = partcfgNext(pc) ;
	}

	if (debug) {
		printf("Required config looks like ...\n") ;
		partcfgDump(pc_req) ;
	}

	inquire_and_reboot(pc_req, errbuf) ;

}

#include <invent.h>
#include <ftw.h>
#include <sys/attributes.h>
#include <sys/iograph.h>
#include <paths.h>

#define MAX_WALK_DEPTH 32

/* ARGSUSED */

int
check_hub_version_cb(const char *p, const struct stat *s, int i, struct FTW *f)
{
	struct stat buf;
	char info[256], node_name[256] ;
	int len = sizeof(info);
	int rc;
	invent_miscinfo_t	*minvent ;
	invent_generic_t	*ginvent ;


	if (lstat(p,&buf))              /* Make sure lstat succeeds */
		return 0;
	if (S_ISLNK(buf.st_mode))       /* Check if we are looking at a link*/
		return 0;
	rc = attr_get((char *)p, INFO_LBL_DETAIL_INVENT, info, &len, 0);
	if (rc == 0) {
		minvent = (invent_miscinfo_t *) info ;
		ginvent = (invent_generic_t *)info ;
		if (	(ginvent->ig_invclass == INV_MISC) 	&&
			(minvent->im_type == INV_HUB)  		&&
			(minvent->im_rev < 5)) {
			strcpy(node_name, p) ;
			node_name[strlen(node_name)-strlen("/node/hub")] = 0 ;
			fprintf(stderr, "The node board %s is not at the "
				"required \n"
				"revision level for partitioning."
				" Use -F option to override this.\n", 
				node_name) ;
			return 2 ;
		}
	}
	return 0 ;
}

int
check_hub_version(void)
{
        if (nftw(_PATH_HWGFS, check_hub_version_cb, MAX_WALK_DEPTH, FTW_PHYS)
			== 2)
		return -1 ;
	else
		return 1 ;
}

/* Valid Config Support */

/* This section contains support to check for certain
 * specific partition configs and alert the user about
 * them. For example, on a 8 module machine, we can
 * allow the user to do a 2 X 4 module partition only.
 * Any other partitioning is not supported and is the
 * responsibility of the user.
 */

/* The list of supported configs is read from the file
 * /etc/config/mkpart.config. This is not user modifiable.
 * It has a weird fixed format that can be understood by
 * the mkpart only. In case the info cannot be obtained from
 * the file, the data struct below provides it.
 */

/* The interpretation of various fields are:
 * NMods   NCfgs   <NCombos, (NPartTypes X NModsPerPart) + > ... 
 * For a NMods number of modules system, eg: 4
 * There are NCfgs number of configs supported eg: 3
 * (other than the default of all mods in 1 partition) 
 * There are 3 sets of info that follow NCfgs. NCombos is
 * the different combinations that follow eg: 2
 * NPartTypes X NModsPerPart is the number of partitions 
 * allowed and the modules per partition eg 2 X 1 
 * So for a 4 module system we can have 3 combinations of
 * partitioning, 4 partitions with 1 mod per partition OR
 * 2 partitions with 2 modules per partition OR 2 partitions
 * 1 partition with 2 modules and 2 partitions with 1 module each.
 */

char *valid_config_default[] = {
"2 2 1 1 2 1 2 1",
"4 4 1 1 4 1 4 1 1 2 2 2 1 2 2 1",
"8 2 1 1 8 1 2 4",
"16 2 1 1 16 1 4 4",
NULL
} ;

/*
 * Parse a line and store its info in valid_config_t
 * element given. Return -1 if something is wrong.
 * Does not clean the element on failure.
 */

int
read_valid_config_line(char *line, valid_config_t *vcp)
{
        int     	j,k ;
        char            tok_buf[MAX_CMDLINE_LEN], *tok_ptr = tok_buf ;
	char		*tmp ;

	if (!line || !vcp)
		return -1 ;

	/* Line too big. */

        if (strlen(line) >= sizeof(tok_buf))
                return -1 ;
        strcpy(tok_ptr, line) ;
        tok_ptr = strtok(tok_ptr, sep) ;

	/* Get next integer from the line */

	if (!(tmp = tok_ptr)) return -1 ;
	if (sscanf(tmp, "%d", &vcTotalModules(vcp)) != 1)
		return -1 ;

	if (!(tmp = strtok(NULL, sep))) return -1 ;
	if (sscanf(tmp, "%d", &vcNConfigs(vcp)) != 1)
		return -1 ;

        for (j=0;j<vcNConfigs(vcp);j++) {

		if (!(tmp = strtok(NULL, sep))) return -1 ;
		if (sscanf(tmp, "%d", &vcComboNCombos(vcp,j)) != 1)
			return -1 ;

                for (k=0;k<vcComboNCombos(vcp,j);k++) {

			if (!(tmp = strtok(NULL, sep))) return -1 ;
			if (sscanf(tmp, "%d", &vcComboPartition(vcp,j,k)) != 1)
				return -1 ;

			if (!(tmp = strtok(NULL, sep))) return -1 ;
			if (sscanf(tmp, "%d", &vcComboModule(vcp,j,k)) != 1)
				return -1 ;

                }
        }

	return 1 ;
}

/* Parse file for the permitted configs info. */

int
read_valid_config_file(char *vc_fname, valid_config_t *vchp)
{
        FILE    	*fp ;
        char            buf[MAX_CMDLINE_LEN] ; /* max len of line in file */
        char            *bufp ;
	int		i = 0 ;

        fp = fopen(vc_fname, "r");
        if (fp == NULL) {
		return -1 ;
	}

	bufp = fgets(buf, MAX_CMDLINE_LEN, fp) ;
	if (bufp) {
		if (strncmp(	bufp, VALID_CONFIG_FILE_MAGIC, 
				strlen(VALID_CONFIG_FILE_MAGIC)))
			return -1 ;
	}
	else 
		return -1 ;

	i = 0 ;
        while (bufp = fgets(buf, MAX_CMDLINE_LEN, fp)) {

                if (!bufp || (i >= MAX_TOTAL_MODULES))
                        return -1 ;

                /* Skip comments, empty lines */

                if (	(*bufp == '#') || (*bufp == '*') || 
			(*bufp == '\n') || (*bufp == '\0'))
                        continue ;

		if (read_valid_config_line(bufp, &vchp[i]) < 0)
			return -1 ;

		i++ ;
	}

	return 1 ;
}

/* Parse local data structure if file does not contain valid info. */

int
read_valid_config_default(valid_config_t *vchp)
{
	int	i = 0 ;

	while (valid_config_default[i]) {

		if (i >= MAX_TOTAL_MODULES)
			return -1 ;

		if (read_valid_config_line(	valid_config_default[i], 
						&vchp[i]) < 0)
			return -1 ;

		i++ ;
	}
	return 1 ;
}

/* Read valid configuration info, either from file or the
 * local data struct. If everything fails, clean the config struct.
 */

void
read_valid_config(valid_config_t *vchp)
{
	if (read_valid_config_file(VALID_CONFIG_FILE, vchp) < 0) {
		bzero(vchp, sizeof(valid_config_t) * MAX_TOTAL_MODULES) ;
		if (read_valid_config_default(vchp) < 0)
                	bzero(vchp, sizeof(valid_config_t)*MAX_TOTAL_MODULES) ;

	}

	if (debug)
		dump_valid_config_all(vchp) ;
}

char *vc_file_hdr = 
"NMods 	NCfgs 	<NCombos, (NPartTypes X NModsPerPart) + > ... \n" ;

void
dump_valid_config(valid_config_t *vcp)
{
	int		nc ;
	int		j,k ;

        printf("%d      %d", vcTotalModules(vcp), vcNConfigs(vcp)) ;
        for (j=0;j<vcNConfigs(vcp);j++) {
                printf("        <%d,", vcComboNCombos(vcp,j)) ;
                nc = vcComboNCombos(vcp,j) ;
                for (k=0;k<nc;k++) {
                        printf(" (%d X %d) ",
                                        vcComboPartition(vcp,j,k),
                                        vcComboModule(vcp,j,k)) ;
                        if (nc>1)
                                        printf(" + ") ;
                }
                printf(">") ;
        }
        printf("\n") ;
}

void
dump_valid_config_all(valid_config_t *vchp)
{
	int		i;
	valid_config_t	*vcp = &vchp[0] ;

	printf("%s", vc_file_hdr) ;

	for (i=0;i<MAX_TOTAL_MODULES;i++) {
		vcp = &vchp[i] ;
		if (!vcValid(vcp))
			continue ;
		dump_valid_config(vcp) ;
	}
}

int
count_total_modules(partcfg_t *pch)
{
        int             count = 0 ;
        partcfg_t       *pc_tmp = pch ;

        while (pc_tmp) {
                count += partcfgNumMods(pc_tmp) ;
                pc_tmp = partcfgNext(pc_tmp) ;
        }
        return count ;
}

/*
 * check_valid_config
 *
 * Checks if a required config matches one of the permitted
 * configurations. Returns -1 if it does not match.
 * The required config may be a combination of pc_req and 
 * pc_new = left out modules.
 */
int
check_valid_config(	partcfg_t *pc_act, 
			partcfg_t *pc_req, 
			partcfg_t *pc_new, 
			valid_config_t *vchp)
{
	int		total_modules = count_total_modules(pc_act) ;
	int		i = 0, j ;
	valid_config_t	*vcp = &vchp[0] ;
	int		found = 0 ;
	partcfg_t	*pc ;
	int		tmp[MAX_CONFIG][MAX_COMBOS];
	int		fail ;

	/* Find if we have an entry for our total_modules in the
 	 * valid config table. 
	 */
	while (vcValid(vcp)) {
		if (vcTotalModules(vcp) == total_modules) {
			found = 1 ;
			break ;
		}
		i++ ;
		vcp = &vchp[i] ;
	}

	if (!found) {
		return -1 ;
	}

	if (debug)
		dump_valid_config(vcp) ;

	/* For all required partitions, check if there is a match
 	 * on number of modules anywhere in valid config list. If
	 * we find a match, put 1 in the tmp array.
 	 */
	pc = pc_req ;
	bzero(tmp, sizeof(tmp)) ;
	while (pc) {
		for (i=0;i<vcNConfigs(vcp); i++) {
			for (j=0;j<vcComboNCombos(vcp,i);j++) {
				if (	vcComboModule(vcp,i,j) == 
					partcfgNumMods(pc)) {
					tmp[i][j] += 1 ;
				}
			}
		}
		pc = partcfgNext(pc) ;
	}

	pc = pc_new ;
        while (pc) {
                for (i=0;i<vcNConfigs(vcp); i++) {
                        for (j=0;j<vcComboNCombos(vcp,i);j++) {
                                if (    vcComboModule(vcp,i,j) ==
                                        partcfgNumMods(pc)) {
                                        tmp[i][j] += 1 ;
                                }
                        }
                }
                pc = partcfgNext(pc) ;
        }

	/* For all config X combos, check if we have a perfect
	 * match on number of partitions in tmp.
	 */
	for (i=0;i<vcNConfigs(vcp); i++) {
		fail = 0 ;
        	for (j=0;j<vcComboNCombos(vcp,i);j++) {
			if (tmp[i][j] != vcComboPartition(vcp,i,j)) {
				fail = 1 ;
				break ;
			}
		}
		if (!fail) break ;
	}

	if (fail) {
		return -1 ;
	}

	return 1 ;
}

static void
check_for_partid0(partcfg_t *pc_req)
{
	partcfg_t	*pc = pc_req ;

	while (pc) {
		if (partcfgId(pc) == 0) {
			printf("WARNING: Partition id 0 should be avoided "
				"on a partitioned system.\n") ;
			if (!yes_or_no("Do you want to continue? [y/n] ")) {
				printf("Exiting the command now.\n") ;
				exit(0) ;
			} else
				return ;
		}
		pc = partcfgNext(pc) ;
	}
}

#ifdef DEBUG_VALID_CONFIG

partcfg_t 	*
generate_pc_from_user(void)
{
	char 		buf[MAX_CMDLINE_LEN] ;
	char		*bufp ;
	partcfg_t       *pc = NULL , *pch = NULL ;
	int		status ;

	printf("Enter partition:p=module:m1 m2 ...\n") ;
	while (bufp = gets(buf)) {
        	pc = partcfgCreate(MAX_MODS_PER_PART) ;
        	parse_string(bufp, pc, &status, NULL) ;
                partcfgInsertList(&pch, pc) ;
	}

	partcfgDumpList(pch) ;

	return pch ;
}

#endif



