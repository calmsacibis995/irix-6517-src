#ifndef __COB_H__
#define __COB_H__

#define MAX_FILES	20

#define NUM_SECTS       4
/* Which sections will be converted? */

#define TEXT_S          0
#define DATA_S          1
#define BSS_S           2
#define SHARED_S        3       /* Fake section name for globally shared
                                   niblet section */

#define NUM_REAL_SECTS	2
/* Sections whose data actually need to be saved away must come before 
   the all zero sections (like BSS and "SHARED" */

struct obj_inf {
        char name[25];                         /* Section name */
        unsigned int scn_size[NUM_SECTS];       /* Size of each section */
        unsigned int scn_addr[NUM_SECTS];       /* Address of each section */
        unsigned int scn_end[NUM_SECTS];        /* End of each section */
        unsigned int *scn_ptr[NUM_SECTS];       /* Ptr to each section's mem. */
        int start_addr[NUM_SECTS];              /* Start of scn's mem */
        int real_size[NUM_SECTS];               /* How long is this proc ? */
        int end_addr[NUM_SECTS];                /* Last page of proc's mem. */
	int pg_tbl_addr;			/* Page table address */
	int pg_tbl_end;				/* End of page table (+ 1) */
        int entry_addr;                         /* Virtual address of ENTRY */
};

#define LABEL_LENGTH    8

#endif /* __COB_H__ */
