/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: George Wilkie
 *
 * tune.h of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * Definitions and functions required by tuning utilities
 */

/*
 * Standard I/O functions
 */
#include <stdio.h>
#ifdef SUNOS4
extern int fprintf(FILE *, const char *, ...);
extern int fclose(FILE *);
extern int printf(const char *, ...);
#endif
#ifndef SVR4
extern void perror(const char *);
#endif

/*
 * C library functions
 */
#include <string.h>
#ifdef SVR4
#include <stdlib.h>
#else /*SVR4*/
extern int atoi(const char *);
extern long atol(const char *);
extern long strtol(const char *, char **, int);
#endif /*SVR4*/

/*
 * UINT and STREAMS definitions
 */
#include <sys/snet/uint.h>
#include <streamio.h>

/*
 * Configuration file functions
 */
extern void open_conf(char *);
extern void close_conf_silent(void);
extern void close_conf(void);
extern char *get_conf_line(void);
extern int8 get_conf_int8(void);
extern int16 get_conf_int16(void);
extern int32 get_conf_int32(void);
extern uint8 get_conf_uint8(void);
extern uint16 get_conf_uint16(void);
extern uint32 get_conf_uint32(void);
extern char get_conf_char(void);
extern unsigned short get_conf_ushort(void);
extern void conf_exerr(const char *);
extern void exerr(const char *, const char *);
extern void experr(const char *, const char *);
extern char *basename(char *);
extern int hex_val(int);
extern uint32 str2uint32(const char *);
