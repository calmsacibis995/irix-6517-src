/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) ram.c:3.2 5/30/92 20:18:46" };
#endif

#include "suite3.h"

/*
 *      ram_cchar
 */
ram_cchar( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register char	*pa, *pb; 
        char		a[512], b[512];
        int		i64;

	if ( sscanf(argv, "%d", &i64) < 1 ) {
		fprintf(stderr, "ram_cchar(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
        while (n--)  {
                pa = a; pb = b;
		for (i=32; i--;)
              *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,*pb++ = *pa++,
              *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,*pb++ = *pa++,
              *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,*pb++ = *pa++,
              *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,*pb++ = *pa++;
        } /* 16x32= 512 */
	res->c = b[i64];
        return(0);
}

/*
 *      ram_clong
 */
ram_clong( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register long	*pa, *pb; 
        long		a[128], b[128];
        int		i64;

	if ( sscanf(argv, "%d", &i64) < 1 ) {
		fprintf(stderr, "ram_clong(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
        while (n--)  {
                pa = a; pb = b; /* 128*sizeof long=512 */
                for (i=8; i--; )/* 128 = 16*8 */
        *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
        *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
        *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
        *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++;
        }
	res->l = b[i64];
        return(0);
}

/*
 *      ram_cshort
 */
ram_cshort( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register short	*pa, *pb; 
        short		a[256], b[256];
        int		i64;

	if ( sscanf(argv, "%d", &i64) < 1 ) {
		fprintf(stderr, "ram_cshort(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
        while (n--)  {
                pa = a; pb = b; /* 256*sizeof short=512 */
                for (i=16; i--; )/* 16*16 = 256 */
             *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
             *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
             *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++,
             *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++, *pb++ = *pa++;
        }
	res->s = b[i64];
        return(0);
}

/*
 *      ram_rchar
 */
ram_rchar( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register char	*pa, *b;
        char		a[512];
        int		i32;

	if ( sscanf(argv, "%d", &i32) < 1 ) {
		fprintf(stderr, "ram_rchar(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
	b = (char *)res;
        while (n--)  {
                pa = a;         /* 512 = 512*sizeof char */
                i = i32;        /* 32*16 = 512 */
                while (i--)
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++;
        }
        return(0);
}

/*
 *      ram_rlong
 */
ram_rlong( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register long	*pa, *b;
        long		a[128];
        int		i8;

	if ( sscanf(argv, "%d", &i8) < 1 ) {
		fprintf(stderr, "ram_rlong(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
	b = (long *)res;
        while (n--)  {
                pa = a;         /* 512 = 128*sizeof long */
                i = i8;         /* 128 = 16*8 */
                while (i--)
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++,
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++,
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++,
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++;
        }
        return(0);
}

/*
 *      ram_rshort
 */
ram_rshort( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register short	*pa, *b;
        short		a[256];
        int		i16;

	if ( sscanf(argv, "%d", &i16) < 1 ) {
		fprintf(stderr, "ram_rshort(): needs 1 argument!\n");
		return(-1);
	}

	n = 10000;
	b = (short *)res;
        while (n--)  {
                pa = a;         /* 256*sizeof short = 512 */
                i = i16;        /* 256=16*16 */
                while (i--)
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++, 
                      *b = *pa++, *b = *pa++, *b = *pa++, *b = *pa++;
        }
        return(0);
}

/*
 *      ram_wchar
 */
ram_wchar( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register char	*pa, *b;
        char		a[512];
        int		i32, i64;

	if ( sscanf(argv, "%d %d", &i32, &i64) < 2 ) {
		fprintf(stderr, "ram_wchar(): needs 2 arguments!\n");
		return(-1);
	}

	n = 10000;
	b = (char *)res;
        while (n--)  {
                pa = a;         /* 512 = 512*sizeof char */
                i = i32;        /* 512 = 16*32 */
                while (i--)
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b;
        }
	res->c = a[i64];
        return(0);
}

/*
 *      ram_wlong
 */
ram_wlong( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register long	*pa, *b;
        long		a[128];
        int		i8, i64;

	if ( sscanf(argv, "%d %d", &i8, &i64) < 2 ) {
		fprintf(stderr, "ram_wlong(): needs 2 arguments!\n");
		return(-1);
	}

	n = 10000;
	b = (long *)res;
        while (n--)  {
                pa = a;         /* 512 = 128*sizeof long */
                i = i8;         /* 8*16 = 128 */
                while (i--)
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b;
        }
	res->l = a[i64];
        return(0);
}

/*
 *      ram_wshort
 */
ram_wshort( argv, res )
char *argv;
Result *res;
{
        register short	i, n;
        register short	*pa, *b;
        short		a[256];
        int		i16, i64;

	if ( sscanf(argv, "%d %d", &i16, &i64) < 2 ) {
		fprintf(stderr, "ram_wshort(): needs 2 arguments!\n");
		return(-1);
	}

	n = 10000;
	b = (short *)res;
        while (n--)  {
                pa = a;         /* 512 = 256*sizeof short */
                i = i16;         /* 16*16 = 256 */
                while (i--)
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b,
                      *pa++ = *pa++ = *pa++ = *pa++ = *b;
        }
	res->s = a[i64];
        return(0);
}
