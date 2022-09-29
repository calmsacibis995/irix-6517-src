/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:48:30 $
 * -------------------------------------------------------------
 */
 	
#include <sys/regdef.h>
#include <inttypes.h>
#include <post1supt.h>

#define NoDIAG

extern void DPRINTF (char *, ...) ;
extern int mace_serial_test(void);

/*
 *  Testtable, for every valid diags need to have an entry in the
 *  following table, and endOFdiags must be the LAST one in the
 *  table.
 */
void setUPmh (void) ;
int  mace_serial_test (void) ;
int  endOFdiags (void) ;
static int
(*testtable[])() = {
  mace_serial_test,
  /* We can not run following test yet.
  mem_scache1, 
  mem_scache2, 
  mem_scache3, 
  mem_scache4, 
  mem_scache5, 
  mem_scache6, 
  mem_scache7, 
  mem_scache8, 
  mem_scache9, 
  mem_scache91,
  mem_scache10,
  */
  endOFdiags
} ;


/*
 *
 *  test name:      mem_scache1.s
 *  descriptions:   Cache read miss, no primary cache dirty line write
 *                  back, verify that data come from memory and second
 *                  cache also get updated.
 */
int
mem_scache1 ()
{
    uint32_t    taglo, lineaddr ;
    uint64_t    data, expected_data ;
    int32_t     i ;

    DPRINTF ("<post1> <mem_scache1> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_init (TESTADDR, 3, 0x5a5a5a5a) ;
    pcache_init (TESTADDR, 3, 0xa5a5a5a5) ;
    mem_init    (TESTADDR, 3, 0xdeadbeef) ;

    for (lineaddr = TESTADDR, i = 3 ; i > 0 ; i--) {
        pcache_hit_invalid (KSEG0|lineaddr) ;
        mtc0_taglo ((lineaddr >> 19) << 15) ;
        scache_index_store_tag (KSEG0|lineaddr) ;
        lineaddr += 0x20 ;
    }
    
    /*
     * [01] Initialize the memory location.
     */
    for (lineaddr = TESTADDR + 8, i = 3 ; i > 0 ; i--) {
        data = ~lineaddr ;
        expected_data = (data << 32) | lineaddr ;
        data = * (uint64_t *) (KSEG0 | lineaddr) ;
        if (data != expected_data) diag_error_exit () ;
        lineaddr += 0x20 ;
    }
    
    /*
     * [02] Verify primary/secondary/memory.
     */
    pcache_verify (TESTADDR, 3, 0xdeadbeef) ;
    scache_verify (TESTADDR, 2, 0xdeadbeef) ;
    mem_verify    (TESTADDR, 2, 0xdeadbeef) ;
    
    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache1> Leaving test\n") ;
    return 1 ; 
}

/*
 *  test name:      mem_scache2.c
 *  descriptions:   Cache read miss, primary cache dirty line write
 *                  back, verify that data come from memory and second
 *                  cache also will have the data from the primary cache
 *                  write back data (dirty line).
 *                  (The primary write back line and secondary read miss
 *                   line are not alias line in the secondary cache).
 *  Note:           bit 13 in the VA determind which set of the primary cache
 *                  the alias addess should replace.
 */
int
mem_scache2 ()
{
    uint64_t    data, expected_data ;
    
    DPRINTF ("<post1> <mem_scache2> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (ALIASADDR, 1, STESTKEY1) ;
    scache_init ((TESTADDR|PCACHESIZE), 1, STESTKEY2) ;
    scache_init ((TESTADDR|PSET), 1, STESTKEY3) ;
        
    pcache_init ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
    
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_init    ((TESTADDR|PCACHESIZE), 1, MTESTKEY4) ;

    /*
     * [01] Read to ALIASADDR, a read miss which cause a primary cache
     *      dirty line write back - BUT don't know which line get replaced.
     */
    data = ~(TESTADDR ^ MTESTKEY1) ;
    expected_data = (data << 32) | (TESTADDR ^ MTESTKEY1) ;
    data = * (uint64_t *) (TESTADDR|KSEG0) ;
    if (data != expected_data) diag_error_exit () ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify (TESTADDR, 1, MTESTKEY1) ;         /* data from memory    */
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;  /* nothing changed     */
    
    scache_verify (TESTADDR, 1, MTESTKEY1) ;         /* data from memory    */
    scache_verify ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ; /* writeback line */
    scache_verify ((TESTADDR|PSET), 1, STESTKEY3) ; /* writeback line */

    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_verify    ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    
    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache2> Leaving test\n") ;
    return 1 ; 
}


/*
 *  test name:      mem_scache3.c
 *  descriptions:   Cache read miss, primary cache dirty line write
 *                  back, verify that data come from memory and second
 *                  cache also will have the data from the primary cache  
 *                  write back data (dirty line).
 *  Note:           bit 13 in the VA determind which set of the primary cache
 *                  the alias addess should replace.
 */
int
mem_scache3 ()
{
    uint64_t    data, expected_data ;
    
    DPRINTF ("<post1> <mem_scache3> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_init (TESTADDR, 1, STESTKEY1) ;
    pcache_init (TESTADDR, 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;

    /*
     * [01] Read to ALIASADDR, a read miss which cause a primary cache
     *      dirty line write back - BUT don't know which line get replaced.
     */
    data = ~ALIASADDR ;
    expected_data = (data << 32) | ALIASADDR ;
    data = * (uint64_t *) (ALIASADDR|KSEG0) ;
    if (data != expected_data) diag_error_exit () ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    pcache_verify (ALIASADDR, 1, MTESTKEY2) ;
    scache_verify (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    (TESTADDR, 1, PTESTKEY1) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache3> Leaving test\n") ;
    return 1 ; 
}


/*
 *  test name:      mem_scache4.c
 *  descriptions:   Secondary cache read hit, primary cache read miss.
 *                  no primary cache dirty line write back.
 *                  verify that data from secondary cache not from memory
 */
int
mem_scache4 ()
{
    uint64_t    data, expected_data ;
    
    DPRINTF ("<post1> <mem_scache4> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_init (TESTADDR, 1, STESTKEY1) ;
    
    pcache_hit_invalid (TESTADDR|KSEG0) ;
    pcache_hit_invalid (TESTADDR|PSET|KSEG0) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;

    /*
     * [01] Read to ALIASADDR, a read miss which cause a primary cache
     *      dirty line write back - BUT don't know which line get replaced.
     */
    data = ~(TESTADDR ^ STESTKEY1) ;
    expected_data = (data << 32) | (TESTADDR ^ STESTKEY1) ;
    data = * (uint64_t *) (TESTADDR|KSEG0) ;
    if (data != expected_data) diag_error_exit () ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify (TESTADDR, 1, STESTKEY1) ;
    
    scache_verify (TESTADDR, 1, STESTKEY1) ;
    scache_verify (ALIASADDR, 1, STESTKEY2) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache4> Leaving test\n") ;
    return 1 ; 
}


/*
 *  test name:      mem_scache5.c
 *  descriptions:   Secondary cache write hit, no primary cache dirty
 *                  line write back,
 *                  verify that the write data goes to both secondary cache
 *                  and memory.
 *  Note:           bit 13 in the VA determind which set of the primary cache
 *                  the alias addess should replace.
 */
int
mem_scache5 ()
{
    uint32_t    taglo, expected_taglo ;
    
    
    DPRINTF ("<post1> <mem_scache5> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (TESTADDR, 1, STESTKEY1) ;
    scache_init ((TESTADDR|PSET), 1, STESTKEY2) ;
        
    pcache_init (TESTADDR, 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
    
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;

    /*
     * [01] flush the testaddr cache line in primary cache to cause a 
     *      block write which hit the primary cache,
     */
    pcache_hit_wb_invalid (TESTADDR|KSEG0) ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_index_load_tag (KSEG0|TESTADDR) ;
    taglo = mfc0_taglo () ;
    if ((taglo & 0xc0) != 0x0) diag_error_exit () ;
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;  /* nothing changed     */
    
    scache_verify (TESTADDR, 1, PTESTKEY1) ;         /* data from pcache    */
    scache_verify ((TESTADDR|PSET), 1, STESTKEY2) ; /* writeback line */

    mem_verify    (TESTADDR, 1, PTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    
    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache5> Leaving test\n") ;
    return 1 ; 
}
 


/*
 *  test name:      mem_scache6.c
 *  descriptions:   Secondary cache read hit, primary cache read miss.
 *                  primary cache dirty line write back.
 *                  primary write back alias line in secondary cache.
 *                  verify that data from secondary cache not from memory
 */
int
mem_scache6 ()
{
    uint64_t    data, expected_data ;
    
    DPRINTF ("<post1> <mem_scache6> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (TESTADDR, 1, STESTKEY1) ;
    scache_init ((TESTADDR|PCACHESIZE), 1, STESTKEY2) ;

    pcache_init ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PCACHESIZE), 1, MTESTKEY3) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY4) ;

    /*
     * [01] Read to ALIASADDR, a read miss which cause a primary cache
     *      dirty line write back - BUT don't know which line get replaced.
     */
    data = ~(TESTADDR ^ STESTKEY1) ;
    expected_data = (data << 32) | (TESTADDR ^ STESTKEY1) ;
    data = * (uint64_t *) (TESTADDR|KSEG0) ;
    if (data != expected_data) diag_error_exit () ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify (TESTADDR, 1, STESTKEY1) ;
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    
    scache_verify (TESTADDR, 1, STESTKEY1) ;
    scache_verify ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    
    mem_verify    ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY4) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache6> Leaving test\n") ;
    return 1 ;
}
 


/*
 *  test name:      mem_scache7.c
 *  descriptions:   SysAD block read hit secondary cache.
 *                  primary cache dirty line write back.
 *                  the block read missed line and primary write back line
 *                  are alias in secondary cache.
 *                  verify that primary cache got the data from secondary
 *                  cache, and the write back data in both secondary cache
 *                  and memory,
 */
int
mem_scache7 ()
{
    uint64_t    data, expected_data ;
    
    DPRINTF ("<post1> <mem_scache7> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (TESTADDR, 1, STESTKEY1) ;

    pcache_init (ALIASADDR, 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;

    /*
     * [01] cause a block read to SysAD which bit the secondary cache.
     *      dirty line write back - BUT don't know which line get replaced.
     */
    data = ~(TESTADDR ^ STESTKEY1) ;
    expected_data = (data << 32) | (TESTADDR ^ STESTKEY1) ;
    data = * (uint64_t *) (TESTADDR|KSEG0) ;
    if (data != expected_data) diag_error_exit () ;
    
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify (TESTADDR, 1, STESTKEY1) ;
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    
    scache_verify (ALIASADDR, 1, MTESTKEY1) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, PTESTKEY1) ;
    mem_verify    ((TESTADDR|PSET), 1, PTESTKEY2) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache7> Leaving test\n") ;
    return 1 ;
}
 



/*
 *  test name:      mem_scache8.c
 *  descriptions:   Primary cache write miss, a block read to SysAD, 
 *                  which cause a secondary cache read miss, 
 *                  no primary cache dirty line write back.
 *                  verify that primary cache got the data from secondary
 *                  cache, and the write back data in both secondary cache
 *                  and memory,
 */
int
mem_scache8 ()
{
    uint64_t    data, expected_data ;
    uint32_t    testaddr, taglo, expected_taglo ;
    int32_t     i ;
    
    DPRINTF ("<post1> <mem_scache8> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (ALIASADDR, 1, STESTKEY1) ;

    pcache_init (TESTADDR, 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;

    /*
     * [01] A primary cache write miss, cause a block read to SysAD, 
     *      a block read miss on secondary cache.
     */
    pcache_hit_invalid (KSEG0|TESTADDR) ;
    data = ~(TESTADDR ^ TESTKEY) ;
    data = (data << 32) | (TESTADDR ^ TESTKEY) ;
    * (uint64_t *) (KSEG0|TESTADDR) = data ;

    pcache_index_load_tag (KSEG0|TESTADDR) ;
    taglo = mfc0_taglo () ;
    expected_taglo = (((KSEG0|TESTADDR) >> 12) << 8) | 0xc0 ;
    taglo = (taglo >> 6) << 6 ;
    if (expected_taglo != taglo) diag_error_exit () ;
    
    for (testaddr = TESTADDR, i = 0; i < 4 ; i++) {
        if (i != 0x0) {
            data = ~(testaddr ^ MTESTKEY1) ;
            expected_data = (data << 32) | (testaddr ^ MTESTKEY1) ;
        } else {
            data = ~(testaddr ^ TESTKEY) ;
            expected_data = (data << 32) | (testaddr ^ TESTKEY) ;
        }
        data = * (uint64_t *) (KSEG0|testaddr) ;
        if (data != expected_data) diag_error_exit () ;
        testaddr += 0x8 ;
    }
            
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    scache_verify (TESTADDR, 1, MTESTKEY1) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, PTESTKEY1) ;
    mem_verify    ((TESTADDR|PSET), 1, PTESTKEY2) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache8> Leaving test\n") ;
    return 1 ;
}
 


/*
 *  test name:      mem_scache9.c
 *  descriptions:   Primary cache write miss, a block read to SysAD, 
 *                  which cause a secondary cache read miss, 
 *                  primary cache dirty line write back.
 *                  the write back line miss the secondary cache.
 */
int
mem_scache9 ()
{
    uint64_t    data, expected_data ;
    uint32_t    testaddr, taglo, expected_taglo ;
    int32_t     i ;
    
    DPRINTF ("<post1> <mem_scache9> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (ALIASADDR, 1, STESTKEY1) ;

    pcache_init ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_init    ((TESTADDR|PCACHESIZE), 1, MTESTKEY4) ;

    /*
     * [01] A primary cache write miss, cause a block read to SysAD, 
     *      a block read miss on secondary cache.
     */
    data = ~(TESTADDR ^ TESTKEY) ;
    data = (data << 32) | (TESTADDR ^ TESTKEY) ;
    * (uint64_t *) (KSEG0|TESTADDR) = data ;

    pcache_index_load_tag (KSEG0|TESTADDR) ;
    taglo = mfc0_taglo () ;
    expected_taglo = (((KSEG0|TESTADDR) >> 12) << 8) | 0xc0 ;
    taglo = (taglo >> 6) << 6 ;
    if (expected_taglo != taglo) diag_error_exit () ;
    
    for (testaddr = TESTADDR, i = 0; i < 4 ; i++) {
        if (i != 0x0) {
            data = ~(testaddr ^ MTESTKEY1) ;
            expected_data = (data << 32) | (testaddr ^ MTESTKEY1) ;
        } else {
            data = ~(testaddr ^ TESTKEY) ;
            expected_data = (data << 32) | (testaddr ^ TESTKEY) ;
        }
        data = * (uint64_t *) (KSEG0|testaddr) ;
        if (data != expected_data) diag_error_exit () ;
        testaddr += 0x8 ;
    }
            
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    scache_verify (TESTADDR, 1, MTESTKEY1) ;
    scache_verify ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_verify    ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache9> Leaving test\n") ;
    return 1 ;
}
 
/*
 *  test name:      mem_scache91.c
 *  descriptions:   Primary cache write miss, a block read to SysAD, 
 *                  which cause a secondary cache read miss, 
 *                  primary cache dirty line write back.
 *                  the write back line hit the secondary cache.
 */
int
mem_scache91 ()
{
    uint64_t    data, expected_data ;
    uint32_t    testaddr, taglo, expected_taglo ;
    int32_t     i ;
    
    DPRINTF ("<post1> <mem_scache91> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (ALIASADDR, 1, STESTKEY1) ;
    scache_init ((TESTADDR|PCACHESIZE), 1, STESTKEY2) ;

    pcache_init ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    pcache_init ((TESTADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_init    ((TESTADDR|PCACHESIZE), 1, MTESTKEY4) ;

    /*
     * [01] A primary cache write miss, cause a block read to SysAD, 
     *      a block read miss on secondary cache.
     */
    data = ~(TESTADDR ^ TESTKEY) ;
    data = (data << 32) | (TESTADDR ^ TESTKEY) ;
    * (uint64_t *) (KSEG0|TESTADDR) = data ;

    pcache_index_load_tag (KSEG0|TESTADDR) ;
    taglo = mfc0_taglo () ;
    expected_taglo = (((KSEG0|TESTADDR) >> 12) << 8) | 0xc0 ;
    taglo = (taglo >> 6) << 6 ;
    if (expected_taglo != taglo) diag_error_exit () ;
    
    for (testaddr = TESTADDR, i = 0; i < 4 ; i++) {
        if (i != 0x0) {
            data = ~(testaddr ^ MTESTKEY1) ;
            expected_data = (data << 32) | (testaddr ^ MTESTKEY1) ;
        } else {
            data = ~(testaddr ^ TESTKEY) ;
            expected_data = (data << 32) | (testaddr ^ TESTKEY) ;
        }
        data = * (uint64_t *) (KSEG0|testaddr) ;
        if (data != expected_data) diag_error_exit () ;
        testaddr += 0x8 ;
    }
            
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify ((TESTADDR|PSET), 1, PTESTKEY2) ;
    scache_verify (TESTADDR, 1, MTESTKEY1) ;
    scache_verify ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, MTESTKEY2) ;
    mem_verify    ((TESTADDR|PSET), 1, MTESTKEY3) ;
    mem_verify    ((TESTADDR|PCACHESIZE), 1, PTESTKEY1) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache91> Leaving test\n") ;
    return 1 ;
}



/*
 *  test name:      mem_scache10.c
 *  descriptions:   Primary cache write miss, a block read to SysAD, 
 *                  which cause a secondary cache read miss, 
 *                  primary cache dirty line write back.
 *                  the write back line miss the secondary cache.
 */
int
mem_scache10 ()
{
    uint64_t    data, expected_data ;
    uint32_t    testaddr, taglo, expected_taglo ;
    int32_t     i ;
    
    DPRINTF ("<post1> <mem_scache10> Entering test\n") ;
#if !defined(NoDIAG)
    /*
     * [00] Initialize the primary/secondary/memory
     *      <1> proimary cache invalid.
     *      <2> secondary cache invalid.
     */
    scache_clear_sequence (0x0) ;
    scache_init (ALIASADDR, 1, STESTKEY1) ;

    pcache_init (ALIASADDR, 1, PTESTKEY1) ;
    pcache_init ((ALIASADDR|PSET), 1, PTESTKEY2) ;
        
    mem_init    (TESTADDR, 1, MTESTKEY1) ;
    mem_init    (ALIASADDR, 1, MTESTKEY2) ;
    mem_init    ((ALIASADDR|PSET), 1, MTESTKEY3) ;

    /*
     * [01] A primary cache write miss, cause a block read to SysAD, 
     *      a block read miss on secondary cache.
     */
    data = ~(TESTADDR ^ TESTKEY) ;
    data = (data << 32) | (TESTADDR ^ TESTKEY) ;
    * (uint64_t *) (KSEG0|TESTADDR) = data ;

    pcache_index_load_tag (KSEG0|TESTADDR) ;
    taglo = mfc0_taglo () ;
    expected_taglo = (((KSEG0|TESTADDR) >> 12) << 8) | 0xc0 ;
    taglo = (taglo >> 6) << 6 ;
    if (expected_taglo != taglo) diag_error_exit () ;
    
    for (testaddr = TESTADDR, i = 0; i < 4 ; i++) {
        if (i != 0x0) {
            data = ~(testaddr ^ MTESTKEY1) ;
            expected_data = (data << 32) | (testaddr ^ MTESTKEY1) ;
        } else {
            data = ~(testaddr ^ TESTKEY) ;
            expected_data = (data << 32) | (testaddr ^ TESTKEY) ;
        }
        data = * (uint64_t *) (KSEG0|testaddr) ;
        if (data != expected_data) diag_error_exit () ;
        testaddr += 0x8 ;
    }
            
    /*
     * [02] We assume here that bit<13> of the virtual address determind
     *      which set to be replaced. 
     */
    pcache_verify ((ALIASADDR|PSET), 1, PTESTKEY2) ;
    scache_verify (ALIASADDR, 1, PTESTKEY1) ;
    
    mem_verify    (TESTADDR, 1, MTESTKEY1) ;
    mem_verify    (ALIASADDR, 1, PTESTKEY1) ;
    mem_verify    ((ALIASADDR|PSET), 1, PTESTKEY1) ;

    /*
     * [03] We done.
     */
#endif  /* NoDIAG */
    DPRINTF ("<post1> <mem_scache10> Leaving test\n") ;
    return 1 ;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/




/*
 *  Function name: endOFdiags
 *  Description:   return 1 to indicate that the diags exec 
 *                 environment is in place.
 */
void
setUPmh (void)
{
  register uint64_t  CrimeREG ;
  register uint32_t  addrptr, tmp ;
  register int       indx, enty ; 
    
  /*
   * [1] Disable processor interrupt.
   */
  CPUinterruptOFF (0x0, 0x0, 0x0) ;
  
  /*
   * [2] Turn off ECC checking.
   */
  CrimeREG = (uint64_t) (MEMcntlBASE|MEMCntl) ;
  * (uint64_t *) CrimeREG = 0x0 ;
  
  /*
   * [3] Turn off interrupts in Crime.
   */
  CrimeREG = (uint64_t) CrimeBASE ;
  * (uint64_t *) (CrimeREG|CrimeIntrEnable) = 0 ; 
  * (uint64_t *) (CrimeREG|CrimeSoftIntr)   = 0 ; 
  * (uint64_t *) (CrimeREG|CrimeHardIntr)   = 0 ; 
  * (uint64_t *) (CrimeREG|CPUErrENABLE)    = 0 ; 

  /*
   * [4] Invalidate all tlb entries.
   */
  addrptr = 0x0 ;
  tmp = 0x15 ; 
  enty = TLBentries (0x0) ;
  mtc0_pagemask(0x0) ;      /* setup 4K pages.  */
  for (indx = 0 ; indx < enty ; indx += 1) {
    mtc0_index (indx) ;
    mtc0_entryhi  (addrptr) ;
    mtc0_entrylo0 (tmp)  ;
    mtc0_entrylo1 (tmp += 0x40) ; 
    tlb_write () ;
    addrptr += 0x2000 ;
    tmp += 0x40 ; 
  }

  /*
   * [5] Invalidate all cache entries (primary/secondary caches.)
   */

  /* Do primary caches */
  enty    = (int) pcachesize (0x0) ;
  for (indx = 0 ; indx < enty ; indx += 0x20) {
    addrptr = (uint32_t)indx | KSEG0 ;
    icache_idx_invalidate (addrptr) ;
    mtc0_taglo ((addrptr >> 12) << 8) ;
    pcache_index_store_tag (addrptr) ;
  }
  
  /* Do secondary cache.            */
  /* first make sure it is enabled. */
  tmp = mfc0_config () ;
  if ((tmp & SC) != 0x0) {
    mtc0_config (tmp | SE) ;
    scache_clear_sequence (0x0) ;
    mtc0_config (tmp & ~SE) ;
  }

  /*
   * [6] We done.
   */
  DPRINTF ("<post1> <setUPmh> setting up diags exec environment\n") ;
  return ;
}

/*
 *  Function name: endOFdiags
 *  Description:   return -1 to indicate this is the end of the 
 *                 post1 testtable.
 */
int
endOFdiags (void)
{
  /*
   * Return -1 to indicate this is the end of post1diags.
   */
  DPRINTF ("<post1> <endOFdiags> This is the end of post1\n") ;
  return -1 ;
}

/*
 *  Function name: post1diags
 *  Description:   dispatch each diags listed in the testtable.
 */
int
post1diags (void)
{
  int   result, diags_idx = 0 ;

  DPRINTF ("<post1> <post1diags> .......\n") ;
  
  /*
   * [1] Exec all the diags listed in the testtable, - 
   */
  do {
    setUPmh () ;
    DPRINTF ("<post1> <post1diags> diags%d\n", diags_idx) ;
    result = (*testtable[diags_idx])() ;
    diags_idx = diags_idx + 1 ;
  }  while (result != -1) ;  /* We can do some fancy stuff like loop on test
                                loop on error here, but ... */

  /*
   * [2] Oh boy, finally we have got through post1, clean up the state
   *     again.
   */
  setUPmh () ;
  return 1 ; 
}

/*
 *  Function name: pcache_init (paddr, lines)
 *  Description:   Initialize the primary cache to it address.
 */
void
pcache_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, taglo, tmp ;
    uint64_t    data ;

    DPRINTF ("<post1> <pcache_init> Enter routine.\n") ;
    
    for (Paddr = paddr, count = 0 ; count < lines ; Paddr += 0x20, count++) {
      mtc0_taglo ((Paddr >> 12) << 8) ; 
      pcache_index_store_tag (KSEG0|Paddr) ;
      pcache_create_dirty_exclusive (KSEG0|Paddr) ;
      dword = 0 ;
      while (dword < 0x20) {
        testaddr = Paddr | dword ; 
        data = (uint64_t) (~(testaddr ^ key)) ;
        data = (data << 32) | (testaddr ^ key) ;
        * (uint64_t *) (KSEG0|testaddr) = data ;
        dword += 0x8 ;
      }
    }
    DPRINTF ("<post1> <pcache_init> Leaving routine.\n") ;
}


/*
 *  Function name: caches_init (paddr, lines)
 *  Description:   Initialize the primary/seconadry cache to it address.
 */
void
scache_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    uint32_t    Paddr ;
    uint64_t    data ;
    int32_t     count ;

    DPRINTF ("<post1> <scache_init> Enter routine.\n") ;
    pcache_init (paddr, lines, key) ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        pcache_hit_wb_invalid (KSEG0|Paddr) ;
        Paddr += 0x20 ; 
    }
    DPRINTF ("<post1> <scache_init> Leaving routine.\n") ;
}


/*
 *  Function name: mem_init (paddr, lines)
 *  Description:   Initialize the memory without destroy caches.
 */
void
mem_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count ;
    uint32_t    Paddr, dword, testaddr ;
    uint64_t    data ;

    DPRINTF ("<post1> <mem_init> Enter routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            data = (uint64_t) (~(testaddr ^ key)) ;
            data = (data << 32) | (testaddr ^ key) ;
            * (uint64_t *) (KSEG1|testaddr) = data ;
            dword += 0x8 ;
        }
    }
    DPRINTF ("<post1> <mem_init> Leaving routine.\n") ;
}


/*
 *  Function name: pcache_verify (paddr, lines)
 *  Description:   Verify the cache line with its unique data pattern.
 */
void
pcache_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, taglo, expected_taglo ;
    uint64_t    expected_data, data ;

    DPRINTF ("<post1> <pcache_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        pcache_index_load_tag (KSEG0|Paddr) ;
        expected_taglo = ((Paddr >> 12) << 8) | 0xc0 ;
        taglo = (mfc0_taglo() >> 5) << 5 ;
        if (taglo != expected_taglo) diag_error_exit () ;
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG0|testaddr) ;
            if (data != expected_data) diag_error_exit () ;
            dword += 0x8 ;
        }
    }
    DPRINTF ("<post1> <pcache_verify> Leaving routine.\n") ;
}


/*
 *  Function name: scache_verify (paddr, line, key)
 *  Description:   verify the secondary cache line without destroy
 *                 the memory content.
 */
void
scache_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, expected_taglo ;
    uint64_t    expected_data, data ;

    DPRINTF ("<post1> <scache_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        scache_index_load_tag (KSEG0|Paddr) ;
        expected_taglo = ((Paddr >> 19) << 15) | 0x1000 ;
        if (mfc0_taglo() != expected_taglo) diag_error_exit () ;
        mtc0_taglo ((Paddr >>12) << 8) ;
        pcache_index_store_tag (KSEG0|Paddr) ;
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG0|testaddr) ;
            if (data != expected_data) diag_error_exit () ;
            dword += 0x8 ;
        }
    }
    DPRINTF ("<post1> <scache_verify> Leaving routine.\n") ;
}


/*
 *  Function name: mem_verify (paddr, line, key)
 *  Description:   verify the memory 
 */
void
mem_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr ;
    uint64_t    expected_data, data ;

    DPRINTF ("<post1> <mem_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG1|testaddr) ;
            if (data != expected_data) diag_error_exit () ;
            dword += 0x8 ;
        }
    }
    DPRINTF ("<post1> <mem_verify> Leaving routine.\n") ;
}
    

/*
 * -------------------------------------------------------------
 *
 * $Log: post1diags.c,v $
 * Revision 1.1  1997/08/18 20:48:30  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.4  1995/12/30  04:38:49  kuang
 * Added mace_serial_test and commented out all secondary cache tests
 *
 * Revision 1.3  1995/12/28  02:01:37  mwang
 * Fixed compile problem: mace_serial_test was undefined.
 *
 * Revision 1.2  1995/12/26  22:24:14  philw
 * add serial port test to post1
 *
 * Revision 1.1  1995/11/28  02:17:00  kuang
 * General cleanup
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
