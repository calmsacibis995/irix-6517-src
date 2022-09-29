/*
 *=============================================================================
 *			File: 		part.c
 *			Purpose:	Partition table manipulator.
 *	Note:
 *	We always start with a clean state. We create partitions in a specific.
 * 	order. The first three partitions that're created are regular primary
 * 	partitions. When we create subsequent partitions, we first create one
 * 	extended partition, and then create logical partitions *within* this
 * 	extended partition.
 *				
 *=============================================================================
 */
#include "part.h"
#include "misc.h"

#define	OUT_OF_SPACE	(65535)

int	fd;				/* Device file descr */
u_int	heads;				/* Disk geometry     */
u_int	sectors;			/* Disk geometry     */
u_int	cylinders;			/* Disk geometry     */
char	*disk_device;			/* Device name       */
extern	int	debug_flag;		/* Flag for debug info*/
extern	int	verbose_flag;		/* Flag for verbosity */

/*
 *----------------------------------------------------------------------------
 * Global variables.
 *----------------------------------------------------------------------------
 */
char	buffer[SECTOR_SIZE];		
char	exists[MAX_PARTNS];	
char	changed[MAX_PARTNS];
char	*buffers[MAX_PARTNS];

int	part_count = 0;				
int	ext_index;
u_short	ext_bcyl;
u_short ext_ecyl;
u_long	ext_size;
u_long	ext_offset;

pte_t 	*part_tbl[MAX_PARTNS];
u_long	offsets[MAX_PARTNS];	

struct 	systypes {	
        u_char 	index;
        char 	*name;
} sys_types[] = {
                {0, "Empty"		},
                {1, "DOS 12-bit FAT"	},
                {4, "DOS 16-bit <32M"	},
                {PTE_EXTENDED, "Extended"},
                {6, "DOS 16-bit >=32M"	}
};

/*
 *----------------------------------------------------------------------------
 * External interface
 *----------------------------------------------------------------------------
 */
void	part_read_disk(void);
void 	part_read_logic(u_long ext_offset);
void 	part_data_sync(void);
void	part_incore_init(void);

int     part_create(u_long *dsize, u_long *daddr);
int	part_entire_create(u_long *dsize, u_long *daddr);
int     part_prime_create(int indx, u_long *dsize, u_long *daddr);
int     part_extnd_create(int indx, u_long *dsize, u_long *daddr);
int     part_logic_create(int indx, u_long *dsize, u_long *daddr);

/*
 *----------------------------------------------------------------------------
 * Function Prototypes.
 *----------------------------------------------------------------------------
 */
void 	part_table_print(void);
void	part_exist_set(int i);
void 	part_exist_clr(void);
void 	part_changed_set(int i);
void 	part_changed_clr(void);
void 	part_pte_print(int partn, pte_t *p, 
		       u_long  ptbl_offset, u_long  part_offset);
pte_t  *part_pte_find(int partn, u_long *ptbl_offset, u_long *part_offset);
void    part_pte_clr(pte_t *p);
void    part_pte_set_prime(pte_t *p, u_char type, 
                     u_char h1, u_short s1,  u_short c1,
                     u_char h2, u_short s2,  u_short c2);
void	part_pte_set_logic(pte_t *p, u_char type,
		     u_char h1, u_short s1,  u_short c1,
		     u_char h2, u_short s2,  u_short c2,
		     u_int  stt_sect, u_int num_sect);
u_short	part_nextcyl_find_prime(void);
u_short part_nextcyl_find_logic(void);
u_short part_size_to_cyl(u_long size, u_char h1, u_short c1);

void	set_bcyl(pte_t *p, u_short cyl);
void	set_ecyl(pte_t *p, u_short cyl);
void	set_bsect(pte_t *p, u_short sect);
void	set_esect(pte_t *p, u_short sect);

u_short	get_bcyl(pte_t *p);
u_short	get_ecyl(pte_t *p);
u_short	get_bsect(pte_t *p);
u_short get_esect(pte_t *p);

/*
 *----------------------------------------------------------------------------
 * part_incore_init()
 * This routine is used to initialize the in-core data structures.
 *----------------------------------------------------------------------------
 */
void	part_incore_init(void)
{
	part_exist_clr();
	part_changed_clr();
	bzero(buffer, SECTOR_SIZE);
	buffers[0]  = buffer;
	buffers[1]  = buffer;
	buffers[2]  = buffer;
	buffers[3]  = buffer;
	ext_ecyl    = -1;
	ext_bcyl    = -1;
	ext_size    = 0;
	ext_index   = -1;
	ext_offset  = 0;
	part_tbl[0] = pte_offset(buffer, 0);
	part_tbl[1] = pte_offset(buffer, 1);
	part_tbl[2] = pte_offset(buffer, 2);
	part_tbl[3] = pte_offset(buffer, 3);
	offsets[0]  = 0;
	offsets[1]  = 0;
	offsets[2]  = 0;
	offsets[3]  = 0;
	
	
}

/*
 *----------------------------------------------------------------------------
 * part_read_disk()
 * This routine reads the master boot record of the hard-drive and then
 * invokes other functions to read extended partitions.
 *----------------------------------------------------------------------------
 */
void	part_read_disk(void)
{
	pte_t	*p;
	int	partn;

	gread(buffer, SECTOR_SIZE);
	part_exist_clr();
	part_changed_clr();
	for (partn = 0; partn < 4; partn++){
		p = pte_offset(buffer, partn);
		if (p->type)
			part_count++;
		if (p->type == PTE_EXTENDED){
			ext_index  = partn;
			ext_offset = p_start_sect(p);
			ext_size   = p_numbr_sect(p);
			part_read_logic(ext_offset);
		}
		offsets[partn] = p_start_sect(p);
		exists[partn]  = 1;
	}
	part_table_print();
	return;	
}

/*
 *----------------------------------------------------------------------------
 * part_read_logic()
 * This routine looks at all the logical volumes within an extended partn.
 * We work with the assumption that extended partitions can't be nested.
 * NOTE: offsets[] = holds the offset for the partition table at which
 *                   this particular partition starts.
 *----------------------------------------------------------------------------
 */
void	part_read_logic(u_long ext_offset)
{
	int	 nread;
	pte_t	 *p1, *p2;
	u_long 	 next_offset = 0;

	part_count = 4;
	do {
		exists[part_count]  = 1;
		offsets[part_count] = ext_offset+next_offset;
		buffers[part_count] = (char *) gmalloc(SECTOR_SIZE);
		gseek(offsets[part_count]);
		gread(buffers[part_count], SECTOR_SIZE);
		p1 = pte_offset(buffers[part_count], 0);
		p2 = pte_offset(buffers[part_count], 1);
		next_offset = p_start_sect(p2);
		part_count++;	
	} while (next_offset);
	return;
}

/*
 *----------------------------------------------------------------------------
 * part_table_print()
 * This routine prints out the partition table.
 * It first prints out the partition table in the MBR.
 * It later prints out the logical volumes inside the extended partition.
 * These are numbered from 0.
 *----------------------------------------------------------------------------
 */
void    part_table_print(void)
{
        int      partn;
        char     part_type;
        pte_t    *p;
        u_long   part_offset;
        u_long   ptbl_offset;
	
	if (verbose_flag){	
                for (partn = 0; partn < part_count; partn++){
		    if (exists[partn]){
                        p = part_pte_find(partn, &ptbl_offset, &part_offset);
			part_pte_print(partn, p, ptbl_offset, part_offset);
		    }
           	}
	}
        return;
}

/*
 *---------------------------------------------------------------------------
 * part_pte_find()
 * This routine is used to find a partition table entry, given the partition
 * number (The numbering of this starts with zero).
 *---------------------------------------------------------------------------
 */
pte_t   *part_pte_find(int partn, u_long *ptbl_offset, u_long *part_offset)
{
        pte_t   *p1, *p2;

        if (partn < 4){
                p1 = pte_offset(buffer, partn);
                *ptbl_offset = 0;
                *part_offset = p_start_sect(p1);
                return (p1);
        }
        p1= pte_offset(buffer, ext_index);
        ext_offset = p_start_sect(p1);
        if (partn == 4){
                p2 = pte_offset(buffers[partn], 0);
                *ptbl_offset = ext_offset;
                *part_offset = ext_offset+p_start_sect(p2);
        }
        else {
                p1 = pte_offset(buffers[partn-1], 1);
                p2 = pte_offset(buffers[partn], 0);
                *ptbl_offset = ext_offset+p_start_sect(p1);
                *part_offset = ext_offset+p_start_sect(p1)+p_start_sect(p2);
        }
        return (p2);
}

/*
 *---------------------------------------------------------------------------
 * part_create()
 * This routine provides the top-level create interface for partition creatn.
 * NOTE:
 * (1) partitions #0, #1, #2: regular partitions.
 * (2) partition  #3        : extendd partition.
 * (3) partition  #4, #5, ..: logical partition.
 *---------------------------------------------------------------------------
 */
int     part_create(u_long *dsize, u_long *daddr)
{
        int     ret, indx;
        u_long  in_size, out_size;
        
       	indx = part_count;
        in_size = *dsize;
        if (indx < 3){
                ret  = part_prime_create(indx, &in_size, daddr);
		if (ret == 0){
			part_count++;
			*dsize = in_size;
			return (0);
		}
		else {
			*dsize = -1;
			*daddr = 0;
			return (1);
		}
        }
        else if (indx == 3){
                ret  = part_extnd_create(indx, &in_size, daddr);
		if (ret == 0){
			part_count++;
		}
		else {
			*dsize = -1;
			*daddr = 0;
			return (1);
		}
                in_size = *dsize;
                indx = part_count;
                ret = part_logic_create(indx, &in_size, daddr);
		if (ret == 0){
			part_count++;
                	*dsize = in_size;
			return (0);
		}
		else {
			*dsize = -1;
			*daddr = 0;
			return (1);
		}
        }
        else {
                ret  = part_logic_create(indx, &in_size, daddr);
		if (ret == 0){
			part_count++;
                	*dsize = in_size;
			return (0);
		}
		else {
			*dsize = -1;
			*daddr = 0;
			return (1);
		}
        }
}

/*
 *---------------------------------------------------------------------------
 * part_entire_create()
 * This routine is used to create a primary partition that encompasses the
 * entire device.
 *---------------------------------------------------------------------------
 */
int	part_entire_create(u_long *dsize, u_long *daddr)
{
        pte_t   *p;
        u_char  h1, h2;
        u_short s1, s2;
        u_short bcyl, ecyl;
        u_long  ptbl_offset;
        u_long  part_offset;
	u_long	part_size;

	h1 = 1;
	h2 = heads-1;
	s1 = 1;
	s2 = sectors;
	bcyl = 0;
	ecyl = cylinders-1;
	*dsize = part_size(h1, bcyl, ecyl)*SECTOR_SIZE;
	p = pte_offset(buffer, 0);
	part_exist_set(0);
	part_changed_set(0);
	part_flag_set(buffer);
	part_pte_set_prime(p, PTE_HUGE, h1, s1, bcyl, h2, s2, ecyl);


	*daddr = offsets[0] = part_begin(h1, bcyl)*SECTOR_SIZE;
#ifdef	PART_PRINT
	p = part_pte_find(0, &ptbl_offset, &part_offset);
	part_pte_print(0, p, ptbl_offset, part_offset);
#endif
	return (0);
}

/*
 *---------------------------------------------------------------------------
 * part_prime_create()
 * This routine is used to create a primary partition.
 * The first three partitions that're created are primary partitions.
 *---------------------------------------------------------------------------
 */
int     part_prime_create(int indx, u_long *dsize, u_long *daddr)
{
        pte_t   *p;
        u_char  h1, h2;
        u_short s1, s2;
        u_short bcyl, ecyl;
        u_long  ptbl_offset;
        u_long  part_offset;
        u_long  in_size, out_size;
	char	emsg[128];

        in_size = *dsize;
        bcyl = part_nextcyl_find_prime();
        if (bcyl == OUT_OF_SPACE){
		sprintf(emsg, "Partition %d cannot be accomodated, no space",
									indx);
                gerror(emsg);
                return (1);
        }
        if (indx == 0)
                h1 = 1;        
        else    h1 = 0;        
        h2   = heads-1;
        s1   = 1;               
        s2   = sectors;         
        ecyl = part_size_to_cyl(*dsize, h1, bcyl);   
        out_size = part_size(h1, bcyl, ecyl)*SECTOR_SIZE;
        if (ecyl >= cylinders){
		sprintf(emsg,
			"Partition %d cannot be accomodated, no cylinders",
									indx);
                gerror(emsg);
                return (1);
        }
        p = pte_offset(buffer, indx);  
        part_exist_set(indx);
        part_changed_set(indx);
        part_flag_set(buffer);
        part_pte_set_prime(p, PTE_HUGE, h1, s1, bcyl, h2, s2, ecyl);


        *daddr = offsets[indx] = part_begin(h1, bcyl)*SECTOR_SIZE;
        *dsize = out_size;
#ifdef	PART_PRINT
        p = part_pte_find(indx, &ptbl_offset, &part_offset);
        part_pte_print(indx, p, ptbl_offset, part_offset);
#endif
        return (0);
}

/*
 *---------------------------------------------------------------------------
 * part_extnd_create()
 * This routine is used to create a extended partition.
 * The fourth partition that's created is an extended partition.
 *---------------------------------------------------------------------------
 */
int     part_extnd_create(int indx, u_long *dsize, u_long *daddr)
{
        pte_t   *p;
        u_char  h1, h2;
        u_short s1, s2;
        u_short bcyl, ecyl;
        u_long  ptbl_offset;
        u_long  part_offset;
        u_long  in_size, out_size;
	char	emsg[128];

        in_size = *dsize;
        bcyl = part_nextcyl_find_prime();
        if (bcyl == OUT_OF_SPACE){
		sprintf(emsg,
			"Partition %d cannot be accomodated, no space",
									indx);
                gerror(emsg);
                return (1);
        }
        h1   = 0;        
        h2   = heads-1;
        s1   = 1;               
        s2   = sectors;         
        ecyl = cylinders-1;
        out_size = part_size(h1, bcyl, ecyl)*SECTOR_SIZE;
        if (out_size < in_size){
		sprintf(emsg,
			"Partition %d cannot be accomodated, no size",
									indx);
                gerror(emsg);
                return (1);
        }
        p = pte_offset(buffer, indx);  
        part_exist_set(indx);
        part_changed_set(indx);
        part_flag_set(buffer);
        part_pte_set_prime(p, PTE_EXTENDED, h1, s1, bcyl, h2, s2, ecyl);

        *daddr = offsets[indx] = part_begin(h1, bcyl)*SECTOR_SIZE;
        *dsize = out_size;
        ext_index  = indx;
        ext_offset = offsets[indx];
        ext_size   = out_size;
        ext_bcyl   = bcyl;
        ext_ecyl   = ecyl;
#ifdef	PART_PRINT
        p = part_pte_find(indx, &ptbl_offset, &part_offset);
        part_pte_print(indx, p, ptbl_offset, part_offset);
#endif
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * part_logic_create()
 * This routine is used to create a logical partition.
 * The fifth and subsequent partitions that are created are logical partitions.
 *-----------------------------------------------------------------------------
 */
int     part_logic_create(int indx, u_long *dsize, u_long *daddr)
{
	pte_t	*p;
        pte_t   *p1, *p2;
        pte_t   *q1, *q2;
	u_char  prevh1;
        u_char  h1, h2;
        u_short s1, s2;
        u_short bcyl, ecyl;
        u_long  ptbl_offset;
        u_long  part_offset;
        u_long  in_size, out_size;
        u_int   stt_sect, num_sect;
	char	emsg[128];

        in_size = *dsize;
        bcyl = part_nextcyl_find_logic();
        if (bcyl == OUT_OF_SPACE){
		sprintf(emsg,
			"Partition %d cannot be accomodated, no space",
									indx);
                gerror(emsg);
                return (1);
        }
        h1 = 1;
        h2 = heads-1;
        s1 = 1;
        s2 = sectors;
        ecyl = part_size_to_cyl(*dsize, h1, bcyl);
        out_size = part_size(h1, bcyl, ecyl)*SECTOR_SIZE;
        if (ecyl > ext_ecyl){
		sprintf(emsg,
			"Partition %d cannot be accomodated, no cylinders",
									indx);
                gerror(emsg);
                return (1);     
        }       
        buffers[indx] = gmalloc(SECTOR_SIZE);
        if (indx == 4){
		part_offset = (part_begin(h1, bcyl))*SECTOR_SIZE;
		ptbl_offset = (part_begin(h1, bcyl)-sectors)*SECTOR_SIZE;
                p1 = 0;
                p2 = 0;
                q1 = pte_offset(buffers[indx], 0);
                q2 = pte_offset(buffers[indx], 1);
                stt_sect = sectors;
                num_sect = part_size(h1, bcyl, ecyl);
                part_pte_set_logic(q1, PTE_HUGE, 
                                   h1, s1, bcyl,
                                   h2, s2, ecyl,
                                   stt_sect, num_sect);
                part_pte_clr(q2);
#ifdef	PART_PRINT
		printf(" Partition # (%d)\n", indx+1);
                printf(" Curr PTE {\n");
                printf(" q1->start = %d\n", get_start_sect(q1));
                printf(" q1->numbr = %d\n", get_numbr_sect(q1));
                printf(" Curr PTE }\n");
#endif
        }
        else {

		part_offset = (part_begin(h1, bcyl))*SECTOR_SIZE;
		ptbl_offset = (part_begin(h1, bcyl)-sectors)*SECTOR_SIZE;
                p1 = pte_offset(buffers[indx-1], 0);
                p2 = pte_offset(buffers[indx-1], 1);
                q1 = pte_offset(buffers[indx], 0);
                q2 = pte_offset(buffers[indx], 1);
                stt_sect = sectors;
                num_sect = part_size(h1, bcyl, ecyl);
                part_pte_set_logic(q1, PTE_HUGE, 
                                   h1, s1, bcyl,
                                   h2, s2, ecyl,
                                   stt_sect, num_sect);
                part_pte_clr(q2);
                prevh1 = 0;
                num_sect = get_numbr_sect(q1)+sectors;  
                stt_sect = (ptbl_offset-ext_offset)/SECTOR_SIZE;
                part_pte_set_logic(p2, PTE_EXTENDED,
                                   prevh1, s1, bcyl,
                                   h2, s2, ecyl,
                                   stt_sect, num_sect);
#ifdef	PART_PRINT
		printf("\n");
		printf(" Partition # (%d)\n", indx+1);
		printf(" Prev PTE {\n");
		printf(" p1->start = %d\n", get_start_sect(p1));
		printf(" p1->numbr = %d\n", get_numbr_sect(p1));
		printf(" p2->start = %d\n", get_start_sect(p2));
		printf(" p2->numbr = %d\n", get_numbr_sect(p2));
		printf(" Prev PTE }\n");
		printf(" Curr PTE {\n");
		printf(" q1->start = %d\n", get_start_sect(q1));
		printf(" q1->numbr = %d\n", get_numbr_sect(q1));
		printf(" Curr PTE }\n");
#endif
        }       
        part_exist_set(indx);
        part_changed_set(indx);
        part_flag_set(buffers[indx]);

        offsets[indx] = ptbl_offset;
	*daddr = part_offset;
        *dsize = out_size;
#ifdef	PART_PRINT
       	p = part_pte_find(indx, &ptbl_offset, &part_offset);
       	part_pte_print(indx, p, ptbl_offset, part_offset);
#endif
        return (0);
}

/*
 *---------------------------------------------------------------------------
 * part_pte_print()
 * This routine prints out the information in a partition table entry.
 *---------------------------------------------------------------------------
 */
void part_pte_print(int partn, pte_t *p, u_long ptbl_offset, u_long part_offset)
{
        u_long   part_size;
        u_long   part_endaddr;

        part_size    = p_numbr_sect(p);
        part_endaddr = part_offset+part_size;
	if (p->type != PTE_EXTENDED && debug_flag){
		printf("\n");
		printf(" mkfp: partition index = %d\n", partn+1);
		printf(" mkfp: partition type  = %d\n", p->type);
		printf(" mkfp: partition begin = %d cyl\n", get_bcyl(p));
		printf(" mkfp: partition end   = %d cyl\n", get_ecyl(p));
		printf(" mkfp: partition size  = %s\n", ground(part_size));

		printf(" mkfp: ptbl offset     = %d\n", ptbl_offset);
		printf(" mkfp: part offset     = %d\n", part_offset);
		printf(" mkfp: end  offset     = %d\n", part_endaddr);
	}
        return;
}

/*
 *---------------------------------------------------------------------------
 * part_data_sync()
 * This routine part_data_sync's all the data structures to disk.
 * (1) Deals with primary partitions.
 * (2) Deals with extended partitions (and logical volumes).
 * Basically, we need to write out all the buffers that contain partn tables.
 *---------------------------------------------------------------------------
 */
void    part_data_sync(void)
{
        int     partn;
        pte_t   *p;
        u_long   part_offset;
        u_long   ptbl_offset;

        ptbl_offset = 0;
        gseek(ptbl_offset);
        gwrite(buffer, SECTOR_SIZE);
        for (partn = 4; partn < part_count; partn++){
           if (exists[partn]){
                p = part_pte_find(partn, &ptbl_offset, &part_offset);
                gseek(ptbl_offset);
                gwrite(buffers[partn], SECTOR_SIZE);
           }
        }
        return;
}

/*
 *---------------------------------------------------------------------------
 * part_nextcyl_find_prime()
 * This routine is used to find the cylinder number at which the next
 * partition ought to start.
 *---------------------------------------------------------------------------
 */
u_short	part_nextcyl_find_prime(void)
{
	u_short	ncyl;
	pte_t	*p;
	u_long	d1, d2;

	if (part_count == 0){
		ncyl = 0;
		return (ncyl);
	}
	else {
		p = part_pte_find(part_count-1, &d1, &d2);
		ncyl = get_ecyl(p)+1;
		if (ncyl >= cylinders)
			return (OUT_OF_SPACE);
		return (ncyl);
	}
}

/*
 *-----------------------------------------------------------------------------
 * part_nextcyl_find_logic()
 * This routine is used to find the cylinder number at which the next
 * logical partition ought to start, within an existing extended partition.
 *-----------------------------------------------------------------------------
 */
u_short part_nextcyl_find_logic(void)
{
        pte_t   *p;
        int     indx;
        u_short ncyl;
        u_long  d1, d2;

        if (part_count == 4){
                /* Extended partition exists */
                /* No logical partition(s) exist */
                p = part_pte_find(part_count-1, &d1, &d2);
                ncyl = get_bcyl(p);
                return (ncyl);
        }
        else if (part_count > 4){
                /* Extended partition exists */
                /* Some logical partition(s) exist */
                p = part_pte_find(part_count-1, &d1, &d2);
                ncyl = get_ecyl(p)+1;
                if (ncyl > ext_ecyl)
                        return (OUT_OF_SPACE);
                return (ncyl);
        }
        return (OUT_OF_SPACE);

}

/*
 *---------------------------------------------------------------------------
 * part_changed_set()
 * This marks one of the buffers as dirty.
 * It's a function because one might have to do a few other things in addition
 * to marking this array, in the future.
 *---------------------------------------------------------------------------
 */
void	part_changed_set(int i)
{
	changed[i] = 1;
	return;
}

/*
 *----------------------------------------------------------------------------
 * part_changed_clr()
 * This routine clears all the elements in the changed array, which represents
 * if any of the buffers have been dirtied or not.
 *----------------------------------------------------------------------------
 */
void	part_changed_clr(void)
{
	int	partn;

	for (partn = 0; partn < MAX_PARTNS; partn++)
		changed[partn] = 0;
	return;
}

/*
 *---------------------------------------------------------------------------
 * part_exist_set()
 * This routine marks one partition as exists.
 *---------------------------------------------------------------------------
 */
void	part_exist_set(int i)
{
	exists[i] = 1;
	return;
}


/*
 *---------------------------------------------------------------------------
 * part_exist_clr()
 * This routine clears all the elemnts in the exists array, which represents
 * the existence of partitions.
 *---------------------------------------------------------------------------
 */
void	part_exist_clr(void)
{
        int     partn;

        for (partn = 0; partn < MAX_PARTNS; partn++)
                exists[partn] = 0;

	part_count = 0;

        return;
}

/*
 *----------------------------------------------------------------------------
 * part_pte_set_prime()
 * This routine is used to set a partition table entry with values.
 *----------------------------------------------------------------------------
 */
void	part_pte_set_prime(pte_t *p, u_char type, 
	             u_char h1, u_short s1, u_short c1, 
		     u_char h2, u_short s2, u_short c2)
{
	u_int	stt_sect;
	u_int	num_sect;

	p->type 	= type;
	p->sys_ind 	= 0;
	set_bhead(p, h1);
	set_ehead(p, h2);
	set_bsect(p, s1);
	set_esect(p, s2);
	set_bcyl(p, c1);
	set_ecyl(p, c2);

	stt_sect = part_begin(h1, c1);
	num_sect = part_size(h1, c1, c2);
	set_start_sect(p, stt_sect);
	set_numbr_sect(p, num_sect);
	return;
}

/*
 *----------------------------------------------------------------------------
 * part_pte_set_logic()
 * This routine is used to set a partition table entry with values.
 *----------------------------------------------------------------------------
 */
void    part_pte_set_logic(pte_t *p, u_char type,
                     u_char h1, u_short s1, u_short c1,
                     u_char h2, u_short s2, u_short c2,
                     u_int stt_sect, u_int num_sect)
{
        p->type         = type;
        p->sys_ind      = 0;
        set_bhead(p, h1);
        set_ehead(p, h2);
        set_bsect(p, s1);
        set_esect(p, s2);
        set_bcyl(p, c1);
        set_ecyl(p, c2);

        set_start_sect(p, stt_sect);
        set_numbr_sect(p, num_sect);
        return;
}


/*
 *-----------------------------------------------------------------------------
 * part_pte_clr()
 * This routine is used to clear a partition table entry.
 *-----------------------------------------------------------------------------
 */
void	part_pte_clr(pte_t *p)
{
	bzero(p, sizeof(pte_t));
	return;
}

/*
 *----------------------------------------------------------------------------
 * part_size_to_cyl()
 * This routine takes the starting cylinder, and a particular partition size
 * and computes what the ending cylinder ought to be.
 *----------------------------------------------------------------------------
 */
u_short part_size_to_cyl(u_long size, u_char h1, u_short c1)
{
        u_int   nsects;
        u_short c2;

        nsects = ceiling(size, SECTOR_SIZE);
        c2 = c1+ceiling((nsects-(heads-h1)*sectors), (heads*sectors));
        return (c2);
}

/*
 *----------------------------------------------------------------------------
 * set_bcyl()
 *----------------------------------------------------------------------------
 */
void	set_bcyl(pte_t *p, u_short cyl)
{
	p->bsect = p->bsect | (cyl & 0x0300) >> 2;
	p->bcyl  = (cyl & 0x00FF);
	return;
}

/*
 *----------------------------------------------------------------------------
 * set_ecyl()
 *----------------------------------------------------------------------------
 */
void    set_ecyl(pte_t *p, u_short cyl)
{
	p->esect = p->esect | (cyl & 0x0300) >> 2;
	p->ecyl  = (cyl & 0x00FF);
        return;
}

/*
 *----------------------------------------------------------------------------
 * get_bcyl()
 *----------------------------------------------------------------------------
 */
u_short	get_bcyl(pte_t *p)
{
	u_short	bcyl;

	bcyl = (p->bsect & 0xC0) << 2;
	bcyl = bcyl | p->bcyl;

	return (bcyl);
}

/*
 *----------------------------------------------------------------------------
 * get_ecyl()
 *----------------------------------------------------------------------------
 */
u_short get_ecyl(pte_t *p)
{
        u_short ecyl;

	ecyl = (p->esect & 0xC0) << 2;
	ecyl = ecyl | p->ecyl;
        return (ecyl);
}


/*
 *-----------------------------------------------------------------------------
 * set_bsect()
 *-----------------------------------------------------------------------------
 */
void	set_bsect(pte_t *p, u_short sect)
{
	p->bsect = sect;
	return;
}

/*
 *-----------------------------------------------------------------------------
 * set_esect()
 *-----------------------------------------------------------------------------
 */
void	set_esect(pte_t *p, u_short sect)
{
	p->esect = sect;
	return;
}

/*
 *-----------------------------------------------------------------------------
 * get_bsect()
 *-----------------------------------------------------------------------------
 */
u_short	get_bsect(pte_t *p)
{
	return (p->bsect & 0x3F);
}

/*
 *-----------------------------------------------------------------------------
 * get_esect()
 *-----------------------------------------------------------------------------
 */
u_short	get_esect(pte_t *p)
{
	return (p->esect & 0x3F);
}

