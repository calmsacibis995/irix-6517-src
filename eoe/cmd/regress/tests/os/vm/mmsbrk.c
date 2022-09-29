#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

extern int errno;

#ifndef PROT_EXEC
#define PROT_EXEC PROT_EXECUTE
#endif

#ifdef NOGETPAGESIZE
#define getpagesize() sysconf(_SC_PAGESIZE)
#endif

#ifdef NOMKSTEMP
#include <sys/stat.h>
#include <fcntl.h>

int mkstemp(char *s) {
    mktemp(s);
    if (*s == 0) return -1;
    return open(s, O_RDWR|O_CREAT, 0x777);
}
#endif

static int tmpfd = -1;
static char tmpfn[] = "/tmp/tXXXXXX";

void unlink_tmpfn(void) {
    close(tmpfd);
    unlink(tmpfn);
}

void abort(char *s) {
    if (tmpfd != -1) {
        unlink_tmpfn();
    }
    perror(s);
    exit(1);
}

int main(void) {
    int ps, i, n;
    void *s;

  /* pick up host page size */
    if ((ps = getpagesize()) == -1) abort("getpagesize");

  /* open a temp file */
    if ((tmpfd = mkstemp(tmpfn)) == -1) abort("mkstemp");

  /* write a page or slightly more */
    n = strlen(tmpfn);
    for(i = 0; i < ps; i += n)
        if (write(tmpfd, tmpfn, n) != n) abort("write");

  /* reserve two pages worth from sbrk */
    if ((int)(s = (void *)sbrk(ps * 2)) == -1) abort("sbrk");

  /* align on page boundary (assumes page size is power of two) */
    s = (void *)(((int)s + (ps - 1)) & ~(ps - 1));

  /* map over one page of sbrk'd area */
    if ((int)(mmap(s,
                   (size_t)ps,
                   PROT_READ|PROT_WRITE|PROT_EXEC,
                   MAP_FIXED|MAP_PRIVATE,
                   tmpfd,
                   (off_t)0)) == -1)
        abort("mmap");

    unlink_tmpfn();

  /* check sbrk */
    if ((int)sbrk(ps) == -1) {
        printf("test 1 failed, errno = %d\n", errno);
        exit(2);
    }

    if ((int)sbrk(ps) == -1) {
        printf("test 2 failed, errno = %d\n", errno);
        exit(2);
    }

    printf("test succeeded\n");
    return 0;
}
