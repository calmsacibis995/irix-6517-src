/* mc146818.h - Motorolla ?  146818 RTC (Real Time Clock) */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,09nov93,vin  moved AUX_CLK_RATE_MIN and AUX_CLK_RATE_MAX macro definitions
                 to config.h
01a,17aug93,hdn	 written.
*/

#ifndef	__INCmc146818h
#define	__INCmc146818h

#ifdef __cplusplus
extern "C" {
#endif


#ifndef	_ASMLANGUAGE

typedef struct
    {
    int rate;
    char bits;
    } CLK_RATE;

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif	/* __INCmc146818h */
