
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_vm.c measures virtual memory access performance. */

#include "unixperf.h"
#include "test_vm.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <bstring.h>

Bool
InitDevZero(Version version)
{
    fd = open("/dev/zero", O_RDWR);
    SYSERROR(fd, "open");
    return TRUE;
}

unsigned int
DoDevZeroMap(void)
{
    char *region;
    int i;

    for(i=50;i>0;i--) {
        region = mmap(0, 1024*1024, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        munmap(region, 1024*1024);
    }
    return 50;
}

unsigned int
DoDevZeroWalk(void)
{
    char *region, *end, *ptr;

    region = mmap(0, 1024*1024, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    end = region + 1024*1024;
    for(ptr=region;ptr<end;ptr+=1024) {
        *ptr = 1;
    }
    munmap(region, 1024*1024);
    return 1;
}

void
CleanupDevZero(void)
{
    close(fd);
    SYSERROR(fd, "close");
}

Bool
InitMmapMegabyteFile(Version version)
{
#define SIZE 256
    char filename[200];
    int buffer[SIZE];
    int i;

    sprintf(filename, "%s/unixperf-bigfile-%d", TmpDir, getpid());
    fd = open(filename, O_WRONLY|O_CREAT, 0644);
    SYSERROR(fd, "open");
    for(i=0;i<SIZE;i++) {
	buffer[i] = i;
    }
    for(i=0;i<1024*1024;i+=sizeof(buffer)) {
        rc = write(fd, buffer, sizeof(buffer));
	SYSERROR(rc, "write");
    }
    rc = close(fd);
    SYSERROR(rc, "close");
    fd = open(filename, O_RDONLY, 0);
    SYSERROR(fd, "open");
    return TRUE;
#undef SIZE
}

unsigned int
DoMmapMegabyteFile(void)
{
    char *region, *end, *ptr;
    char buffer[1024];

    region = mmap(0, 1024*1024, PROT_READ, MAP_PRIVATE, fd, 0);
    if((void*)region == (void*)-1L) DOSYSERROR("mmap");
    end = region + 1024*1024;
    for(ptr=region;ptr<end;ptr+=1024) {
	bcopy((void*)ptr, (void*)buffer, 1024);
    }
    munmap(region, 1024*1024);
    return 1;
}

void
CleanupMmapMegabyteFile(void)
{
    char filename[200];

    sprintf(filename, "%s/unixperf-bigfile-%d", TmpDir, getpid());
    rc = unlink(filename);
    SYSERROR(rc, "unlink");
    close(fd);
    SYSERROR(fd, "close");
}

