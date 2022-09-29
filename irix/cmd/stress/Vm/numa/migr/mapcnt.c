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


#define DATA_POOL_SIZE  (8*16*1024)
#define CACHE_TRASH_SIZE ((4*1024*1024)/sizeof(long))

char fixed_data_pool[DATA_POOL_SIZE];
long cache_trash_buffer[CACHE_TRASH_SIZE];

/*
 * Reference Counter Configuration for all nodes
 */
rcb_info_t** rcbinfo;

/*
 * Hardware Page Size
 */
uint hw_page_size;

/*
 * Physical Memory Config for all nodes
 */
rcb_slot_t** slotconfig;

/*
 * Mapped counters for all nodes
 */
refcnt_t** cbuffer;

/*
 * Verbose ?
 */

int verbose = 0;

void
print_rcb(int node, rcb_info_t* rcb, rcb_slot_t* slot)
{
        int s;
        
        printf("RCB for node [%d]\n", node);
        
        printf("\trcb_len: %lld\n", rcb->rcb_len);
        printf("\trcb_sw_sets: %d\n", rcb->rcb_sw_sets);        
        printf("\trcb_sw_counters_per_set: %d\n", rcb->rcb_sw_counters_per_set);
        printf("\trcb_sw_counter_size: %d\n", rcb->rcb_sw_counter_size);
        
        printf("\trcb_base_pages: %d\n", rcb->rcb_base_pages);
        printf("\trcb_base_page_size: %d\n", rcb->rcb_base_page_size);
        printf("\trcb_base_paddr: 0x%llx\n", rcb->rcb_base_paddr);
        
        printf("\trcb_cnodeid: %d\n", rcb->rcb_cnodeid);
        printf("\trcb_granularity: %d\n", rcb->rcb_granularity);
        printf("\trcb_hw_counter_max: %d\n", rcb->rcb_hw_counter_max);
        printf("\trcb_diff_threshold: %d\n", rcb->rcb_diff_threshold);        
        printf("\trcb_abs_threshold: %d\n", rcb->rcb_abs_threshold);

        for (s = 0; s < rcb->rcb_num_slots; s++) {
                printf("\tSlot[%d]: 0x%llx -> 0x%llx, size: 0x%llx\n",
                       s, slot[s].base, slot[s].base + slot[s].size, slot[s].size);
        }
}

void
mmap_counters(void)
{
        int fd;
        char refcnt[1024];
        refcnt_t* set_base;
        int numnodes;
        int node;

        /* number of nodes */

        numnodes = sysmp(MP_NUMNODES);

        /* space for refcnt config -- just basic array for now */

        rcbinfo = (rcb_info_t**)malloc(sizeof(rcb_info_t*) * numnodes);
        if (rcbinfo == NULL) {
                perror("malloc");
                exit(1);
        }

        /* space for phys mem config -- just basic array for now*/

        slotconfig = (rcb_slot_t**)malloc(sizeof(rcb_slot_t*) * numnodes);
        if (slotconfig == NULL) {
                perror("malloc");
                exit(1);
        }

        /* space for array of pointers to the counter buffers */
        cbuffer = (refcnt_t**)malloc(sizeof(refcnt_t*) * numnodes);
        if (cbuffer == NULL) {
                perror("malloc");
                exit(1);
        }

        for (node = 0; node < numnodes; node++) {
                sprintf(refcnt, "/hw/nodenum/%d/refcnt", node);
                if (verbose) {
                        printf("Opening dev %s\n", refcnt);
                }
        
                if ((fd = open(refcnt, O_RDONLY)) < 0) {
                        perror("open");
                        exit(1);
                }

                /* get rcb info */
                
                rcbinfo[node] = (rcb_info_t*)malloc(sizeof(rcb_info_t));
                if (rcbinfo[node] == NULL) {
                        perror("malloc");
                        exit(1);
                }
                
                if (ioctl(fd, RCB_INFO_GET, rcbinfo[node]) < 0) {
                        perror("icctl RCB_INFO_GET");
                        exit(1);
                }

                /* get phys mem config */
                
                slotconfig[node] = (rcb_slot_t*)malloc(rcbinfo[node]->rcb_num_slots * sizeof(rcb_slot_t));
                if (slotconfig[node] == NULL) {
                        perror("malloc");
                        exit(1);
                }

                if (ioctl(fd, RCB_SLOT_GET, slotconfig[node]) < 0) {
                        perror("ioctl RCB_SLOT_GET");
                        exit(1);
                }                

                /* map the counter buffer for this node */
                cbuffer[node] = (refcnt_t*)mmap(0, rcbinfo[node]->rcb_len, PROT_READ, MAP_SHARED, fd, 0);
                if (cbuffer[node] == (refcnt_t*)MAP_FAILED) {
                        perror("mmap");
                        exit(1);
                }

                if (verbose) {
                        print_rcb(node, rcbinfo[node], slotconfig[node]);
                }

                if (close(fd) <  0) {
                        perror("close");
                        exit(1);
                }
                
        }
}
  

uint
logb2(uint v)
{
        uint r;
        uint l;

        r = 0;
        l = 1;
        while (l < v) {
                r++;
                l <<= 1;
        }

        return (r);
}


refcnt_t*
paddr_to_setbase(int node, __uint64_t paddr)
{
        int slot_index;
        int s;
        uint set_offset;
        int btoset_shift;
        refcnt_t* set_base;
        

        btoset_shift = logb2(rcbinfo[node]->rcb_granularity);
        slot_index = -1;
        set_offset = 0;

        for (s = 1; s < rcbinfo[node]->rcb_num_slots; s++) {
                if (paddr < slotconfig[node][s].base) {
                        slot_index = s - 1;
                        break;
                }
                set_offset += slotconfig[node][s - 1].size >> btoset_shift;
        }
        if (slot_index < 0) {
                fprintf(stderr, "Could not find slot\n");
                exit(1);
        }
        
        set_offset += (paddr - slotconfig[node][slot_index].base) >> btoset_shift;
#ifdef DEBUG
        printf("set_offset: %d\n", set_offset);
        printf("paddr:0x%llx, slot_base: 0x%llx, slot_index:%d\n",
               paddr, slotconfig[node][slot_index].base, slot_index);
#endif
        set_base  = cbuffer[node] + set_offset * rcbinfo[node]->rcb_sw_counters_per_set;
        
        return (set_base);
}

void
place_data(char* vaddr, int size, char* node, int migr_on)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        pmo_handle_t pm;
        policy_set_t policy_set;
        migr_policy_uparms_t migr_parms;

        if ((mld = mld_create(0, size)) < 0) {
                perror("mld_create");
                exit(1);
        }

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldst_create");
                exit(1);
        }

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist,
                         1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        pm_filldefault(&policy_set);

        policy_set.placement_policy_name = "PlacementFixed";
        policy_set.placement_policy_args = (void*)mld;
        policy_set.migration_policy_name = "MigrationRefcnt";
        policy_set.migration_policy_args = NULL;
                
        if ((pm = pm_create(&policy_set)) < 0) {
                perror("pm_create");
                exit(1);
        }

        if (pm_attach(pm, vaddr, size) < 0) {
                perror("pm_attach");
                exit(1);
        }

}

void
place_process(char* node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        
        /*
         * The mld, radius = 0 (from one node only)
         */
        
        if ((mld = mld_create(0, 0)) < 0) {
                perror("mld_create");
                exit(1);
        }

        /*
         * The mldset
         */

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldset_create");
                exit(1);
        }

        /*
         * Placing the mldset with the one mld
         */

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist, 1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        /*
         * Attach this process to run only on the node
         * where thr mld has been placed.
         */

        if (process_mldlink(0, mld, RQMODE_MANDATORY) < 0) {
                perror("process_mldlink");
                exit(1);
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
        char mem_node[512];
        refcnt_t* set_base;

        sprintf(pfile, "/proc/%05d", pid);
        if ((fd = open(pfile, O_RDONLY)) < 0) {
		fprintf(stderr,"Can't open /proc/%d", pid);
		exit(1);
	}

        vaddr = (char *)( (unsigned long)vaddr & ~(hw_page_size-1) );
        npages = (len + (hw_page_size-1)) >> logb2(hw_page_size);

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

        for (page = 0; page < npages; page++) {
                printf("page[%05d, 0x%lx, 0x%llx (0x%llx)]:",
                       page,
                       vaddr + page*0x1000,
                       refcnt_buffer[page].paddr,
                       refcnt_buffer[page].paddr >> 14);
                for (node = 0; node < numnodes; node++) {
                        printf(" %ll05d (%ll06d)",
                               refcnt_buffer[page].refcnt_set.refcnt[node],
                               direct_refcnt_buffer[page].refcnt_set.refcnt[node]);
                }
                printf("\n");
                
                set_base = paddr_to_setbase(refcnt_buffer[page].cnodeid, refcnt_buffer[page].paddr);
                printf("MMAPPED CTRS: ");
                for (node = 0; node < numnodes; node++) {
                        printf(" %ll05d (%ll06d)",
                               set_base[node],
                               direct_refcnt_buffer[page].refcnt_set.refcnt[node]);
                }
                printf("\n");
        }

        close(fd);
        free(refcnt_args);
        free(refcnt_buffer);
}

void
check_refcounters(char* vaddr, int len)
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
        char mem_node[512];
        refcnt_t* set_base;

        sprintf(pfile, "/proc/%05d", pid);
        if ((fd = open(pfile, O_RDONLY)) < 0) {
		fprintf(stderr,"Can't open /proc/%d", pid);
		exit(1);
	}

        vaddr = (char *)( (unsigned long)vaddr & ~0xfff );
        npages = (len + 0xfff) >> 12;

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

        for (page = 0; page < npages; page++) {
                set_base = paddr_to_setbase(refcnt_buffer[page].cnodeid, refcnt_buffer[page].paddr);
                for (node = 0; node < numnodes; node++) {
                        if (refcnt_buffer[page].refcnt_set.refcnt[node] != set_base[node]) {
                                if (verbose) {
                                        fprintf(stderr, "DIFF: procf-refcnt: %lld, mmapped-refcnt: %lld\n",
                                                refcnt_buffer[page].refcnt_set.refcnt[node], set_base[node]);
                                }
                        }
                }
        }

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

        if (verbose) {
                printf("{%s}, sum after %d loops: 0x%x\n", label, loops, total);
        }
}



void
main(int argc, char** argv)
{
        char* thread_node;
        char* mem_node;

        if (argc != 4) {
                fprintf(stderr, "Usage %s <thread-node> <mem-node> <0|1 (verbose)>\n", argv[0]);
                exit(1);
        }

        thread_node = argv[1];
        mem_node = argv[2];
        verbose = atoi(argv[3]);

        mmap_counters();
        hw_page_size = rcbinfo[0]->rcb_granularity;
        
        /*
         * Place data, migr off
         */
        place_data(&fixed_data_pool[0], DATA_POOL_SIZE, mem_node, 0);
        init_buffer(&fixed_data_pool[0], DATA_POOL_SIZE);
        
        /*
         * Place process
         */
        place_process(thread_node);

        
        /*
         * Reference pages & verify
         */

        do_stuff(fixed_data_pool, DATA_POOL_SIZE, 100, "FIXED");

        if (verbose) {
                print_refcounters(fixed_data_pool, DATA_POOL_SIZE);
        }

        check_refcounters(fixed_data_pool, DATA_POOL_SIZE);

        

}        
        
        
        
                

        
        

        
