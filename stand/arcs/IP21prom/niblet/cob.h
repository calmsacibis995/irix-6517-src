/* REVISION HISTORY
   ----------------
3/21/94  RAR  Increased Section name array allocation from 25 to 35 in
               structure obj_inf to accomodate longer test names.
*/

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

typedef unsigned long long tm_addr_type; 
typedef unsigned long long hm_addr_type; 
typedef unsigned long long em_addr_type; 
/* typedef unsigned long long addr_type; */
typedef unsigned int scn_data;
typedef scn_data *scn_data_ptr;

struct obj_inf {
        char name[35];                          /* Section name */
        unsigned int scn_size[NUM_SECTS];       /* Size of each section */
        tm_addr_type scn_addr[NUM_SECTS];       /* Address of each section */
        tm_addr_type scn_end[NUM_SECTS];        /* End of each section */
        scn_data_ptr scn_ptr[NUM_SECTS];        /* Ptr to each section's mem. */
        tm_addr_type start_addr[NUM_SECTS];     /* Start of scn's mem */
        int real_size[NUM_SECTS];               /* How long is this proc ? */
        tm_addr_type end_addr[NUM_SECTS];       /* Last page of proc's mem. */
		tm_addr_type pg_tbl_addr;               /* Page table address */
		tm_addr_type pg_tbl_end;				/* End of page table (+ 1) */
        tm_addr_type entry_addr;                /* Virtual address of ENTRY */
};

#define LABEL_LENGTH    8

#endif /* __COB_H__ */
