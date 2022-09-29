/**************************************************************************
 *
 *  File:  hello_tune.c
 *
 *  Description:
 *
 *      RAD audio support for the boot tune etc...
 *
 **************************************************************************
 *          Copyright (C) 1995 Silicon Graphics, Inc.
 *
 *  These coded instructions, statements, and computer programs  contain
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and
 *  are protected by Federal copyright law.  They  may  not be disclosed
 *  to  third  parties  or copied or duplicated in any form, in whole or
 *  in part, without the prior written consent of Silicon Graphics, Inc.
 *
 *
 **************************************************************************/
/*
 * $Id: hello_tune.c,v 1.21 1999/12/01 21:12:59 mnicholson Exp $
 */

#if !NOGFX && !NOBOOTTUNE

#include "sys/types.h"
#include <sys/param.h>
#include <sys/RACER/IP30addrs.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>
#include "sys/rad.h"
#include "adpcm.h"
#include <sys/RACER/sflash.h>
#include <sys/RACER/IP30nvram.h>


struct adpcm_state pcm_state;

int	setup_tune(int* pbuf, int tunetype);

#if HEART_COHERENCY_WAR
#define __dcache_wb_inval(a, l)		heart_dcache_wb_inval(a,l)
#define __dcache_wb(a, l)		heart_dcache_wb_inval(a,l)
#else
#define	__dcache_wb_inval(a,b)
#define	__dcache_wb_wb(a,b)
#endif	/* HEART_COHERENCY_WAR */

#if DEBUG
#define DPRINTF(x)		printf x
#else
#define DPRINTF(x)
#endif

/*
 * the ADPCM tune samples are in samp.c
 */
extern unsigned char IP30starttune[];
extern int           IP30starttune_size;
extern unsigned char IP30stoptune[];
extern int           IP30stoptune_size;
extern unsigned char IP30badgfxtune[];
extern int           IP30badgfxtune_size;

#define starttune       IP30starttune
#define starttune_size  IP30starttune_size
#define stoptune        IP30stoptune
#define stoptune_size   IP30stoptune_size
#define badgfxtune      IP30badgfxtune
#define badgfxtune_size IP30badgfxtune_size


int	prad_probe(void);
void	prad_mute_relay(void);
void	prad_unmute_relay(void);
int	prad_setup(int);
int	prad_setup_dtoa( __psunsigned_t bufptr, unsigned int bufsize );
void	prad_start_dtoa_dma(void);
int	prad_drain_dtoa_dma(int);

static int *
get_tune_mem(void)
{
    int *bufp;
#ifdef ENETBOOT
    extern int 	tune_file_size;
    extern int *tune_buf;

    if (tune_file_size)
	bufp = tune_buf;
    else
	bufp = (int *) PHYS_TO_K0(IP30_SCRATCH_MEM);

#else /* ENETBOOT */

    bufp = (int *) PHYS_TO_K0(IP30_SCRATCH_MEM);

#endif /* ENETBOOT */

    DPRINTF(("play tune buffer @ 0x%x\n", bufp));
    return((int *)bufp);
}

/*
 * tune_type:   0=hello, 1=goodbye, 2=graphics failure
 */
void 
play_hello_tune(int tune_type)
{

    int			*tunebuffer;
    int			buflen;
    int			volume;
    int			boottune;
    char		*p;

    /*
     * Should not play the boottune at all if:
     *   1. there is no audio
     *   2. the volume parameter is equal to 0
     */

    if ( prad_probe() != 0 )
	return;

    p = cpu_get_nvram_offset(NVOFF_VOLUME,NVLEN_VOLUME);
    atob(p,&volume);
    if (volume == 0) 
	return;

    /* The RAD hw has its output neutral (no gain/no attenuation) at 192
     * in the volume control register.
     * The default volume env variable setting in PROM for desktops is 80.
     * So for volume 0 ~ 80, scale from 0 ~ 192, for values 81 ~ 255
     * scale 193 ~ 255 
     */
    if (volume <= 80) {
	volume = (volume * 192)/80;
    } else 
	volume = 192 + ((255-193) * (volume - 80)) / (255 - 81);

    p = cpu_get_nvram_offset(NVOFF_BOOTTUNE,NVLEN_BOOTTUNE);
    atob(p,&boottune);
    if (boottune == 0)
	return;

    /*
     * setup the RAD
     *
     * XXX still need to setup the volume 
     */
    prad_setup(volume);

    /*
     * get some memory
     */
    tunebuffer = get_tune_mem();
    if (!tunebuffer)
	return;

    /*
     * setup the tune
     */

    if ( (buflen = setup_tune(tunebuffer, tune_type)) == 0 )
	return;

    __dcache_wb((void *)tunebuffer, buflen);
    DPRINTF(("setup_tune done for %d bytes\n", buflen));

    /*
     * play the tune now
     */
    prad_setup_dtoa((__psunsigned_t)tunebuffer, buflen);

    prad_start_dtoa_dma();

}
 
/*
 * set timeout to 6 secs. Also some time has already
 * passed since the tune has started playing and the
 * startup tune (2 secs long) should have finished already
 */
#define MS_PER_SEC		(1000)
#define BOOTTUNE_TIMEOUT	(6 * MS_PER_SEC)

int
pon_audio_wait(void)
{
    static char *radbad = "Boottune on RAD Audio Processor            *FAILED*\n";
    static char *ip30bad = "\n\tCheck or replace:  System board (IP30)\n";
    int rv;

    if ( prad_probe() != 0 )
	return 0;

    rv = prad_drain_dtoa_dma(BOOTTUNE_TIMEOUT);
    prad_mute_relay();
    if (rv) {
	printf("%s", radbad);
	flash_pds_log(radbad);
	printf("%s", ip30bad);
    }
    return(rv);
}

void
wait_hello_tune(void)
{
    if ( prad_probe() != 0 )
	return;

    (void)prad_drain_dtoa_dma(BOOTTUNE_TIMEOUT);
    prad_mute_relay();
}

#if BOOTTONE
/*
 * a 16-bit mono sine wave (just for testing)
 */

static int sampbuf[] = {
0x0, 0x1200, 0x23a6, 0x3496, 
0x447a, 0x5301, 0x5fe2, 0x6adb, 
0x73b5, 0x7a41, 0x7e5e, 0x7ff9, 
0x7f08, 0x7b91, 0x75a4, 0x6d61, 
0x62f1, 0x5689, 0x4869, 0x38d8, 
0x2826, 0x16a7, 0x4b6, 0xfffff2ad, 
0xffffe0e7, 0xffffcfbf, 0xffffbf8d, 0xffffb0a3, 
0xffffa34d, 0xffff97cf, 0xffff8e63, 0xffff873a, 
0xffff8277, 0xffff8033, 0xffff807a, 0xffff834a, 
0xffff8895, 0xffff9040, 0xffff9a23, 0xffffa60d, 
0xffffb3c1, 0xffffc2f9, 0xffffd368, 0xffffe4ba, 
0xfffff696, 0x8a1, 0x1a82, 0x2bdb, 
0x3c56, 0x4b9d, 0x5963, 0x6562, 
0x6f5e, 0x7722, 0x7c88, 0x7f74, 
0x7fd7, 0x7db0, 0x7908, 0x71f9, 
0x68a5, 0x5d3d, 0x4ffa, 0x4120, 
0x30fb, 0x1fdc, 0xe1b, 0xfffffc13, 
0xffffea1f, 0xffffd899, 0xffffc7dc, 0xffffb83d, 
0xffffaa0c, 0xffff9d8f, 0xffff9308, 0xffff8aac, 
0xffff84a4, 0xffff8111, 0xffff8004, 0xffff8182, 
0xffff8584, 0xffff8bf6, 0xffff94b6, 0xffff9f99, 
0xffffac66, 0xffffbadd, 0xffffcab3, 0xffffdb99, 
0xffffed39, 0xffffff37, 0x1139, 0x22e4, 
0x33de, 0x43d0, 0x5268, 0x5f5d, 
0x6a6c, 0x735e, 0x7a04, 0x7e3e, 
0x7ff5, 0x7f20, 0x7bc4, 0x75f3, 
0x6dc9, 0x6370, 0x571d, 0x490e, 
0x398c, 0x28e5, 0x176d, 0x57e, 
0xfffff375, 0xffffe1aa, 0xffffd07a, 0xffffc03b, 
0xffffb141, 0xffffa3d8, 0xffff9844, 0xffff8ec0, 
0xffff877d, 0xffff829f, 0xffff803f, 0xffff806a, 
0xffff831e, 0xffff884d, 0xffff8fde, 0xffff99aa, 
0xffffa57f, 0xffffb320, 0xffffc249, 0xffffd2ac, 
0xffffe3f5, 0xfffff5ce, 0x7d9, 0x19bd, 
0x2b1e, 0x3ba4, 0x4afa, 0x58d3, 
0x64e7, 0x6efa, 0x76d8, 0x7c59, 
0x7f61, 0x7fe0, 0x7dd5, 0x7949, 
0x7254, 0x6919, 0x5dc6, 0x5097, 
0x41cd, 0x31b4, 0x209f, 0xee3, 
0xfffffcdc, 0xffffeae5, 0xffffd959, 0xffffc891, 
0xffffb8e4, 0xffffaaa1, 0xffff9e10, 0xffff9372, 
0xffff8afd, 0xffff84db, 0xffff812c, 0xffff8002, 
0xffff8164, 0xffff854b, 0xffff8ba2, 0xffff9449, 
0xffff9f15, 0xffffabce, 0xffffba34, 0xffffc9fd, 
0xffffdad9, 0xffffec72, 0xfffffe6e, 0x1072, 
0x2223, 0x3326, 0x4325, 0x51ce, 
0x5ed6, 0x69fc, 0x7306, 0x79c7, 
0x7e1c, 0x7fef, 0x7f37, 0x7bf7, 
0x7640, 0x6e30, 0x63ee, 0x57b0, 
0x49b3, 0x3a3f, 0x29a3, 0x1833, 
0x647, 0xfffff43d, 0xffffe26d, 0xffffd134, 
0xffffc0ea, 0xffffb1e0, 0xffffa464, 0xffff98bb, 
0xffff8f1f, 0xffff87c1, 0xffff82c8, 0xffff804c, 
0xffff805a, 0xffff82f2, 0xffff8807, 0xffff8f7e, 
0xffff9932, 0xffffa4f1, 0xffffb280, 0xffffc199, 
0xffffd1f0, 0xffffe331, 0xfffff505, 0x710, 
0x18f8, 0x2a61, 0x3af2, 0x4a57, 
0x5842, 0x646b, 0x6e95, 0x768d, 
0x7c29, 0x7f4c, 0x7fe8, 0x7df9, 
0x7989, 0x72ae, 0x698b, 0x5e4f, 
0x5133, 0x4279, 0x326d, 0x2161, 
0xfab, 0xfffffda5, 0xffffebab, 0xffffda19, 
0xffffc947, 0xffffb98c, 0xffffab37, 0xffff9e92, 
0xffff93dd, 0xffff8b4f, 0xffff8512, 0xffff8147, 
0xffff8001, 0xffff8147, 0xffff8512, 0xffff8b4f, 
0xffff93dd, 0xffff9e92, 0xffffab37, 0xffffb98c, 
0xffffc947, 0xffffda19, 0xffffebab, 0xfffffda5, 
0xfab, 0x2161, 0x326d, 0x4279, 
0x5133, 0x5e4f, 0x698b, 0x72ae, 
0x7989, 0x7df9, 0x7fe8, 0x7f4c, 
0x7c29, 0x768d, 0x6e95, 0x646b, 
0x5842, 0x4a57, 0x3af2, 0x2a61, 
0x18f8, 0x710, 0xfffff505, 0xffffe331, 
0xffffd1f0, 0xffffc199, 0xffffb280, 0xffffa4f1, 
0xffff9932, 0xffff8f7e, 0xffff8807, 0xffff82f2, 
0xffff805a, 0xffff804c, 0xffff82c8, 0xffff87c1, 
0xffff8f1f, 0xffff98bb, 0xffffa464, 0xffffb1e0, 
0xffffc0ea, 0xffffd134, 0xffffe26d, 0xfffff43d, 
0x647, 0x1833, 0x29a3, 0x3a3f, 
0x49b3, 0x57b0, 0x63ee, 0x6e30, 
0x7640, 0x7bf7, 0x7f37, 0x7fef, 
0x7e1c, 0x79c7, 0x7306, 0x69fc, 
0x5ed6, 0x51ce, 0x4325, 0x3326, 
0x2223, 0x1072, 0xfffffe6e, 0xffffec72, 
0xffffdad9, 0xffffc9fd, 0xffffba34, 0xffffabce, 
0xffff9f15, 0xffff9449, 0xffff8ba2, 0xffff854b, 
0xffff8164, 0xffff8002, 0xffff812c, 0xffff84db, 
0xffff8afd, 0xffff9372, 0xffff9e10, 0xffffaaa1, 
0xffffb8e4, 0xffffc891, 0xffffd959, 0xffffeae5, 
0xfffffcdc, 0xee3, 0x209f, 0x31b4, 
0x41cd, 0x5097, 0x5dc6, 0x6919, 
0x7254, 0x7949, 0x7dd5, 0x7fe0, 
0x7f61, 0x7c59, 0x76d8, 0x6efa, 
0x64e7, 0x58d3, 0x4afa, 0x3ba4, 
0x2b1e, 0x19bd, 0x7d9, 0xfffff5ce, 
0xffffe3f5, 0xffffd2ac, 0xffffc249, 0xffffb320, 
0xffffa57f, 0xffff99aa, 0xffff8fde, 0xffff884d, 
0xffff831e, 0xffff806a, 0xffff803f, 0xffff829f, 
0xffff877d, 0xffff8ec0, 0xffff9844, 0xffffa3d8, 
0xffffb141, 0xffffc03b, 0xffffd07a, 0xffffe1aa, 
0xfffff375, 0x57e, 0x176d, 0x28e5, 
0x398c, 0x490e, 0x571d, 0x6370, 
0x6dc9, 0x75f3, 0x7bc4, 0x7f20, 
0x7ff5, 0x7e3e, 0x7a04, 0x735e, 
0x6a6c, 0x5f5d, 0x5268, 0x43d0, 
0x33de, 0x22e4, 0x1139, 0xffffff37, 
0xffffed39, 0xffffdb99, 0xffffcab3, 0xffffbadd, 
0xffffac66, 0xffff9f99, 0xffff94b6, 0xffff8bf6, 
0xffff8584, 0xffff8182, 0xffff8004, 0xffff8111, 
0xffff84a4, 0xffff8aac, 0xffff9308, 0xffff9d8f, 
0xffffaa0c, 0xffffb83d, 0xffffc7dc, 0xffffd899, 
0xffffea1f, 0xfffffc13, 0xe1b, 0x1fdc, 
0x30fb, 0x4120, 0x4ffa, 0x5d3d, 
0x68a5, 0x71f9, 0x7908, 0x7db0, 
0x7fd7, 0x7f74, 0x7c88, 0x7722, 
0x6f5e, 0x6562, 0x5963, 0x4b9d, 
0x3c56, 0x2bdb, 0x1a82, 0x8a1, 
0xfffff696, 0xffffe4ba, 0xffffd368, 0xffffc2f9, 
0xffffb3c1, 0xffffa60d, 0xffff9a23, 0xffff9040, 
0xffff8895, 0xffff834a, 0xffff807a, 0xffff8033, 
0xffff8277, 0xffff873a, 0xffff8e63, 0xffff97cf, 
0xffffa34d, 0xffffb0a3, 0xffffbf8d, 0xffffcfbf, 
0xffffe0e7, 0xfffff2ad, 0x4b6, 0x16a7, 
0x2826, 0x38d8, 0x4869, 0x5689, 
0x62f1, 0x6d61, 0x75a4, 0x7b91, 
0x7f08, 0x7ff9, 0x7e5e, 0x7a41, 
0x73b5, 0x6adb, 0x5fe2, 0x5301, 
0x447a, 0x3496, 0x23a6, 0x1200, 
0x0, 0xffffee00, 0xffffdc5a, 0xffffcb6a, 
0xffffbb86, 0xffffacff, 0xffffa01e, 0xffff9525, 
0xffff8c4b, 0xffff85bf, 0xffff81a2, 0xffff8007, 
0xffff80f8, 0xffff846f, 0xffff8a5c, 0xffff929f, 
0xffff9d0f, 0xffffa977, 0xffffb797, 0xffffc728, 
0xffffd7da, 0xffffe959, 0xfffffb4a, 0xd53, 
0x1f19, 0x3041, 0x4073, 0x4f5d, 
0x5cb3, 0x6831, 0x719d, 0x78c6, 
0x7d89, 0x7fcd, 0x7f86, 0x7cb6, 
0x776b, 0x6fc0, 0x65dd, 0x59f3, 
0x4c3f, 0x3d07, 0x2c98, 0x1b46, 
0x96a, 0xfffff75f, 0xffffe57e, 0xffffd425, 
0xffffc3aa, 0xffffb463, 0xffffa69d, 0xffff9a9e, 
0xffff90a2, 0xffff88de, 0xffff8378, 0xffff808c, 
0xffff8029, 0xffff8250, 0xffff86f8, 0xffff8e07, 
0xffff975b, 0xffffa2c3, 0xffffb006, 0xffffbee0, 
0xffffcf05, 0xffffe024, 0xfffff1e5, 0x3ed, 
0x15e1, 0x2767, 0x3824, 0x47c3, 
0x55f4, 0x6271, 0x6cf8, 0x7554, 
0x7b5c, 0x7eef, 0x7ffc, 0x7e7e, 
0x7a7c, 0x740a, 0x6b4a, 0x6067, 
0x539a, 0x4523, 0x354d, 0x2467, 
0x12c7, 0xc9, 0xffffeec7, 0xffffdd1c, 
0xffffcc22, 0xffffbc30, 0xffffad98, 0xffffa0a3, 
0xffff9594, 0xffff8ca2, 0xffff85fc, 0xffff81c2, 
0xffff800b, 0xffff80e0, 0xffff843c, 0xffff8a0d, 
0xffff9237, 0xffff9c90, 0xffffa8e3, 0xffffb6f2, 
0xffffc674, 0xffffd71b, 0xffffe893, 0xfffffa82, 
0xc8b, 0x1e56, 0x2f86, 0x3fc5, 
0x4ebf, 0x5c28, 0x67bc, 0x7140, 
0x7883, 0x7d61, 0x7fc1, 0x7f96, 
0x7ce2, 0x77b3, 0x7022, 0x6656, 
0x5a81, 0x4ce0, 0x3db7, 0x2d54, 
0x1c0b, 0xa32, 0xfffff827, 0xffffe643, 
0xffffd4e2, 0xffffc45c, 0xffffb506, 0xffffa72d, 
0xffff9b19, 0xffff9106, 0xffff8928, 0xffff83a7, 
0xffff809f, 0xffff8020, 0xffff822b, 0xffff86b7, 
0xffff8dac, 0xffff96e7, 0xffffa23a, 0xffffaf69, 
0xffffbe33, 0xffffce4c, 0xffffdf61, 0xfffff11d, 
0x324, 0x151b, 0x26a7, 0x376f, 
0x471c, 0x555f, 0x61f0, 0x6c8e, 
0x7503, 0x7b25, 0x7ed4, 0x7ffe, 
0x7e9c, 0x7ab5, 0x745e, 0x6bb7, 
0x60eb, 0x5432, 0x45cc, 0x3603, 
0x2527, 0x138e, 0x192, 0xffffef8e, 
0xffffdddd, 0xffffccda, 0xffffbcdb, 0xffffae32, 
0xffffa12a, 0xffff9604, 0xffff8cfa, 0xffff8639, 
0xffff81e4, 0xffff8011, 0xffff80c9, 0xffff8409, 
0xffff89c0, 0xffff91d0, 0xffff9c12, 0xffffa850, 
0xffffb64d, 0xffffc5c1, 0xffffd65d, 0xffffe7cd, 
0xfffff9b9, 0xbc3, 0x1d93, 0x2ecc, 
0x3f16, 0x4e20, 0x5b9c, 0x6745, 
0x70e1, 0x783f, 0x7d38, 0x7fb4, 
0x7fa6, 0x7d0e, 0x77f9, 0x7082, 
0x66ce, 0x5b0f, 0x4d80, 0x3e67, 
0x2e10, 0x1ccf, 0xafb, 0xfffff8f0, 
0xffffe708, 0xffffd59f, 0xffffc50e, 0xffffb5a9, 
0xffffa7be, 0xffff9b95, 0xffff916b, 0xffff8973, 
0xffff83d7, 0xffff80b4, 0xffff8018, 0xffff8207, 
0xffff8677, 0xffff8d52, 0xffff9675, 0xffffa1b1, 
0xffffaecd, 0xffffbd87, 0xffffcd93, 0xffffde9f, 
0xfffff055, 0x25b, 0x1455, 0x25e7, 
0x36b9, 0x4674, 0x54c9, 0x616e, 
0x6c23, 0x74b1, 0x7aee, 0x7eb9, 
0x7fff, 0x7eb9, 0x7aee, 0x74b1, 
0x6c23, 0x616e, 0x54c9, 0x4674, 
0x36b9, 0x25e7, 0x1455, 0x25b, 
0xfffff055, 0xffffde9f, 0xffffcd93, 0xffffbd87, 
0xffffaecd, 0xffffa1b1, 0xffff9675, 0xffff8d52, 
0xffff8677, 0xffff8207, 0xffff8018, 0xffff80b4, 
0xffff83d7, 0xffff8973, 0xffff916b, 0xffff9b95, 
0xffffa7be, 0xffffb5a9, 0xffffc50e, 0xffffd59f, 
0xffffe708, 0xfffff8f0, 0xafb, 0x1ccf, 
0x2e10, 0x3e67, 0x4d80, 0x5b0f, 
0x66ce, 0x7082, 0x77f9, 0x7d0e, 
0x7fa6, 0x7fb4, 0x7d38, 0x783f, 
0x70e1, 0x6745, 0x5b9c, 0x4e20, 
0x3f16, 0x2ecc, 0x1d93, 0xbc3, 
0xfffff9b9, 0xffffe7cd, 0xffffd65d, 0xffffc5c1, 
0xffffb64d, 0xffffa850, 0xffff9c12, 0xffff91d0, 
0xffff89c0, 0xffff8409, 0xffff80c9, 0xffff8011, 
0xffff81e4, 0xffff8639, 0xffff8cfa, 0xffff9604, 
0xffffa12a, 0xffffae32, 0xffffbcdb, 0xffffccda, 
0xffffdddd, 0xffffef8e, 0x192, 0x138e, 
0x2527, 0x3603, 0x45cc, 0x5432, 
0x60eb, 0x6bb7, 0x745e, 0x7ab5, 
0x7e9c, 0x7ffe, 0x7ed4, 0x7b25, 
0x7503, 0x6c8e, 0x61f0, 0x555f, 
0x471c, 0x376f, 0x26a7, 0x151b, 
0x324, 0xfffff11d, 0xffffdf61, 0xffffce4c, 
0xffffbe33, 0xffffaf69, 0xffffa23a, 0xffff96e7, 
0xffff8dac, 0xffff86b7, 0xffff822b, 0xffff8020, 
0xffff809f, 0xffff83a7, 0xffff8928, 0xffff9106, 
0xffff9b19, 0xffffa72d, 0xffffb506, 0xffffc45c, 
0xffffd4e2, 0xffffe643, 0xfffff827, 0xa32, 
0x1c0b, 0x2d54, 0x3db7, 0x4ce0, 
0x5a81, 0x6656, 0x7022, 0x77b3, 
0x7ce2, 0x7f96, 0x7fc1, 0x7d61, 
0x7883, 0x7140, 0x67bc, 0x5c28, 
0x4ebf, 0x3fc5, 0x2f86, 0x1e56, 
0xc8b, 0xfffffa82, 0xffffe893, 0xffffd71b, 
0xffffc674, 0xffffb6f2, 0xffffa8e3, 0xffff9c90, 
0xffff9237, 0xffff8a0d, 0xffff843c, 0xffff80e0, 
0xffff800b, 0xffff81c2, 0xffff85fc, 0xffff8ca2, 
0xffff9594, 0xffffa0a3, 0xffffad98, 0xffffbc30, 
0xffffcc22, 0xffffdd1c, 0xffffeec7, 0xc9, 
0x12c7, 0x2467, 0x354d, 0x4523, 
0x539a, 0x6067, 0x6b4a, 0x740a, 
0x7a7c, 0x7e7e, 0x7ffc, 0x7eef, 
0x7b5c, 0x7554, 0x6cf8, 0x6271, 
0x55f4, 0x47c3, 0x3824, 0x2767, 
0x15e1, 0x3ed, 0xfffff1e5, 0xffffe024, 
0xffffcf05, 0xffffbee0, 0xffffb006, 0xffffa2c3, 
0xffff975b, 0xffff8e07, 0xffff86f8, 0xffff8250, 
0xffff8029, 0xffff808c, 0xffff8378, 0xffff88de, 
0xffff90a2, 0xffff9a9e, 0xffffa69d, 0xffffb463, 
0xffffc3aa, 0xffffd425, 0xffffe57e, 0xfffff75f, 
0x96a, 0x1b46, 0x2c98, 0x3d07, 
0x4c3f, 0x59f3, 0x65dd, 0x6fc0, 
0x776b, 0x7cb6, 0x7f86, 0x7fcd, 
0x7d89, 0x78c6, 0x719d, 0x6831, 
0x5cb3, 0x4f5d, 0x4073, 0x3041, 
0x1f19, 0xd53, 0xfffffb4a, 0xffffe959, 
0xffffd7da, 0xffffc728, 0xffffb797, 0xffffa977, 
0xffff9d0f, 0xffff929f, 0xffff8a5c, 0xffff846f, 
0xffff80f8, 0xffff8007, 0xffff81a2, 0xffff85bf, 
0xffff8c4b, 0xffff9525, 0xffffa01e, 0xffffacff, 
0xffffbb86, 0xffffcb6a, 0xffffdc5a, 0xffffee00, 
};

#endif

int
setup_tune(int *pbuf, int tunetype)
{

    int		i;
    int		buflen = 0;		/* in bytes */
    unchar	*sbuf;

#ifdef ENETBOOT
    extern int 	tune_file_size;


    if (tune_file_size)
	return(tune_file_size);
#endif
#if BOOTTONE
    switch ( tunetype ) {

      case 0:
      case 1:
      case 2:
      default:

	for ( i = 0; i < 24000; i+=2 ) {
	    pbuf[i] = sampbuf[i%1024] << 8;
	    pbuf[i+1] = 0;
	}
	for ( i = 24000; i < 48000; i+=2 ) {
	    pbuf[i] = 0;
	    pbuf[i+1] = sampbuf[i%1024] << 8;
	}
	for ( i = 48000; i < 72000; i+=2 ) {
	    pbuf[i] = sampbuf[i%1024] << 8;
	    pbuf[i+1] = 0;
	}
	for ( i = 72000; i < 96000; i+=2 ) {
	    pbuf[i] = 0;
	    pbuf[i+1] = sampbuf[i%1024] << 8;
	}
	
	buflen = 96000 * 2;

	break;

    }
#else

    switch(tunetype) {
      case 0:
	sbuf = starttune;
	buflen = starttune_size;
	break;

      case 1:
	sbuf = stoptune;
	buflen = stoptune_size;
	break;

      case 2:
      default:
	sbuf = badgfxtune;
	buflen = badgfxtune_size;
	break;

    }
    buflen = adpcm_decoder(sbuf, pbuf, buflen, &pcm_state);

#endif

    return(buflen);

}


/***********************************************************
Copyright 1992 by Stichting Mathematisch Centrum, Amsterdam, The
Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/*
** Intel/DVI ADPCM coder/decoder.
**
** The algorithm for this coder was taken from the IMA Compatability Project
** proceedings, Vol 2, Number 2; May 1992.
**
** Version 1.1, 16-Dec-92.
**
** Change log:
** - Fixed a stupid bug, where the delta was computed as
**   stepsize*code/4 in stead of stepsize*(code+0.5)/4. The old behavior can
**   still be gotten by defining STUPID_V1_BUG.
*/

#ifndef __STDC__
#define signed
#endif

/* Intel ADPCM step variation table */
static int prom_indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static int prom_stepsizeTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

int *stepsizeTable, *indexTable;

int
adpcm_decoder(unchar *indata, int *outbuf, int len,
	      struct adpcm_state *state)
{
    int delta;		/* Current adpcm output value */
    int code;		/* Current adpcm output value */
    int step;		/* Stepsize */
    int valprev;	/* virtual previous output value */
    int vpdiff;		/* difference between adjacent output samps */
    int index;		/* Current step change index */
    int indexdelta;
    int *outdata = outbuf;
    unsigned int inputbuffer;	/* place to keep next 4-bit value */

#if OLD
    void (*bcopyp)(const void *, void *, int);
    void* (*mallocp)(size_t);

    /*
     * Create area in memory for the tables used by the ADPCM
     * algorithm. Then copy the values from the tables in the
     * prom to memory. Bcopy cannot be used since we must do a 
     * jump instruction. Thus, we must call bcopy via a pointer
     * to the function.
     */
    bcopyp = bcopy;
    mallocp = malloc;
    stepsizeTable = (int *) mallocp(sizeof(prom_stepsizeTable));
    indexTable = (int *) mallocp(sizeof(prom_indexTable));
    bcopyp(prom_stepsizeTable, stepsizeTable, sizeof(prom_stepsizeTable));
    bcopyp(prom_indexTable, indexTable, sizeof(prom_indexTable));
#endif

    stepsizeTable = prom_stepsizeTable;
    indexTable = prom_indexTable;

    
    valprev = state->valprev;
    index = state->index;
    step = stepsizeTable[index];

    /*len /= 2; */
    
    for ( ; len > 0 ; len-- ) {
	
	/* Step 1 - get the code value */
        inputbuffer = *indata++;
	code = (inputbuffer >> 4);

	/* Step 2 - Find new index value (for later) */
	indexdelta = indexTable[code];
	index += indexdelta;
        if( (unsigned)index> 88) {
	    if ( index < 0 ) index = 0;
	    else if ( index > 88 ) index = 88;
        }

	/* Step 3 - compute the magnitude */
	delta = code & 7;

	/* Step 4 - update and clamp the output value */
	vpdiff = ((delta*step) >> 2) + (step >> 3);
	if ( delta != code ) {
	    valprev -= vpdiff;
	    if ( valprev < -32768 ) valprev = -32768;
	} else {
	    valprev += vpdiff;
	    if ( valprev > 32767 ) valprev = 32767;
        }

	/* Step 6 - Update step value */
	step = stepsizeTable[index];

	/* Step 7 - Output value */
	*outdata++ = valprev<<8;	/* chan 1 */
	*outdata++ = valprev<<8;	/* chan 1 */

	/* Step 1 - get the delta value and compute next index */
	code = inputbuffer & 0xf;

	/* Step 2 - Find new index value (for later) */
	indexdelta = indexTable[code];
	index += indexdelta;
        if( (unsigned)index> 88) {
	    if ( index < 0 ) index = 0;
	    else if ( index > 88 ) index = 88;
        }

	/* Step 3 - compute the magnitude */
	delta = code & 7;

	/* Step 4 - update and clamp the output value */
	vpdiff = ((delta*step) >> 2) + (step >> 3);
	if ( delta != code ) {
	    valprev -= vpdiff;
	    if ( valprev < -32768 ) valprev = -32768;
	} else {
	    valprev += vpdiff;
	    if ( valprev > 32767 ) valprev = 32767;
        }

	/* Step 6 - Update step value */
	step = stepsizeTable[index];

	/* Step 7 - Output value */
	*outdata++ = valprev<<8;	/* chan 1 */
	*outdata++ = valprev<<8;	/* chan 1 */
    }

    bzero(outdata, 0x10000);
    outdata += (0x10000/4);

    state->valprev = valprev;
    state->index = index;
    return(outdata - outbuf);
}

void
end_adpcm()
{
}


#else /* NOGFX || NOBOOTTUNE */

/*ARGSUSED*/
void 
play_hello_tune(int tune_type) {}

void
wait_hello_tune(void) {}

int
pon_audio_wait(void) { return(0); }

#endif /* !NOGFX && !NOBOOTTUNE */
