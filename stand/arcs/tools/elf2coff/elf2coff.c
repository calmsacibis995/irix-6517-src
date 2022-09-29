/*
 * elf2coff
 * Usage: elf2coff elfExecutableInputFileName CoffExecutableOutputFileName
 * converts elf executable into coff executable
 * CAVEATS:	-code must be linked at lower address than data
 *		-data should be adjacent to code, or else elf2coff will extend
 *			the text segment in size and pad it with zeroes until
 *			the code and data are contiguous
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <arcs/io.h>
#include <arcs/large.h>
#include <string.h>
#include <setjmp.h>

#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <sym.h>
#include <arcs/dload.h>

extern __psint_t malloc(int);

int Verbose = 0;
int Debug = 0;

int e2c_verbose = 0;

LONG Close(ULONG foo){ return 0; }
mem_contains(){return 1;}
void dmabuf_free(){}
int range_check(){return 0;}
void flush_cache(){}
MEMORYDESCRIPTOR *md_alloc(unsigned long a, unsigned long b, MEMORYTYPE c)
{
MEMORYDESCRIPTOR m;
	return &m;
}
LONG md_dealloc(unsigned long a, unsigned long b) { return 0;}

/* this is dead code, referenced by the elf loader, but never executed */
void (*dload(int a, void * b, void * c, struct execinfo * d, int *e))()
{
	fprintf(stderr, "dload\n"); exit (-1);
	return 0;
}
void expand(){fprintf(stderr, "expand\n"); exit (-1); }
void _argvize(){fprintf(stderr, "_argvize\n"); exit (-1); }
void mem_getblock(){fprintf(stderr, "mem_getblock\n"); exit (-1); }
void init_str(){fprintf(stderr, "init_str\n"); exit (-1); }
void new_str1(){fprintf(stderr, "new_str1\n"); exit (-1); }
void find_str(){fprintf(stderr, "find_str\n"); exit (-1); }
void new_str2(){fprintf(stderr, "new_str2\n"); exit (-1); }
void nuxi_s(){fprintf(stderr, "nuxi_s\n"); exit (-1); }
void nuxi_l(){fprintf(stderr, "nuxi_l\n"); exit (-1); }
void makepath(){fprintf(stderr, "makepath\n"); exit (-1); }
void set_str(){fprintf(stderr, "set_str\n"); exit (-1); }
void _scandevs(){fprintf(stderr, "_scandevs\n"); exit (-1); }
void rbsetbs(){fprintf(stderr, "rbsetbs\n"); exit (-1); }
void client_start(){fprintf(stderr, "client_start\n"); exit (-1); }
void close_noncons(){fprintf(stderr, "close_noncons\n"); exit (-1); }
void mem_list(){}
/* end of dead code; if we had a linker which stripped out unreferenced
 * functions, we wouldn't need this dead code
 */

long
Open(CHAR *a, OPENMODE mode, ulong *ret)
{
	int err = open(a, O_RDONLY);
	if(err == -1) {
		fprintf(stderr, "couldn't open %s for reading\n", a);
		return EBADF;
	}
	else {
		*ret = err;
	}
	return 0;
}

LONG
Seek(unsigned long fd, LARGE *off, enum seekmode how_seek)
{
off_t offset = off->lo, new_off;
	new_off = lseek(fd, offset, (how_seek == SeekAbsolute) ? SEEK_SET : SEEK_CUR);
	return (new_off != -1) ? ESUCCESS : EBADF;
}

char *
dmabuf_malloc(int size)
{
char *p = (char *)malloc(size);
	if(p == 0) {
		fprintf(stderr, "malloc(%d): ran out of memory\n");
		exit(-1);
	}
	return p;
}

static char *outfile;

static
int
do_open(char *s)
{
	int ret = open(s, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	if(ret == -1) {
		fprintf(stderr, "Couldn't open %s for writing.\n", s);
		perror(0);
		exit(-1);
	}
	return ret;
}

static
void
write_out(int fd, char *buf, int cnt)
{
	int ret;
	do {
		ret = write(fd, buf, cnt);
		cnt -= ret;
	} while(cnt && ret != -1);
	if(cnt != 0) {
		fprintf(stderr, "Error writing to %s\n", outfile);
		perror(0);
	}

}


void
dump_output_file(ULONG entry, ULONG text_start, ULONG tsize,
		ULONG data_start, ULONG dsize,
		ULONG bss_start, ULONG bsize, char *coff_blk[2])
{
	struct execinfo e, *ei = &e;
	int fd;
	struct scnhdr scn;
	static struct scnhdr blank_scn;
	char *pzero = 0;
	int pad = 0;

	ei->fh.f_magic = MIPSEBMAGIC;
	ei->fh.f_nscns = 3;
	ei->fh.f_nsyms = 0;
	ei->fh.f_symptr = 0;
	ei->fh.f_opthdr = sizeof(struct aouthdr);
	ei->ah.magic = OMAGIC;
	ei->ah.entry = entry;

	ei->ah.text_start = text_start | 0xa0000000;
	if (e2c_verbose)
		printf("text_start is %x, size is %x\n", ei->ah.text_start, tsize);
	ei->ah.tsize = tsize;
#define  ZBUF_AMT 4096
	if(dsize == 0) {
		if(tsize <= ZBUF_AMT) {
			fprintf(stderr, "Code+Data section in elf program too small to make a coff file\n");
			exit(1);
		}
		dsize = tsize - ZBUF_AMT;
		ei->ah.tsize = tsize = ZBUF_AMT;
		ei->ah.data_start = ei->ah.text_start + ZBUF_AMT;
		coff_blk[1] = coff_blk[0] + ZBUF_AMT;
	}
	else {
		ei->ah.data_start = data_start | 0xa0000000;
		if((pad = ei->ah.data_start - ei->ah.text_start - tsize) > 0 ) {
			if (e2c_verbose)
				printf("padding text by 0x%x\n", pad);
			ei->ah.tsize += pad;
		}
		else {
			/* no pad if data comes right after code or before code*/
			pad = 0;
		}
	}

	ei->ah.dsize = dsize;
	ei->ah.bss_start = bss_start | 0xa0000000;
	ei->ah.bsize = bsize;

	if (e2c_verbose) {
		printf("text_start is %x\n", ei->ah.text_start);
		printf("text_size is %x\n", ei->ah.tsize);
		printf("data_start is %x\n", ei->ah.data_start);
		printf("data_size is %x\n", dsize);
		printf("bss_start is %x\n", ei->ah.bss_start);
		printf("bss_size is %x\n", bsize);
	}

#undef KDM_TO_PHYS
#define KDM_TO_PHYS(x)		((x) & 0x1fffffff)
	if ((KDM_TO_PHYS(ei->ah.text_start) + ei->ah.tsize) >
	    KDM_TO_PHYS(ei->ah.data_start)) {
		fprintf(stderr,"Error: elf2coff: text/data overlap!\n");
		exit(-1);
	}
	if ((KDM_TO_PHYS(ei->ah.data_start) + dsize) >
	    KDM_TO_PHYS(ei->ah.bss_start)) {
		fprintf(stderr,"Error: elf2coff: bss/data overlap!\n");
		exit(-1);
	}

	fd = do_open(outfile);
	write_out(fd, (char *)&ei->fh, sizeof(ei->fh));
	if(pad) {
		pzero = (char *)malloc(pad);
		if(pzero == 0) {
			printf("malloc: unable to allocate 0x%x bytes\n", pad);
			exit(-1);
		}
	}
	write_out(fd, (char *)&ei->ah, sizeof(ei->ah));

	/* blank out all unused fields */
	scn = blank_scn;

	scn.s_scnptr = sizeof(ei->fh) + sizeof(ei->ah) + 3 * sizeof(scn);
	scn.s_paddr = scn.s_vaddr = ei->ah.text_start;
	strcpy(scn.s_name, ".text");
	scn.s_flags = STYP_TEXT;
	scn.s_size = ei->ah.tsize;
	write_out(fd, (char *)&scn, sizeof(scn));

	scn.s_scnptr += tsize + pad;
	scn.s_paddr = scn.s_vaddr = ei->ah.data_start;
	strcpy(scn.s_name, ".data");
	scn.s_size = ei->ah.dsize;
	write_out(fd, (char *)&scn, sizeof(scn));

	scn.s_scnptr += dsize;
	scn.s_paddr = scn.s_vaddr = ei->ah.bss_start;
	strcpy(scn.s_name, ".bss");
	scn.s_flags = STYP_BSS;
	scn.s_size = ei->ah.bsize;
	write_out(fd, (char *)&scn, sizeof(scn));

	write_out(fd, coff_blk[0], tsize);
	if(pad) {
		write_out(fd, pzero, pad);
	}
	write_out(fd, coff_blk[1], dsize);
	/* don't bother to write out the bss segment */
}

LONG
Read(unsigned long fildes, void *bx, unsigned long nbyte, unsigned long *cnt_out)
{
	char *buf = (char *)bx;
	int cnt;

	cnt = read((int)fildes, buf, nbyte);
	*cnt_out = cnt;
	return (cnt == nbyte) ? 0 : -1;

}

extern void loadbin(char *, unsigned long *, unsigned long);

int
main(int argc, char *argv[])
{
	static char *Usage = "Usage: elf2coff [-v] inputfile outputfile\n";
	unsigned long pc_junk;
	char *src;

	if (argc == 3) {
		outfile = argv[2];
		src = argv[1];
	}
	else  if (argc == 4 && !strcmp(argv[1],"-v")) {
		e2c_verbose = 1;
		outfile = argv[3];
		src = argv[2];
	}
	else {
		fprintf(stderr, Usage);
		exit(-1);
	}

	loadbin(src, &pc_junk, 0);

	return 0;
}
