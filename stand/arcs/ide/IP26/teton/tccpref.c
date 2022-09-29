#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "libsc.h"
#include "uif.h"


#define ENTRIES		8

static int check_buf(unsigned char *buf, unsigned char ch);
void prefetch(unsigned long);
void __dcache_wb_inval(void*, int);

#ifdef DEBUG
void snapshot(void);
#endif


int
tcc_pref_test() {
    int errcount = 1;		/* default to error */
    volatile unsigned long *pre_ctrl = (unsigned long*)PHYS_TO_K1(TCC_PREFETCH);
    volatile unsigned long *pre_cam = (unsigned long*)PHYS_TO_K1(TCC_CAM0);
    volatile unsigned long *data[ENTRIES];
    unsigned char *big_buf = NULL, *buse;
    unsigned long ctrl, mask, cam, *use;
    int i;
 
    msg_printf(VRB, "TCC Prefetch test\n");

    /* clear out memory entries */
    for (i = 0; i < ENTRIES; i++)
	data[i] = NULL;

    do {
	/* allocate some memory - cache aligned */
	for (i = 0; i < ENTRIES; i++) {
	    if ((data[i] = (unsigned long *)dmabuf_malloc(sizeof(long))) == 
	      NULL) {
	   	msg_printf(DBG, "memory allocation failed\n");
	    	break;
	    }
	}


	/* TEST 1 - no depth */
	msg_printf(DBG, "  test 1 - prefetch without depth\n");
	/* default prefetch data */
	*pre_ctrl = PRE_INV | PRE_A_EN;
	prefetch(K1_TO_K0(data[0]));
	if (*pre_ctrl & (PRE_A_VALID | PRE_B_VALID)) {
	    msg_printf(DBG, "invalid prefetch load\n");
	    break;
	}



	/* TEST 2 - 5 depth, check fill */
	msg_printf(DBG, "  test 2 - prefetch with depth\n");

	/* initialize the prefetch */
	*pre_ctrl = PRE_INV | PRE_A_EN | PRE_B_EN | (5<<PRE_CAM_DEPTH_SHIFT); 

	/* prefetch 7 addresses */
	prefetch(K1_TO_K0(data[0]));
	prefetch(K1_TO_K0(data[1]));
	prefetch(K1_TO_K0(data[2]));
	prefetch(K1_TO_K0(data[3]));
	prefetch(K1_TO_K0(data[4]));
	prefetch(K1_TO_K0(data[5]));
	prefetch(K1_TO_K0(data[6]));

	/* now see if there data is correctly in the prefetch */
	ctrl = *pre_ctrl; 
	mask = PRE_A_VALID | PRE_B_VALID | PRE_CAM0_VALID | PRE_CAM1_VALID | 
	  PRE_CAM2_VALID | PRE_CAM3_VALID | PRE_CAM4_VALID; 
	if ((ctrl & mask) != mask) {
	    msg_printf(DBG, "prefetch failed\n");
	    break;
	}

	/* check A&B address registers and the CAM registers */
	cam = *pre_cam;
	for (i = 2; i < 7; i++, cam = *pre_cam) {
	    if (!(cam & PRE_VALID) || 
	      (cam & PRE_ADDRESS) != ((unsigned long)data[i] & PRE_ADDRESS)) {
		msg_printf(DBG, "invalid prefetch address (%d)\n", i);
		break;
	    }
	    pre_cam++;
	}
	if (i < 7)
	    break;

	/* check reg A */
        cam = *pre_cam++;
	if (!(cam & PRE_VALID) || 
	  (cam & PRE_ADDRESS) != ((unsigned long)data[0] & PRE_ADDRESS)) {
	    msg_printf(DBG, "invalid prefetch address (0)\n");
	    break;
	}

	/* check reg B */
        cam = *pre_cam;
	if (!(cam & PRE_VALID) || 
	  (cam & PRE_ADDRESS) != ((unsigned long)data[1] & PRE_ADDRESS)) {
	    msg_printf(DBG, "invalid prefetch address (1)\n");
	    break;
	}



	/* TEST 3 - load address from the prefetch into the cache */
	msg_printf(DBG, "  test 3 - prefetch->cache\n");

	/* load each of the addresses in the prefetch into the cache */
	/* by accessing the data */
	for (i = 0; i < 7; i++) {
	    use = (unsigned long *)K1_TO_K0(data[i]);
	    *use = 0xdeadbeafL;
	    __dcache_wb_inval((void*)use, sizeof(unsigned long));
	}

	/* since all of the addresses were loaded into the cache, the */
	/* prefetch will be empty */
	if (*pre_ctrl & mask) {
	    msg_printf(DBG, "cache load from prefetch failed\n");
	    break;
	}



	/* TEST 4 - age prefetch */
	msg_printf(DBG, "  test 4 - prefetch aging\n");

	/* initialize the prefetch w/ age */
	*pre_ctrl = PRE_INV | PRE_A_EN | (2<<PRE_CAM_DEPTH_SHIFT) |
	  (2<<PRE_TIMEOUT_SHIFT);

	/* prefetch 2 addresses */
	prefetch(K1_TO_K0(data[0]));
	prefetch(K1_TO_K0(data[1]));

	/* use the reset of the addresses as non-prefetched cache misses */
	for (i = 2; i < 7; i++) {
	    use = (unsigned long *)K1_TO_K0(data[i]);
	    __dcache_wb_inval((void*)use, sizeof(unsigned long));
	    *use = 0xdeadbeafL;
	}
	
	/* check to make sure everything is aged out */
	if ((*pre_ctrl & mask)) {
	    msg_printf(DBG, "prefetch aging failed\n");
	    break;
	}

	/* TEST 5 - test actual data */
	msg_printf(DBG, "  test 5 - check data buffer\n");

	/* set up prefetch with just reg A */
	*pre_ctrl = PRE_INV | PRE_A_EN | (2<<PRE_CAM_DEPTH_SHIFT);

	if ((big_buf = (unsigned char *)dmabuf_malloc(TCC_LINESIZE)) == NULL) {
	    msg_printf(DBG, "memory allocation failed\n");
	    break;
	}
	buse = (unsigned char *)K1_TO_K0(big_buf);

	/* check the data */
	if (check_buf(buse, 0x55) || check_buf(buse, 0xaa)) {
	    msg_printf(DBG, "reg A prefetch data invalid\n");
	    break;
	}
	
	/* set up prefetch with just reg B */
	*pre_ctrl = PRE_INV | PRE_B_EN | (2<<PRE_CAM_DEPTH_SHIFT);

	/* check the data */
	if (check_buf(buse, 0x55) || check_buf(buse, 0xaa)) {
	    msg_printf(DBG, "reg B prefetch data invalid\n");
	    break;
	}


	/* TEST COMPLETE - if this point is reached, all the tests passed */
	msg_printf(DBG, "  test complete\n");
	errcount = 0;

    } while(0);

    /* free the memory allocated */
    for (i = 0; i < ENTRIES; i++)
	if (data[i])
	    dmabuf_free((void*)data[i]);
    if (big_buf)
	dmabuf_free((void*)big_buf);

    /* write out passed/failed msg */
    if (errcount)
	msg_printf(VRB, "TCC Prefetch test failed\n");
    else
	msg_printf(VRB, "TCC Prefetch test passed\n");

    return errcount;
}


static int
check_buf(unsigned char *buf, unsigned char ch) {
    int i;

    /* fill the buffer */
    for (i = 0; i < TCC_LINESIZE; i++)
	buf[i] = ch;

    /* flush it from the cache */
    __dcache_wb_inval((void*)buf, TCC_LINESIZE);

    /* prefetch it */
    prefetch((unsigned long)buf);

    /* check it */
    for (i = 0; i < TCC_LINESIZE; i++)
	if (buf[i] != ch)
	    break;

    /* flush it from the cache */
    __dcache_wb_inval((void*)buf, TCC_LINESIZE);

    return(i < TCC_LINESIZE ? 1 : 0);
}


#ifdef DEBUG
static void
snapshot() {
    volatile unsigned long  *pre = (unsigned long*)PHYS_TO_K1(TCC_PREFETCH);
    unsigned long f1;
    f1 = *pre;

    pre = (unsigned long*)PHYS_TO_K1(TCC_CAM0);
    msg_printf(DBG, "----\nPrefetch register Snapshot\n");
    if (f1 & PRE_CAM0_VALID)
	msg_printf(DBG, "  CAM 0 - 0x%0lx\n", *pre);
    pre++;
    if (f1 & PRE_CAM1_VALID)
    	msg_printf(DBG, "  CAM 1 - 0x%0lx\n", *pre);
    pre++;
    if (f1 & PRE_CAM2_VALID)
    	msg_printf(DBG, "  CAM 2 - 0x%0lx\n", *pre);
    pre++;
    if (f1 & PRE_CAM3_VALID)
    	msg_printf(DBG, "  CAM 3 - 0x%0lx\n", *pre);
    pre++;
    if (f1 & PRE_CAM4_VALID)
    	msg_printf(DBG, "  CAM 4 - 0x%0lx\n", *pre);
    pre++;
    if (f1 & PRE_A_VALID) {
    	msg_printf(DBG, "  Address A Register - 0x%0lx\n", *pre);
	msg_printf(DBG, "                 age - 0x%lx\n", (f1 & PRE_A_AGE)>>20);
    }
    pre++;
    if (f1 & PRE_B_VALID) {
    	msg_printf(DBG, "  Address B Register - 0x%0lx\n", *pre);
	msg_printf(DBG, "                 age - 0x%lx\n", (f1 & PRE_B_AGE)>>24);
    }
    msg_printf(DBG, "  Control Register - 0x%lx\n----\n", f1 & 0x3ffL);
}
#endif


