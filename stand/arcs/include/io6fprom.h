#include <sys/SN/promhdr.h>
#include <sys/SN/fprom.h>

#define MEG (1024 * 1024)
#define FILENAME_BUF_SIZE       128

typedef __uint64_t        i64;

/*
 * XXX put this in kldir.h  picture
 */
#if defined(FLASHBUF_BASE)
#undef FLASHBUF_BASE
#endif
#define FLASHBUF_BASE           PHYS_TO_K0(0x1300000)
#define FLASHPROM_SIZE		MEG
#define IO6_FPROM_CHUNK_SIZE (16 * 1024) /* Write size, Not sector size. */
#define IP27FLASH_ALIGNMENT 8
#define IO6FLASH_ALIGNMENT 2
#define CPU_PROM   0
#define IO_PROM    1
#define FLASH_NO   0
#define FLASH_YES  1
#define FLASH_QUIT 2

nasid_t 	flash_getnasid(int) ;
__uint64_t 	memsum(void *, size_t) ;
int 		hub_accessible(nasid_t) ;
void 		dump_promhdr(promhdr_t *) ;
int 		read_flashprom(char*, promhdr_t*, char*);
int 		fprom_write_buffer(fprom_t *, char *, int) ;
int 		flash_parse(int , char **, int) ;
void 		prom_check(fprom_t *) ;

char    confirm(char *, char *) ;
int     do_flash_cmd(int, char **) ;

int  	program_all_proms(int, promhdr_t *, char *);
int   	program_all_nodes(vertex_hdl_t , char *, promhdr_t *, char *, int) ;
int  	program_all_widgets(lboard_t *, promhdr_t *, char *) ;
int  	program_io6_node(promhdr_t *, char *) ;


int 	program_ip27(promhdr_t *, char *);
int 	program_io6(promhdr_t *, char *) ;


