/*****************************************************************************
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <procfs/procfs.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/sysmp.h>
#include <sys/SN/hwcntrs.h>

#define HPSIZE           (0x1000)
#define HPSIZE_MASK      (HPSIZE-1)
#define HPSIZE_SHIFT     (12)

#define SPSIZE           (0x4000)

#define DATA_POOL_SIZE   (128*1024)
#define CACHE_TRASH_SIZE ((4*1024*1024)/sizeof(long))

#define NCHUNKS 10
#define CHUNK_SIZE (64*1024)

char data_pool[DATA_POOL_SIZE];
long cache_trash_buffer[CACHE_TRASH_SIZE];

void
ccpm_create(pmo_handle_t* pm, pmo_handle_t* mld, int num_mlds, char* pp)
{
        pmo_handle_t mldset;
        policy_set_t policy_set;
        int i;


        if (strcmp(pp, "PlacementFixed") != 0 &&
            strcmp(pp, "PlacementCacheColor") != 0) {
                fprintf(stderr, "Invalid Placement Policy\n");
                exit(1);
        }
        
        for (i = 0; i < num_mlds; i++) {
                if ((mld[i] = mld_create(0, MLD_DEFAULTSIZE)) < 0) {
                        perror("mld_create");
                        exit(1);
                }
        }

        if ((mldset = mldset_create(mld, num_mlds)) < 0) {
                perror("mldst_create");
                exit(1);
        }

        if (mldset_place(mldset,
                         TOPOLOGY_FREE,
                         NULL,
                         0,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        for (i = 0; i < num_mlds; i++) {
                pm_filldefault(&policy_set);

                policy_set.placement_policy_name = pp;
                policy_set.placement_policy_args = (void*)mld[i];
                policy_set.migration_policy_name = "MigrationRefcnt";
                policy_set.migration_policy_args = NULL;
                
                if ((pm[i] = pm_create(&policy_set)) < 0) {
                        perror("pm_create");
                        exit(1);
                }

        }
}


void
print_refcounters(char* vaddr, int len)
{
        pid_t pid = getpid();
        char  pfile[256];
        int fd;
        sn0_refcnt_buf_t* refcnt_buffer;
        sn0_refcnt_buf_t* direct_refcnt_buffer;        
        sn0_refcnt_args_t* refcnt_args;
        int npages;
        int gen_start;
        int numnodes;
        int page;
        int node;

        sprintf(pfile, "/proc/%05d", pid);
        if ((fd = open(pfile, O_RDONLY)) < 0) {
		fprintf(stderr,"Can't open /proc/%d", pid);
		exit(1);
	}

        vaddr = (char *)( (unsigned long)vaddr & ~HPSIZE_MASK );
        npages = (len + HPSIZE_MASK) >> (HPSIZE_SHIFT);

        if ((refcnt_buffer = malloc(sizeof(sn0_refcnt_buf_t) * npages)) == NULL) {
                perror("malloc refcnt_buffer");
                exit(1);
        }
        
        if ((direct_refcnt_buffer = malloc(sizeof(sn0_refcnt_buf_t) * npages)) == NULL) {
                perror("malloc refcnt_buffer");
                exit(1);
        }
        
        if ((refcnt_args = malloc(sizeof(sn0_refcnt_args_t))) == NULL) {
                perror("malloc refcnt_args");
                exit(1);
        }

        refcnt_args->vaddr = (__uint64_t)vaddr;
        refcnt_args->len = len;
        refcnt_args->buf = refcnt_buffer;

        if ((gen_start = ioctl(fd, PIOCGETSN0EXTREFCNTRS, (void *)refcnt_args)) < 0) {
		perror("ioctl  PIOCGETSN0EXTREFCNTRS returns error");
		exit(1);
	}

        refcnt_args->vaddr = (__uint64_t)vaddr;
        refcnt_args->len = len;
        refcnt_args->buf = direct_refcnt_buffer;
        
        if ((gen_start = ioctl(fd, PIOCGETSN0REFCNTRS, (void *)refcnt_args)) < 0) {
		perror("ioctl  PIOCGETSN0REFCNTRS returns error");
		exit(1);
	}
        
        if ((numnodes = sysmp(MP_NUMNODES)) < 0) {
                perror("sysmp MP_NUMNODES");
                exit(1);
        }

#ifdef NUMA_TEST_VERBOSE        
        for (page = 0; page < npages; page++) {
                printf("page[%05d, 0x%lx, 0x%llx (0x%llx) {%06d}]:",
                       page,
                       vaddr + page*0x1000,
                       refcnt_buffer[page].paddr,
                       refcnt_buffer[page].paddr >> 14,
                       (uint)((refcnt_buffer[page].paddr >> 14) & (__uint64_t)0x3F));
                for (node = 0; node < numnodes; node++) {
                        printf(" %lld (%lld)",
                               refcnt_buffer[page].refcnt_set.refcnt[node],
                               direct_refcnt_buffer[page].refcnt_set.refcnt[node]);
                }
                printf("\n");
        }
#endif /* NUMA_TEST_VERBOSE */        

        close(fd);
        free(refcnt_args);
        free(refcnt_buffer);
}


void
init_buffer(void* m, size_t size)
{
        size_t i;
        char* p = (char*)m;
        
        for (i = 0; i < size; i++) {
                p[i] = (char)i;
        }
}

long
buffer_auto_dotproduct_update(void* m, size_t size)
{
        size_t i;
        size_t j;
        char* p = (char*)m;
        long sum = 0;

        
        for (i = 0, j = size - 1; i < size; i++, j--) {
                sum += (long)p[i]-- * (long)p[j]++;
        }

        return (sum);
}

long
cache_trash(long* m, size_t long_size)
{
        int i;
        long sum = 0;

        for (i = 0; i < long_size; i++) {
               m[i] = i;
        }

        for (i = 0; i < long_size; i++) {
                sum += m[i];
        }

        return (sum);
}

void
do_stuff(void* m, size_t size, int loops, char* label)
{
        long total = 0;
        int count = loops;

        while (count--) {
                total += buffer_auto_dotproduct_update(m, size);
                total += cache_trash(cache_trash_buffer, CACHE_TRASH_SIZE);
        }
#ifdef NUMA_TEST_VERBOSE
        printf("{%s}, sum after %d loops: 0x%x\n", label, loops, total);
#endif /* NUMA_TEST_VERBOSE */        
}

void
main(int argc, char** argv)
{
        int numnodes;
        size_t arraysize;
        char* placement_policy;
        size_t base_chunksize;
        size_t tail_chunksize;
        size_t chunksize[64];
        size_t accindex;
        size_t chunkindex[64];
        pmo_handle_t pm[64];
        pmo_handle_t mld[64];
        int i;
        char* memory;
        char* A;
        char* B;
        
        if (argc != 4) {
                fprintf(stderr, "(ERROR) Usage: %s <num-nodes> <array-size in KB> <PlacPol>\n",
                        argv[0]);
                exit(1);
        }

        numnodes = atoi(argv[1]);
        if (numnodes < 1 || numnodes > 64) {
                fprintf(stderr, "Invalid number of nodes\n");
                exit(1);
        }

        arraysize = 1024 * atoi(argv[2]);
        if (arraysize <= 0) {
                fprintf(stderr, "Invalid array size\n");
                exit(1);
        }

        placement_policy = argv[3];

#ifdef NUMA_TEST_VERBOSE
        printf("Running %s with %d nodes, arrays of size %ld [B], using %s\n",
               argv[0], numnodes, arraysize, placement_policy);
#endif /* NUMA_TEST_VERBOSE */
        
        memory = (char*)memalign(SPSIZE, arraysize * 2);
        if (memory == NULL) {
                perror("memalign");
                exit(1);
        }
        
        A = &memory[0];
        B = &memory[arraysize];

        ccpm_create(pm, mld, numnodes, placement_policy);

        base_chunksize = ((arraysize / SPSIZE) / numnodes) * SPSIZE;
#ifdef NUMA_TEST_VERBOSE
        printf("Base chunksize: %ld\n", base_chunksize);
#endif /* NUMA_TEST_VERBOSE */
        
        tail_chunksize = arraysize;
        accindex = 0;
        for (i= 0; i < (numnodes - 1); i++) {
                chunksize[i] = base_chunksize;
                chunkindex[i] = accindex;
                accindex += base_chunksize;
                tail_chunksize -= base_chunksize;
        }
        chunksize[numnodes - 1] = tail_chunksize;
        chunkindex[numnodes - 1] = accindex;

#ifdef NUMA_TEST_VERBOSE
        for (i = 0; i < numnodes; i++) {
                printf("Chunksize[%d]=%ld, Chunkindex[%d]=%ld\n",
                       i, chunksize[i], i, chunkindex[i]);
        }
#endif /* NUMA_TEST_VERBOSE */
        
        for (i = 0; i < numnodes; i++) {
                if (pm_attach(pm[i], &A[chunkindex[i]], chunksize[i]) < 0) {
                        perror("pm_attach");
                        exit(1);
                }
                if (pm_attach(pm[i], &B[chunkindex[i]], chunksize[i]) < 0) {
                        perror("pm_attach");
                        exit(1);
                }                
        }
        
        do_stuff(A, arraysize, 20, "ARRAY-A");
        do_stuff(B, arraysize, 20, "ARRAY_B");


        for (i = 0; i < numnodes; i++) {
#ifdef NUMA_TEST_VERBOSE                
                printf("Pages associated to node %d:\n", i);
#endif /* NUMA_TEST_VERBOSE */
                print_refcounters(&A[chunkindex[i]], chunksize[i]);
                print_refcounters(&B[chunkindex[i]], chunksize[i]);
        }
}        
        
        
        
                

        
        

        
