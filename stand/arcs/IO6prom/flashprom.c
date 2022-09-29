/***********************************************************************\
*	File:		flashprom.c					*
*									*
*									*
\***********************************************************************/

#include <ctype.h>

#include <libsc.h>
#include <libsk.h>
#include <pgdrv.h>

#include <io6fprom.h>
#include <promgraph.h>

#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#define DEF_IP27_CONFIG_TABLE
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/promhdr.h>
#include <sys/SN/SN0/klhwinit.h>		/* For definitions of LD, SD */
#include <sys/SN/slotnum.h>

#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>

/*
 * SN_PDI - Prom Driver Infrastructure.
 * This is an alternate way of building the prom hwgraph.
 * Since the flash command uses prom hwgraph it needs to
 * take care of both ways. SN_PDI is the new one. The
 * globals, graph handle and root vertex handle are different
 * for both cases, we need this ifdef.
 */
#ifdef SN_PDI
extern vertex_hdl_t	snpdi_rvhdl ;
extern graph_hdl_t	pg_hdl ;
#else
extern vertex_hdl_t	hw_vertex_hdl ;
extern graph_hdl_t	prom_graph_hdl ;
#endif
extern nasid_t		master_nasid;

extern int slot_to_widget() ;
extern int get_my_node_slot_class() ;

#define MAX_IP27_SLOT 4
#define MAX_IO6_SLOT  12

int do_flash_cmd(int, char **);
nasid_t flash_getnasid(int) ;
void _get_cpu_hub_freq(ip27config_t *);
int _fill_in_config_info(ip27config_t *, int);
int _verify_config_info(ip27config_t *);

void 		_get_all_config_vals	(ip27config_t *);
__uint32_t 	get_scache_size		(__uint32_t);


ip27config_t *new_config_info;

#define LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define CONFIG_HDR_OFFSET (PROM_DATA_OFFSET + 0x60)

/* If we are flashing io prom on a speedo with an xbox then
 * target is the flashprom in the xbox.
 */
#define IS_XBOX_FLASHPROM() (SN00 			&& \
			     (flash_which == IO_PROM) 	&& \
			     (xbox_nasid_get() != INVALID_NASID))
/*
 * NOTE: The flash commands use a set of global variables right now
 * since we are flashing one prom at a time. If we have to do them
 * in parallel then this has to be changed.
 */

int		modid, slotid, n, w ;
nasid_t		flashprom_nasid ;
int             widgetid ;
int		foundfile, boardlist, verbose, flash_debug ;
unsigned int	erase_flag ;
unsigned int	count_flag ;
int		confirm_yes ;
int		flash_which;
unsigned int	log_erase_flag ;
unsigned int	check_flag ;
unsigned int	full_check_flag ;
unsigned int	cache_size ;
char		filename[FILENAME_BUF_SIZE];
char		*remain ;
static char	*command;
static int	nasid_option ;
static int	modid_option ;
char 		slotname[SLOTNUM_MAXLENGTH] ;

int
flash_cmd(int argc, char **argv)
{
	flash_which = -1;
	if (do_flash_cmd (argc, argv))
		return 1;
	return 0;
}

void
copy_buffer_adj_sum(ip27config_t *config_info)
{
	__uint64_t             old_sum, new_sum;
	ip27config_t           *config_buff_hdr;


	config_buff_hdr = (ip27config_t *)(
		TO_NODE(get_nasid(), FLASHBUF_BASE) + CONFIG_HDR_OFFSET);

	old_sum = memsum((void *)(TO_NODE(get_nasid(), FLASHBUF_BASE) + 
		CONFIG_HDR_OFFSET), sizeof(ip27config_t))%256;



	bcopy((char *)config_info, (char *) (TO_NODE(get_nasid(), 
		FLASHBUF_BASE) + CONFIG_HDR_OFFSET), sizeof(ip27config_t));
	

	new_sum = memsum((void *) (TO_NODE(get_nasid(), FLASHBUF_BASE) + 
		CONFIG_HDR_OFFSET), sizeof(ip27config_t))%256;



	
	/* now set the appropiate check sum adjust value so
	   that we're back to summing to 0 % 256 */
	if (old_sum>new_sum) {
		config_buff_hdr->check_sum_adj = (uint) (old_sum - new_sum);
	}
	else {
		config_buff_hdr->check_sum_adj = (uint) (256 - (new_sum - old_sum));
	}
	config_buff_hdr->check_sum_adj = config_buff_hdr->check_sum_adj & 0xff;

}

int parse_megahertz(char *s)
{
        int f = 1000000, n = atoi(s) * f;    

        if ((s = strchr(s, '.')) != 0)
                while (*++s)
                        n += (*s - '0') * (f /= 10);

        return n;
}



/*
 * flash reads the future contents of the  PROM from a binary file and
 * writes the data directly into the flash prom.  It is a little
 * dangerous since if some kind of error occurs the flash prom
 * can be corrupted.
 */

int
do_flash_cmd(int argc, char **argv)
{
    	char  		*buffer = (char *) TO_NODE(get_nasid(), FLASHBUF_BASE);
    	promhdr_t 	promhdr;
	promhdr_t       *ph;
	char            input_str[8];
	int             only_io;
	int             rtn_val;

	new_config_info = ((ip27config_t *) malloc(sizeof(ip27config_t)));

	/* flash parse sets various globals in printf below */
	if (!flash_parse(argc, argv, flash_which)) {
		rtn_val = 1;
		goto end_do_flash;
	}



	if (flash_debug) {
		printf("flash_parse: Module = %d, Slot = %d, widget = %d,\
nodepath = %d, nasid = %d\n",
                      modid, slotid, w, n, flashprom_nasid) ;
      	}

	if (log_erase_flag) {
		program_all_proms(KLTYPE_IP27, &promhdr, buffer);
		rtn_val = 0;
		goto end_do_flash;
	}

	if (!flash_debug) {

    		/* Read in the flashprom binary file if we're not erasing */

		if (!(erase_flag || log_erase_flag || count_flag)) {
    			printf("Reading PROM Image file %s ...\n", filename) ;
        		if (read_flashprom(filename, &promhdr, buffer) == -1) {
            			rtn_val = 0;
				goto end_do_flash;
			}
    		}
	}

	only_io = 0;
	if ((flash_which == IO_PROM) || (flash_which == -1)) {
		if (flash_which == IO_PROM)
			only_io = 1;
		flash_which = IO_PROM;
		if ((full_check_flag) && (only_io))
			printf("Ignoring -F since we're only flashing the IO Prom\n");
		if ((check_flag) && (only_io))
			printf("Ignoring -f since we're only flashing the IO Prom\n");

		/* easy case - don't need to fix configuration bits */
		/* Program the selected boards */

		if (boardlist) {
			program_io6(&promhdr, buffer);
		} else {
			program_all_proms(KLTYPE_BASEIO, &promhdr, buffer);
		}
		
		if (only_io) {
			rtn_val = 0;
			goto end_do_flash;
		}

	}
	
	/* if we're here that means we need a valid cpu image thus we
	   will need to obtain valid configuration bits 
	   if we're flashing all the proms we'll pick up the specific 
	   config info for each board in flash_all_nodes */

	flash_which = CPU_PROM;
	
	if (full_check_flag) {
		_get_all_config_vals(new_config_info);
	}
	else if (check_flag) {

		/*
		 * only frequencies and scache size
		 */
		_get_cpu_hub_freq(new_config_info);


		new_config_info->r10k_mode = get_scache_size(SN00 ?
					     1 << IP27C_R10000_SCS_SHFT :
					     4 << IP27C_R10000_SCS_SHFT );

		if (! _verify_config_info(new_config_info)) {
			printf("invalid configuration values\n");
			rtn_val = 1;
			goto end_do_flash;
		}
	}
	else ;

    	/* Now program the selected boards */

    	if (boardlist) {
        	program_ip27(&promhdr, buffer);
    	} else {
        	if (full_check_flag || check_flag) {
			printf("When using -f or -F it is recommended that proms are flashed individually.\n");
			printf("  Since a board has not been specified all proms will be flashed.\n");
			printf("  Do you wish to continue with flash y/n [n] ");
			gets(input_str);
			if (input_str[0] != 'y') {
				rtn_val = 0;
				goto end_do_flash;
			}
		}
		program_all_proms(KLTYPE_IP27, &promhdr, buffer);
    	}

    	rtn_val = 0;

      end_do_flash:
	free(new_config_info);
	return rtn_val;
}

/* returns 1 if there's enough info in the header to set the entire
   configuration header - else rerturn 0 */
int
_verify_config_info(ip27config_t *config_info)
{
	int i;

	/*
	 * check if this is a valid configuration, e.g. it shows
	 * up in the configuration tables.
	 */

	for (i=0; i<NUMB_IP27_CONFIGS; i++) {

		__uint32_t sz1,sz2;

		sz1 = ip27config_table[i].r10k_mode & IP27C_R10000_SCS_MASK;
		sz2 = config_info->r10k_mode        & IP27C_R10000_SCS_MASK;

		if ((sz1                           == sz2                   ) &&
		    (ip27config_table[i].mach_type == config_info->mach_type) &&
		    (ip27config_table[i].freq_cpu  == config_info->freq_cpu ) &&
		    (ip27config_table[i].freq_hub  == config_info->freq_hub ))
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
_get_cpu_hub_freq(ip27config_t *config_info)
{
	char    input_str[8];

	config_info->freq_cpu = (__uint64_t) IP27C_MHZ(300) ;
	printf("%-35s [%5d] : ","CPU frequency (MHZ)",
				config_info->freq_cpu / 1000000);
	gets(input_str);
	if (strlen(input_str) > 0)
		config_info->freq_cpu = (__uint64_t) parse_megahertz(input_str);

	config_info->freq_hub = (__uint64_t) IP27C_MHZ(100) ;
	printf("%-35s [%5d] : ","Hub frequency (MHZ)",
				config_info->freq_hub / 1000000);
	gets(input_str);
	if (strlen(input_str) > 0) 
		config_info->freq_hub = (__uint64_t) parse_megahertz(input_str);

	
	config_info->mach_type = (uint) SN00 ? 1 : 0;
	printf("%-35s [%5d] : ","Machine type (0)SN0 (1)SN00",
						config_info->mach_type);
	gets(input_str);
	if (strlen(input_str) > 0) 
		config_info->mach_type = (uint) atoi(input_str);

	config_info->check_sum_adj = (uint) 0;

	config_info->fprom_cyc =
		(config_info->mach_type == SN00_MACH_TYPE ? 15 : 8);
	config_info->fprom_wr =
		(config_info->mach_type == SN00_MACH_TYPE ? 4 : 1);
}


/*
 * returns the CPU mode bits setting for secondary
 * cache size.
 *
 * same format and options for IP27 and IO6
 */

char *cache_size_mb[8] = { "0.5","1","2","4","8","16","RSV","RSV" };

__uint32_t
get_scache_size(__uint32_t def_mode)
{
    __uint32_t result;
    char       buf[8];
    int        cache;

    cache = -1;

    while (cache == -1) {

        printf("%-35s [%5s] : ","Scache size in MB",
               cache_size_mb[((def_mode & IP27C_R10000_SCS_MASK) >>
                                          IP27C_R10000_SCS_SHFT)]);

        if (gets(buf) && strlen(buf)) {

            if (!strcmp(buf,"0.5"))
                cache = 0;

            switch(atoi(buf)) {
                case  1 : cache =  1 ; break;
                case  2 : cache =  2 ; break;
                case  4 : cache =  3 ; break;
                case  8 : cache =  4 ; break;
                case 16 : cache =  5 ; break;
                default :              break;
            }

            if (cache == -1)
                    printf("Invalid secondary cache size, try again\n");
        } else {
                return def_mode;
        }
    }

    result  = def_mode & (~IP27C_R10000_SCS_MASK);
    result |= (cache << IP27C_R10000_SCS_SHFT);

    return result;
}


#define MHZ(f)                  (f / 1000000)

/*
 * the define should move to /usr/include/sys/R10k.h
 */
#ifndef R10000_DDR_SHFT
#define	R10000_DDR_SHFT 23
#endif

void
_get_all_config_vals(ip27config_t *config_info)
{
	char       buf[8];
	__uint32_t cpu_mode,val,tap,onetap,SysClkDiv,SCClkDiv;
	int        is_r14k;

re_enter:

	config_info->r10k_mode = 0;

	/*
	 * default mode bits, there should be no need to change them
	 */
	cpu_mode =  5 << R10000_KSEG0CA_SHFT |	/* kseg0 cache algorithm    */
						/* 5 == cacheable coherent  */
						/*      exclusive on write  */
		    1 << R10000_SCBS_SHFT    |  /* secondary cache block sz */
						/* 1 == 32 words	    */
		    1 << R10000_ME_SHFT ;	/* Memory endianness        */
						/* 1 == big endian	    */	
	

	_get_cpu_hub_freq(config_info);
	
	is_r14k = 0;
	printf("%-35s [%5d] : ","CPU type (0)R10K/R12K (1)R14K",is_r14k);
	gets(buf);
	if (strlen(buf) && atoi(buf) == 1) 
		is_r14k = 1;

	cpu_mode |= get_scache_size(SN00 ? 1 << IP27C_R10000_SCS_SHFT :
                                           4 << IP27C_R10000_SCS_SHFT );

	if (is_r14k) {
		printf("%-35s [%5d] : ","DDR SRam",0);
		gets(buf);
		if (strlen(buf) && atoi(buf) == 1) 
			cpu_mode |= (1 << R10000_DDR_SHFT);
	}

	printf("%-35s [%5d] : ","Tandem Mode bit",0);
	gets(buf);
	if (strlen(buf) && atoi(buf) == 1) 
		cpu_mode |=  (1 << R10000_SCCE_SHFT);


	/*
	 * we have a pretty good idea how the divisor should look like
	 * given the hub and CPU frequencies, so we'll use this as
	 * the default
	 */
	val = ((MHZ(config_info->freq_cpu) * 2) / 
		MHZ(config_info->freq_hub)) - 1;
	printf("%-35s [%5d] : ","SysClk Divisor (PClk/SysClk ratio)",val);
	gets(buf);
	if (strlen(buf)) 
		val = atoi(buf);
	cpu_mode |= ((val << R10000_SCD_SHFT) & R10000_SCD_MASK);

	val = 2;	/* reasonable default */
	printf("%-35s [%5d] : ","SCClk  Divisor (PClk/SCClk ratio)", val);
	gets(buf);
	if (strlen(buf))
		val = atoi(buf);
	cpu_mode |= ((val << R10000_SCCD_SHFT) & R10000_SCCD_MASK);

	tap = 0xa;	/* reasonable default */
	printf("%-35s [  0x%x] : ","SCClk Tap", tap);
	gets(buf);
	if (strlen(buf)) 
		tap = strtoull(buf, NULL, 16);
	cpu_mode |= ((tap << R10000_SCCT_SHFT) & R10000_SCCT_MASK);

	val = 0x3;
	printf("%-35s [  0x%x] : ","Outstanding requests", val);
	gets(buf);
	if (strlen(buf))
		val = strtoull(buf, NULL, 16);
	cpu_mode |= ((val << R10000_PRM_SHFT) & R10000_PRM_MASK);

	config_info->r10k_mode = cpu_mode;

	config_info->freq_rtc = (__uint64_t) IP27C_KHZ(IP27_RTC_FREQ);
	printf("%-35s [ 1.25] : ","RTC frequency (MHZ)");
	if (gets(buf) && strlen(buf) > 0) 
		config_info->freq_rtc = (__uint64_t) parse_megahertz(buf);

	config_info->config_type = (uint) 0;
	printf("%-35s [%5d] : ","Configuration (0)Normal (1)12P4I",
				  config_info->config_type);
	gets(buf);
	if (strlen(buf) > 0) 
		config_info->config_type = (uint) atoi(buf);

    	printf("\n");

	/*
	 *	Verification
	 *	------------
	 */
    	printf("CPU    freq\t%3d MHz\n", MHZ(config_info->freq_cpu));
	printf("CPU    type\t%s\n"     , is_r14k ? "R14K":"R10K/R12k");
    	printf("Hub    freq\t%3d MHz\n", MHZ(config_info->freq_hub));

    	/*
     	 * Secondary cache information
     	 */
	SysClkDiv = (config_info->r10k_mode & R10000_SCD_MASK) 
						>> R10000_SCD_SHFT;
	SCClkDiv  = (config_info->r10k_mode & R10000_SCCD_MASK)
						>> R10000_SCCD_SHFT;

	/*
	 * if this is R14K running in DDR mode we need to twist 
	 * the SCClkDiv a bit in order to compute the correct
	 * scache speed. Also handle siliconn test modes in R14k
	 *
	 *      Code    R10K/R12K    R14K SDR     R14K DDR
	 *        0       rsv          rsv           rsv
	 *        1	   1            1             2
	 *        2        1.5          1.5           2
	 *        3        2            2	      2
	 *        4        2.5          2.5           2
	 *        5        3            3             3
	 *        6       rsv           6             6
	 *        7       rsv           4             4
 	 *
	 */
	if (is_r14k) {
		/*
		 * handle silicon test modes. Code 7 translates
		 * correctly to a div 4.0
		 */
		if (SCClkDiv == 6)
			SCClkDiv = 11;

		if (config_info->r10k_mode & (1 << R10000_DDR_SHFT)) {
			switch(SCClkDiv) {
			case 1 :
			case 2 :
			case 4 :
				SCClkDiv = 3;
				break;
			default:
				break;
			}
		}

		printf("Scache mode\t%s\n",
			config_info->r10k_mode & (1 << R10000_DDR_SHFT) ?
			"DDR":"SDR");
	}

	if (is_r14k && (config_info->r10k_mode & (1 << R10000_DDR_SHFT))) {
    		printf("Scache freq\tread %3d MHz / write %3d Mhz\n",
         		(MHZ(config_info->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1)) * 2,
         		MHZ(config_info->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1));
	} else {
    		printf("Scache freq\t%3d MHz\n",
         		MHZ(config_info->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1));
	}

    	printf("Scache Tap\t[0x%x]  ",tap);

    	onetap = (int) 1E6 / MHZ(config_info->freq_cpu) / 12;

    	switch (tap) {
    	case 0x0 : printf("same phase as internal clock"); break;
    	case 0x1 : printf("%d ps",onetap * 1);             break;
    	case 0x2 : printf("%d ps",onetap * 2);             break;
    	case 0x3 : printf("%d ps",onetap * 3);             break;
    	case 0x4 : printf("%d ps",onetap * 4);             break;
    	case 0x5 : printf("%d ps",onetap * 5);             break;
    	case 0x6 : printf("UNDEFINED");                    break;
    	case 0x7 : printf("UNDEFINED");                    break;
    	case 0x8 : printf("%d ps",onetap * 6);             break;
    	case 0x9 : printf("%d ps",onetap * 7);             break;
    	case 0xa : printf("%d ps",onetap * 8);             break;
    	case 0xb : printf("%d ps",onetap * 9);             break;
    	case 0xc : printf("%d ps",onetap * 10);            break;
    	case 0xd : printf("%d ps",onetap * 11);            break;
    	case 0xe : printf("UNDEFINED");                    break;
    	case 0xf : printf("UNDEFINED");                    break;
    	default  : printf("INVAL \"%d\"",tap);             break;
    	}
    	printf(", tap inc %d ps\n",onetap);


    	printf("Machine type\t%s\n", 
		config_info->mach_type == SN00_MACH_TYPE ? "SN00" : "SN0");
    	printf("Cache size\t%s MB\n",
         	cache_size_mb[((config_info->r10k_mode & 0x00070000) 
					    >> IP27C_R10000_SCS_SHFT)]);
    	printf("Tandem mode\t%d\n",
         	(config_info->r10k_mode & R10000_SCCE_MASK) 
					    >> R10000_SCCE_SHFT);
    	printf("R10K mode\t0x%x\n", config_info->r10k_mode);

        printf("RTC freq (KHZ)\t%d\n",config_info->freq_rtc / 1000);
	printf("Configuration\t%s",config_info->config_type == 0 ?
				   "Normal" : "12P4I");				   
	printf("\n\nProceed ? [n] ");
	if (!gets(buf) || ((buf[0] != 'y') && (buf[0] != 'Y')))
		goto re_enter;

	config_info->time_const    = (uint) CONFIG_TIME_CONST;
	config_info->magic 	   = (__uint64_t) CONFIG_MAGIC;
	config_info->ecc_enable    = (uint) CONFIG_ECC_ENABLE;
	config_info->check_sum_adj = (uint) 0;
}

int
flash_parse(int argc, char **argv, int type)
{
	int i ;

	/* Init all globals */

	bzero(filename, FILENAME_BUF_SIZE);
	log_erase_flag = 0;
	check_flag = 0;
	full_check_flag = 0;
	erase_flag = 0 ;
	count_flag = 0 ;
	foundfile = 0 ;
	verbose	  = 0 ;
	boardlist = 0 ;
	flash_debug = 0 ;
	nasid_option = 0 ;
	modid_option = 0 ;
	confirm_yes = 0 ;
	slotname[0] = 0 ;

	modid	 = slotid = -1 ;
        widgetid = -1 ;

	flashprom_nasid = INVALID_NASID;

	command = "Flashing";

	/* Check arguments */

	if (argc < 2)			/* too less */
		return 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'c':
				        if (flash_which != -1) {
						printf("Error: choice of proms already specified\n");
						return 0;
					}
					flash_which = CPU_PROM ;
				break ;

				case 'C':
					count_flag = 1 ;
					command = "Counting";
				break ;

				case 'e':
					erase_flag = 1 ;
					command = "Erasing";
				break ;

				case 'f':
					check_flag = 1 ;
				break ;

				case 'F':
					full_check_flag = 1 ;
				break ;

				case 'i':
				        if (flash_which != -1) {
						printf("Error: choice of proms already specified\n");
						return 0;
					}
					flash_which = IO_PROM ;
				break ;

				case 'l':
					log_erase_flag = 1 ;
				break ;

				case 'm':
				   if (++i == argc)
					return 0 ;

				   if (!(isdigit(argv[i][0]))) {
					printf(
					"Error: Module id must be a number\n");
					return 0 ;
				   }

				   modid = atoi(argv[i]) ;
				break ;

				case 'n':
				        if (flash_which != -1) {
						printf("Error: choice of proms already specified\n");
						return 0;
					}
					flash_which = CPU_PROM ;
				break ;

				case 'N':

				/* For compatibility with the 'flash'
				   command in the IP27prom POD mode
				   we can flash a cpu prom with a nasid */

				   if (++i == argc)
					return 0 ;

				   if (!(isdigit(argv[i][0]))) {
					printf(
					"Error: Nasid must be a number\n");
					return 0 ;
				   }

				   flashprom_nasid = atoi(argv[i]) ;
				   nasid_option = 1 ;
				break ;

				case 's':
				    /* If we are an xbox system then we
				     * can flash the io prom by specifying
				     * module & io slot number.
				     */
				   if (SN00 &&
				       xbox_nasid_get() == INVALID_NASID)
					if (++i == argc)
					    return 0 ;
					else
					    break ;

				   if (++i == argc)
					return 0 ;

				   if (flash_which == IO_PROM) {
				       if (strncmp(argv[i], "io", 2)) {
					    printf(
				"Error: Slotname must begin with \'io\'\n") ;
					    return 0 ;
				   	}
				   	slotid = atoi(&argv[i][2]) ;
					/* If we are an xbox and are trying
					 * to specify an io slot other than
					 * 7 then print an error.
					 */
					if (SN00  && slotid != 7) {
					    printf(
					   "Invalid XBOX IO PROM slot io%d\n",
					   slotid);
					    return(0);
					}

				   }
				   else
				   if (flash_which == CPU_PROM) {
				       if (argv[i][0] != 'n') {
					    printf(
				"Error: Slotname must begin with \'n\'\n") ;
					    return 0 ;
				       }
				   	slotid = atoi(&argv[i][1]) ;
				   }
				   else {
			 printf("Please specify the type of PROM to flash first.\n") ;
					return 0 ;
				    }
				    strcpy(slotname, argv[i]) ;

				break ;

				case 'v':
					verbose = 1 ;
				break ;

				case 'd':
					flash_debug = 1 ;
				break ;

				case 'y':
					confirm_yes = 1 ;
				break ;

				default:
parse_err:
					printf(
					"Error: Unrecognized flag: '%s'\n",
						argv[i]);
					return 0 ;
			}
		} else {
			if (!foundfile) {
				strcpy(filename, argv[i]) ;
				i ++ ;
				foundfile = 1 ;
			} else {
				printf(
				"Error: already specified filename '%s'.\n",
				filename);
				return 0 ;
			}
			break ;
		}
	}


	if (count_flag || log_erase_flag) {
		if (flash_which != CPU_PROM) {
			printf("error you must specify the cpu prom (-c|-n) when requesting to erase the log or count the number of flashes\n");
			return 0;
		}
	}

	if (i != argc)
		printf("Extra argument at end of list ignored\n") ;

	if ((modid == -1) && (slotid != -1)) {
		/* module not specified but slot specified */
		printf("Module id required if slotid is specified.\n") ;
		return 0 ;
	}

	/* Complain if no file name was specified */

	if (!foundfile) {
		if (!(erase_flag || log_erase_flag || count_flag)) {
			printf("Error: no filename specified\n");
			return 0;
		}
	}

	/* Check for proper range of values for module id and slot id */

	if ((slotid > MAX_IP27_SLOT) && (flash_which == CPU_PROM)) {
		printf("Error: Invalid NODE slot number %d\n", slotid) ;
		return 0 ;
	}

	if ((slotid > MAX_IO6_SLOT) && (flash_which == IO_PROM)) {
		printf("Error: Invalid IO slot number %d\n", slotid) ;
		return 0 ;
	}


	if ((flashprom_nasid == INVALID_NASID) && (modid == -1) && (slotid == -1)) {
		printf("%s all PROMs in the system\n", command) ;
	}
	else if ((modid != -1) && (slotid == -1)) {
		/* Flash all proms in module */
		modid_option = 1 ;
	} else { 
		/* modid and slotid are non-negative or nasid is specified on cmd line */

		/* get nasid from promgraph */

		if (flashprom_nasid == INVALID_NASID)
			flashprom_nasid = flash_getnasid(type) ;

		if (flashprom_nasid == INVALID_NASID)
			return 0 ;

		boardlist = 1 ;

		if (nasid_option)
			printf("%s PROM in Node ID %d\n",
				command, flashprom_nasid) ;
		else
			printf("%s PROM in Module %d Slot %d\n",
				command, modid, slotid) ;
	}

	return 1 ;

}

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

hub_accessible(nasid_t nasid)
{
    __uint64_t	       reg, tst;
    int		r = 0;

    reg = (__uint64_t) REMOTE_HUB(nasid, MD_PERF_CNT5);

    tst = 0x5555ULL;
    SD(reg, tst);
    if (LD(reg) != tst)
	goto done;

    tst = 0xaaaaULL;
    SD(reg, tst);
    if (LD(reg) != tst)
	goto done;

    r = 1;

 done:
    return r;
}

#define PRINT_ADDRESS	      0x8000
#define PRINT_DOT	      0x1000

int
fprom_write_buffer(fprom_t *f, char *buffer, int count)
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
				printf("\n") ;
			first = 0 ;
			printf("> %5lx/%05lx ", done, count) ;
			if (n == 0)
				break ;
			pra += PRINT_ADDRESS ;
		}

		if ((r = fprom_write(f, dst_off, src, n)) < 0) {
			printf("> Error programming PROM: %s\n",
					fprom_errmsg(r));
			printf("base = %x, off = %x, src = %x\n",
				f->base, dst_off, src) ;
			return 0 ;
		}

		if (done >= prd) {
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
 * void dump_promhdr
 *      Dump the information in the promhdr_t structure.
 */

void
dump_promhdr(promhdr_t *ph)
{
    int		i;

    printf("PROM Header contains:\n");
    printf("  Magic:    0x%llx\n", ph->magic);
    printf("  Version:  %lld.%lld\n", ph->version, ph->revision);
    printf("  Length:   0x%llx\n", ph->length);
    printf("  Segments: %lld\n", ph->numsegs);

    if (ph->numsegs > 6) {
	printf("Corrupt segment count in segldr header (%d)\n", ph->numsegs);
	return ;
    }

    for (i = 0; i < ph->numsegs; i++) {
	promseg_t *seg = &ph->segs[i];

	printf("Segment %d:\n", i);
	printf("  Name:        %s\n", seg->name);
	printf("  Flags:       0x%lx\n", seg->flags);
	printf("  Offset:      0x%lx\n", seg->offset);
	printf("  Entry:       0x%lx\n", seg->entry);
	printf("  Ld Addr:     0x%lx\n", seg->loadaddr);
	printf("  True Length: 0x%lx\n", seg->length);
	printf("  True sum:    0x%lx\n", seg->sum);

	if (seg->flags & SFLAG_COMPMASK) {
	    printf("  Cmprsd len:  0x%lx\n", seg->length_c);
	    printf("  Cmprsd sum:  0x%lx\n", seg->sum_c);
	}
    }

}

/*
 * read_flashprom()
 *	Reads the flash prom binary data out of the specified
 *	file, checks its checksum, and then converts it into a
 *	form suitable for burning.

	XXX Check the header and make sure that we are not flashing
	    a IO6prom image into a IP27prom and vice versa.
 */

int
read_flashprom(char *file_name, promhdr_t *ph, char *buffer)
{
    ULONG fd, fdwr, cnt, wrcnt;
    long err;
    int numsegs;
    char *curbufptr, *start ;
    int numbytes, offset;
    __uint64_t cksum, cksum2 ;
    unsigned int i, j;
    uint	length = 0 ;

	if (*filename == 0)
		return -1 ;

    /* Open the file */
    if ((err = Open(file_name, OpenReadOnly, &fd)) != 0) {
	printf("Cannot open flash prom source file: ");
	perror(err, file_name);
	return -1;
    }

    /* Read in the header and make sure it looks kosher before we
     * fry the flash prom.
     */
    if (err = Read(fd, buffer, PROM_DATA_OFFSET, &cnt)) {
	printf("Could not read header structure from file.\n");
	Close(fd);
	return -1;
    }

    if (cnt != PROM_DATA_OFFSET) {
	printf("Unable to read prom header.\n");
	Close(fd);
	return -1;
    }

    bcopy(buffer, (char *)ph, sizeof(promhdr_t)) ;

    if (ph->magic != PROM_MAGIC) {
	printf("Invalid PROM magic number: 0x%x\n", ph->magic);
	Close(fd);
	return -1;
    }

    if (verbose)
	dump_promhdr(ph) ;

    if (flash_which == CPU_PROM) {
	if (strcmp(ph->segs[0].name, "ip27prom")) {
        	printf("Image file contains a %s segment. Try again with correct image.\n", 
				ph->segs[0].name);
        	Close(fd);
        	return -1;
	}
    }

    if (flash_which == IO_PROM) {
        if (strcmp(ph->segs[0].name, "io6prom")) {
                printf("Image file contains a %s segment. Try again with correct image.\n", 
                                ph->segs[0].name);
                Close(fd);
                return -1;
        }
    }   

    cksum2 = cksum = 0 ;

    length = ph->segs[0].offset ;
    numsegs = ph->numsegs ;
    for (i = 0; i < numsegs; i++) {
	if (ph->segs[i].flags&SFLAG_COMPMASK) {
		length += ph->segs[i].length_c;
		cksum2 += ph->segs[i].sum_c ;
	} else {
		length += ph->segs[i].length ;
		cksum2 += ph->segs[i].sum ;
	}
	length = (length + 7) & ~7;
    }

    if (length != ph->length) {
	printf("Total length of prom data does not match header.\n") ;
	printf("calculated length = %d, prom header length = %d\n",
		length, ph->length) ;
	return -1 ;
    }

    if (length > FLASHPROM_SIZE) {
	printf("Flash data is too big and won't fit in Flash PROM.\n");
	Close(fd);
	return -1;
    }

    /* Read in the Flash EPROM contents */

    length -= ph->segs[0].offset ;

    curbufptr = buffer + PROM_DATA_OFFSET ;
    start = curbufptr;
    numbytes = length ;

    if (verbose)
	printf("  Reading the entire prom data (%d bytes) ...", numbytes);

    while (numbytes) {
	unsigned int count = ((numbytes < IO6_FPROM_CHUNK_SIZE) ?
					numbytes : IO6_FPROM_CHUNK_SIZE);

	if (err = Read(fd, (CHAR*) curbufptr, count, &cnt)) {
		printf("Error occurred while reading prom file.\n");
		Close(fd);
		return -1;
	}

	if (cnt == 0) {
		printf(" \nUnexpected EOF");
		printf("after reading %d bytes.\n", numbytes) ;
		Close(fd);
		return -1;
	}
	curbufptr += cnt;
	numbytes  -= cnt;
    }
    putchar('\n');

    /* Checksum the buffer and make sure it seems okay */

    numbytes = length ;

    cksum = memsum(start, numbytes) ;

    if (verbose)
	printf("   Checksum results (%x %x).\n", cksum, cksum2);

    if (cksum != cksum2) {
	    printf("   Checksum error (%x %x).\n", cksum, cksum2);
	    printf("   Aborting command.\n") ;
	    Close(fd);
	    return -1;
    }

    Close(fd);
    return 0;
}

static int
check_abort()
{

	return 1 ;

}

void
prom_check(fprom_t *f)
{
	promhdr_t phdr ;

	if (fprom_read(f, 0, (char *)&phdr, sizeof(promhdr_t))) {
		printf("Error reading prom header.\n") ;
		return ;
	}

	dump_promhdr(&phdr) ;
}


nasid_t
flash_getnasid(int type)
{
    	lboard_t 	lb, *rlb ;
        unchar 		brd_class ;
        unchar 		nsclass = 0 ;

	/*
	 * Calculate widget id. This is used later to get
	 * bridge base.
	 */
        w = slot_to_widget(slotid-1) ; 	  /* get the composite widget no */
        w &= SLOTNUM_SLOT_MASK ;          /* widget number */
	if ((SN00) || (CONFIG_12P4I))
	    /* 
	     * In olden days, on SN00, slots used to have
	     * dummy numbers like 1 and 11 while in reality
	     * the widget numbers were 0. Hub directly 
	     * connected to the bridge.
	     */
	    if ((slotid == 1) || (slotid == 11))
		w = 0 ;
	widgetid = w ;

    	/* fill a dummy lboard struct and find a match */

	brd_class = (flash_which == CPU_PROM) ? KLCLASS_CPU : KLCLASS_IO ;
	/* 
	 * This is needed as KEGO's have a different slot class for
 	 *  CPU boards.
	 */
        visit_lboard(get_my_node_slot_class, NULL, (void *)&nsclass) ;

	/* slotid can be anything for SN00. */

        lb.brd_type   = brd_class | slotid ;
        lb.brd_module = modid ;
        lb.brd_slot   = (brd_class == KLCLASS_IO) ?
                        (SLOTNUM_XTALK_CLASS|slotid) :
                        (nsclass|slotid) ;

    	rlb = get_match_lb(&lb, 0) ;
    	if (rlb) {
        	flashprom_nasid = rlb->brd_nasid ;
		return(rlb->brd_nasid) ;
    	}

    	return INVALID_NASID ;
}

/*
 * Scans all modules and calls program_all_nodes
 */

int
program_all_proms(int which, promhdr_t *promhdr, char *buffer)
{
	graph_error_t	graph_err, graph_err1 ;
	char		tmp_buf[64] ;
	char		edge_name[64] ;
	vertex_hdl_t	mod_vhdl, nextmod_vhdl ;
	graph_edge_place_t	eplace = GRAPH_EDGE_PLACE_NONE ;

#ifdef SN_PDI
	strcpy(tmp_buf, "/hw/module/") ;
	graph_err = hwgraph_path_lookup(snpdi_rvhdl, tmp_buf,
					&mod_vhdl, &remain) ;
#else
	strcpy(tmp_buf, "/module/") ;
	graph_err = hwgraph_path_lookup(hw_vertex_hdl, tmp_buf,
					&mod_vhdl, &remain) ;
#endif
	if (graph_err != GRAPH_SUCCESS) {
		printf("Error: No Modules in HWGRAPH.\n") ;
		return 0 ;
	}

	do {
#ifdef SN_PDI
		graph_err = graph_edge_get_next(pg_hdl,
						mod_vhdl,
						edge_name,
						&nextmod_vhdl, &eplace) ;
#else
		graph_err = graph_edge_get_next(prom_graph_hdl,
						mod_vhdl,
						edge_name,
						&nextmod_vhdl, &eplace) ;
#endif
		if (graph_err != GRAPH_SUCCESS) {
			break ;
		}

		if (modid_option) {
			if (atoi(edge_name) != modid)
				continue ;
		}

		if (program_all_nodes(nextmod_vhdl, edge_name,
				  promhdr, buffer, which) == FLASH_QUIT)
			return FLASH_QUIT ;

	} while (graph_err == GRAPH_SUCCESS) ;

	return 1 ;
}

int
program_all_nodes(vertex_hdl_t mod_vhdl, char *mod_name,
		  promhdr_t *promhdr,
		  char *buffer, int type)
{
	graph_error_t	graph_err ;
	char		tmp_buf[64] ;
	vertex_hdl_t	node_vhdl, next_vhdl, slot_vhdl ;
	graph_edge_place_t	eplace = GRAPH_EDGE_PLACE_NONE ;
	char		slot_name[32] ;
	klinfo_t	*kliptr ;
	lboard_t 	*lb ;

	strcpy(tmp_buf, "slot") ;
	graph_err = hwgraph_path_lookup(mod_vhdl, tmp_buf,
					&slot_vhdl, &remain) ;
	if (graph_err != GRAPH_SUCCESS) {
		printf("Error: Edge Slot Module %d not found\n", modid) ;
		return 0 ;
	}

	do {
#ifdef SN_PDI
		graph_err = graph_edge_get_next(pg_hdl,
						slot_vhdl,
						slot_name,
						&next_vhdl, &eplace) ;
#else
		graph_err = graph_edge_get_next(prom_graph_hdl,
						slot_vhdl,
						slot_name,
						&next_vhdl, &eplace) ;
#endif
		if (graph_err != GRAPH_SUCCESS) {
			break ;
		}

		lb = (lboard_t *)pg_get_lbl(next_vhdl, INFO_LBL_BRD_INFO) ;

		if (lb) {
    		    if (flash_which == CPU_PROM) {
			if (KLCLASS(lb->brd_type) != KLCLASS_CPU)
			    continue ;
		    }
    		    else if (flash_which == IO_PROM) {
			     if (KLCLASS(lb->brd_type) != KLCLASS_IO)
			    	continue ;
		    }
		    else 
			continue ;
		} else
		    continue ;

		flashprom_nasid = lb->brd_nasid ;

		strcpy(slotname, slot_name) ; /* for msg printing */

		if (flash_which == CPU_PROM) {
			printf("%s CPUPROM in module %s slot %s\n", command,
				mod_name, slot_name) ;

			if (program_ip27(promhdr, buffer) == 2)
				return 2 ;
		} else if (flash_which == IO_PROM) {
			if (program_all_widgets(lb, promhdr, buffer) 
						== FLASH_QUIT)
				return FLASH_QUIT ;
		} else {
			printf("program_all_nodes: Unknown type of board %d\n",
				type) ;
			break ;
		}
	} while (1) ;

	return 1 ;
}

int
program_all_widgets(lboard_t *lb, 
		    promhdr_t *promhdr, char *buffer)
{
    int is_xbox = (xbox_nasid_get() != INVALID_NASID);
    /* If we are a speedo without an xbox then
     * there is no need to program the io prom.
     */
    if (SN00 && !is_xbox)
	return 1 ;

    slotid = SLOTNUM_GETSLOT(lb->brd_slot) ;
    w = slot_to_widget(slotid-1) ;
    w &= SLOTNUM_SLOT_MASK ;

    if ((SN00) || (CONFIG_12P4I))
        if ((slotid == 1) || (slotid == 11))
            w = 0 ;
    widgetid = w ;
    /* If we are trying to flash the io prom on the xbox
     * flash then it can be only at ONE place (widget 8)
     */
    if (SN00 && is_xbox && (widgetid != XBOX_FLASHPROM_WID))
	return(1);

    if (!is_xbox && (lb->brd_type != KLTYPE_BASEIO))
	return 1 ;

    get_slotname(lb->brd_slot, slotname) ;
    printf("%s IO PROM on module %d slot %s\n",
	command, lb->brd_module, slotname);
    if (program_io6(promhdr, buffer)==FLASH_QUIT)
	return FLASH_QUIT ;
    return 1 ;
}

char
confirm(char *type, char *action)
{
        char        buf[32], ans = 0 ;
	char        *s;

	s = (nasid_option) ? "corresponding to the given nasid" : slotname;

        while ((ans!='y') && (ans!='n') && (ans!='q')) {
		printf("\nOK to %s the %s PROM in slot %s ? (y|n|q)[y]:",
		       action, type, s) ;
		gets(buf) ;
                if (buf[0] == '\0')
                        ans = 'y' ;
                else
                        ans = buf[0] ;
        }
	return ans ;
}
