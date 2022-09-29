#ident	"lib/libsk/cmd/play_cmd.c:  $Revision: 1.7 $"

#include <arcs/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <arcs/errno.h>

#include <libsc.h>

void play_hello_tune(int);
void wait_hello_tune(void);

#define READ_SIZE	(16 * 1024) 

int tune_file_size;
int *tune_buf;

static int
get_file(char *path)
{
    char *bufp;
    ULONG fd, total, cnt, max;
    LONG err;
    unsigned short flash_cksum_r(char *cp, int len, unsigned short sum);
    MEMORYDESCRIPTOR *mem_getblock(void);
    void mem_list(void);
    MEMORYDESCRIPTOR *m;

    if (!(m = mem_getblock())) {
	printf("No free memory descriptors");
	mem_list();
	return ENOMEM;
    }
    max = arcs_ptob(m->PageCount) - READ_SIZE;

    bufp = (char *)PHYS_TO_K0(arcs_ptob(m->BasePage));
    tune_buf = (int *)bufp;

    err = Open(path, OpenReadOnly, &fd);
    if (err != ESUCCESS) {
	printf("Open error %ld for %s\n", err, path);
	return err;
    }

    cnt = total = 0;
    do {
	err = Read(fd, (void *)bufp, READ_SIZE, &cnt);
	if (err != ESUCCESS) {
	    printf("Read error  %ld for %s\n", err, path);
	    goto bail;
	}
	total  += cnt;
	bufp += cnt;
    } while (cnt && (total < max));

    tune_file_size = (int)total;

    printf("%s loaded %d bytes @ 0x%x\n",
	path, tune_file_size, tune_buf); 

bail:
    Close(fd);
    return(err);
}

/*
 * play - play one of the built in tunes
 */
/*ARGSUSED*/
int
play_cmd(int argc, char **argv, char **envp)
{
    int tune;
    char *path;
    
    tune_file_size =  0;
    while (--argc) {
	argv++;
	if ( !strcmp("-f", *argv)) {
	    --argc;++argv;
	    if (argc < 0)
		return -1;
	    else {
		if (get_file(*argv) != ESUCCESS)
		    return -1;
	    }
	}
	else
	    tune = atoi(*argv);
    }

    play_hello_tune(tune);
    wait_hello_tune();
    return 0;
}
