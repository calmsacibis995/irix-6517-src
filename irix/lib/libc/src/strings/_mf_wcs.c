#ident "$Revision: 1.2 $"
/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*	"@(#)libc-port:str/_mf_str.c	1.6"				*/
/*LINTLIBRARY*/
/*
* _mf_str.c - Fill MkFlt object from floating byte or wide string.
*/

#include "synonyms.h"
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <wchar.h>
#include <math.h>	/* only for _lib_version */
#include <errno.h>
#include "mkflt.h"
#undef _mf_wcs


extern unsigned char _numeric[];

static const char	_str__inity[] = "iInNiItTyY";
static const wchar_t    _wcs_lc_nan[] = {'n', 'a', 'n'};
static const wchar_t    _wcs_lc_inf[] = 
				{'i', 'n', 'f', 'i', 'n', 'i', 't', 'y'};

#   undef __wc
#   define __wc		ch	/* only used for ISSPACE(ch) */
#   define FCNNAME	_mf_wcs
#   define PTRNAME	wcs
#   define CHAR		wchar_t
#   define INT		wint_t
#   define ISSPACE(c)	(int)iswspace((wint_t)c)
#   define ISDIGIT(c)	('0' <= (c) && (c) <= '9')
#   define ISXDIGIT(c)	((c) <= UCHAR_MAX && isxdigit(c))
#   define ISALPHA(c)	((c) <= UCHAR_MAX && isalpha(c))

#define BIT(n)	((unsigned long)1 << (n))

#define LP	'('	/* so that editors can do paren matching */
#define RP	')'

void
#ifdef __STDC__
FCNNAME(register MkFlt *mfp)
#else
FCNNAME(mfp)register MkFlt *mfp;
#endif
{
	register const CHAR *sp;	/* walks through input string */
	register unsigned long val;	/* accumulates positive values */
	register BigInt *bp;		/* complete integer value */
	register INT ch;		/* usually sp[0] */
	int cnt;			/* number of digits in current val */
	int step;			/* nonzero after radix */
	size_t nz, nfz;			/* counts of 0s and fractional 0s */

	sp = mfp->PTRNAME;
	mfp->ndig = 0;
	mfp->exp = 0;
	mfp->sign = 0;
	mfp->bp = bp = &mfp->ibi;
	bp->allo = 0;
	bp->next = 0;
	bp->size = NPKT;
	step = 0;
	/*
	* Skip optional leading white space.
	*/
	ch = *sp;
	while (ISSPACE(ch))
		ch = *++sp;
	/*
	* Note optional leading sign.
	*/
	if (ch == '-')
	{
		mfp->sign = 1;
		goto sign1;
	}
	if (ch == '+')
	{
	sign1:;
		ch = *++sp;
	}
	/*
	* Now must be at first byte of string.
	*/
	if (!ISDIGIT(ch))
	{
		if (ch == _numeric[0])
		{
			ch = *++sp;
			if (!ISDIGIT(ch))
				goto reterr;
			mfp->exp = -1;
			/*
			* Skip leading fractional zeros.
			*/
			while (ch == '0')
			{
				mfp->exp--;
				ch = *++sp;
			}
			if (!ISDIGIT(ch))
				goto expzero;
			step = 1;
			goto gotdigit;
		}
#ifndef NO_NCEG_FPE
		/*
		* Check for "inf[inity]", case insensitive.
		*/
		if (ch == 'i' || ch == 'I')
		{
			const char *p;

#ifndef NO_MSE
			if (mfp->PTRNAME == _wcs_lc_inf) /* from *swscanf */
			{
				mfp->PTRNAME = sp + 8;
				goto retinf;
			}
#else
			if (mfp->PTRNAME == _str_lc_inf) /* from *scanf */
			{
				mfp->PTRNAME = sp + 8;
				goto retinf;
			}
#endif
			if ((ch = *++sp) != 'n' && ch != 'N')
				goto reterr;
			if ((ch = *++sp) != 'f' && ch != 'F')
				goto reterr;
			mfp->PTRNAME = sp;		/* fallback position */
			for (p = _str__inity; *p != '\0'; p += 2)
			{
				if ((ch = *++sp) != p[0] && ch != p[1])
					goto retinf;
			}
			mfp->PTRNAME = sp;
			goto retinf;
		}
		/*
		* Check for "nan[(...)]", case insensitive.
		*/
		if (ch == 'n' || ch == 'N')
		{
#ifndef NO_MSE
			if (mfp->PTRNAME == _wcs_lc_nan) /* from *swscanf */
			{
				mfp->PTRNAME = sp + 3;
				goto retdnan;
			}
#else
			if (mfp->PTRNAME == _str_lc_nan) /* from *scanf */
			{
				mfp->PTRNAME = sp + 3;
				goto retdnan;
			}
#endif
			if ((ch = *++sp) != 'a' && ch != 'A')
				goto reterr;
			if ((ch = *++sp) != 'n' && ch != 'N')
				goto reterr;
			mfp->PTRNAME = sp;		/* fallback position */
			if (*++sp == LP)
			{
				/*
				* Process all valid bytes up to right paren,
				* but ignore excess.
				*/
				cnt = 0;
				val = 0;
				for (;;)
				{
					ch = *++sp;
					if (!ISXDIGIT(ch))
					{
						if (ISALPHA(ch) || ch == '_')
							continue;
						break;
					}
					if (++cnt > ULBITS / 4)
					{
						if (mfp->ibi.next >= NPKT)
							continue;
						mfp->ibi.pkt[mfp->ibi.next++]
							= val;
						val = 0;
						cnt = 1;
					}
					val <<= 4;
					if (isdigit(ch))
						val |= ch - '0';
					else if (isupper(ch))
						val |= ch - 'A' + 10;
					else
						val |= ch - 'a' + 10;
				}
				if (ch == RP)
				{
					if (mfp->ibi.next < NPKT)
					{
						if ((cnt = 
						   (int)(ULBITS / 4 - cnt))
							!= 0)
						{
							val <<= cnt * 4;
						}
						mfp->ibi.pkt[mfp->ibi.next++]
							= val;
					}
					mfp->PTRNAME = sp + 1;
					goto retvnan;
				}
			}
			goto retdnan;
		}
#endif /*NO_NCEG_FPE*/
		goto reterr;
	}
	/*
	* Handle leading zero specially.
	*/
	if (ch == '0')
	{
#ifndef NO_NCEG_FPE
		if ((ch = *++sp) == 'x' || ch == 'X')	/* hex floating */
		{
			const CHAR *savesp = sp; /* if hex is malformed */

			ch = *++sp;
			if (!ISXDIGIT(ch))
			{
				if (ch == _numeric[0])
				{
					ch = *++sp;
					if (!ISXDIGIT(ch))
					{
						mfp->PTRNAME = savesp;
						goto retzero;
					}
					/*
					* Skip leading fractional zeros.
					*/
					while (ch == '0')
					{
						mfp->exp -= 4;
						ch = *++sp;
						if (!ISXDIGIT(ch))
							goto hexexpzero;
					}
					step = 4;
					goto hexgotdigit;
				}
				mfp->PTRNAME = savesp;
				goto retzero;
			}
			if (ch == '0')	/* leading zero still special */
			{
				do
					ch = *++sp;
				while (ch == '0'); /* skip leading zeros */
				if (!ISXDIGIT(ch))
				{
					if (ch == _numeric[0])
					{
						/*
						* Skip leading fractional zeros.
						*/
						while ((ch = *++sp) == '0')
							mfp->exp -= 4;
						if (ISXDIGIT(ch))
						{
							step = 4;
							goto hexgotdigit;
						}
					}
					/*
					* Zero value, but still need to find
					* the end of the sequence.
					*/
				hexexpzero:;
					mfp->PTRNAME = sp; /* fallback position */
					if (ch == 'p' || ch == 'P')
					{
						if ((ch = *++sp) == '-'
							|| ch == '+')
						{
							ch = *++sp;
						}
						if (ISDIGIT(ch))
						{
							while (++sp, ISDIGIT(*sp))
								;
							mfp->PTRNAME = sp;
						}
					}
					goto retzero;
				}
			}
			/*
			* Must be at the first nonzero hex digit.
			*/
		hexgotdigit:;
			cnt = 0;
			val = 0;
			for (;;) /* all integer and fractional digits */
			{
				if (++cnt > (int)(ULBITS / 4))
				{
					bp->pkt[bp->next] = val;
					if (++bp->next == bp->size)
					{
						if ((bp = _mf_grow(bp, 0)) == 0)
							goto retallo;
						mfp->bp = bp;
					}
					val = 0;
					cnt = 1;
				}
				val <<= 4;
				if (isdigit(ch))
					val |= ch - '0';
				else if (isupper(ch))
					val |= ch - 'A' + 10;
				else
					val |= ch - 'a' + 10;
				mfp->ndig++;
				mfp->exp -= step;
				ch = *++sp;
				if (!ISXDIGIT(ch))
				{
					if (ch != _numeric[0] || step != 0)
						break;
					step = 4;
					ch = *++sp;
					if (!ISXDIGIT(ch))
						break;
				}
			}
			/*
			* Adjust last unsigned long so that it is fully used.
			*/
			if ((cnt = (int)(ULBITS / 4 - cnt)) != 0)
			{
				cnt *= 4;
				mfp->exp -= cnt;
				val <<= cnt;
			}
			bp->pkt[bp->next++] = val;	/* slot available */
			/*
			* Check for exponent, which is optional only
			* if an explict radix point was present.
			*/
			if (ch != 'p' && ch != 'P')
			{
				if (step == 0)
				{
					mfp->PTRNAME = savesp;
					goto retzero;
				}
			}
			else
			{
				int esign = 0;

				if ((ch = *++sp) == '-')
				{
					esign = 1;
					goto hexsign;
				}
				if (ch == '+')
				{
				hexsign:;
					ch = *++sp;
				}
				if (!ISDIGIT(ch))
				{
					if (step == 0)
					{
						mfp->PTRNAME = savesp;
						goto retzero;
					}
				}
				else
				{
					val = ch - '0';
					while (ch = *++sp, ISDIGIT(ch))
					{
						if (val <= EXP_OFLOW)
						{
							val *= 10;
							val += ch - '0';
						}
					}
					mfp->PTRNAME = sp;
					if (esign == 0)
					{
						if (val > EXP_OFLOW)
							goto retover;
						mfp->exp += val;
					}
					else if (val > EXP_OFLOW ||
						(val += -mfp->exp) > EXP_OFLOW)
					{
						goto retunder;
					}
					else
						mfp->exp = -(long)val;
				}
			}
			/*
			* Normalize so that the highest order bit is set.
			*/
			if (((val = bp->pkt[0]) & BIT(ULBITS - 1)) == 0)
			{
				register int shift = 0;

				do
					shift++;
				while (((val <<= 1) & BIT(ULBITS - 1)) == 0);
				mfp->exp -= shift;
				bp->pkt[0] = val;
				if ((cnt = bp->next) > 1)
				{
					register unsigned long *p = &bp->pkt[0];

					do
					{
						p[0] |= p[1] >> (ULBITS - shift);
						*++p <<= shift;
					} while (--cnt > 1);
				}
			}
			if (bp->next < NPKT)	/* cover all precisions */
			{
				register unsigned long *p = &bp->pkt[NPKT];
				register int i = (int)(NPKT - bp->next);

				do
					*--p = 0;
				while (--i != 0);
			}
			goto rethex;
		}
		/*
		* Not a hexadecimal floating string.
		*/
		while (ch == '0')	/* skip leading zeros */
			ch = *++sp;
#else /*!NO_NCEG_FPE*/
		do
			ch = *++sp;
		while (ch == '0');	/* skip leading zeros */
#endif /*NO_NCEG_FPE*/
		if (!ISDIGIT(ch))
		{
			if (ch == _numeric[0])
			{
				/*
				* Skip leading fractional zeros.
				*/
				do
					mfp->exp--;
				while ((ch = *++sp) == '0');
				if (ISDIGIT(ch))
				{
					step = 1;
					goto gotdigit;
				}
			}
			/*
			* Zero value, but still need to find the end
			* of the sequence:  Check for valid exponent.
			*/
		expzero:;
			mfp->PTRNAME = sp;		/* fallback position */
			if (ch == 'e' || ch == 'E')
			{
				if ((ch = *++sp) == '-' || ch == '+'
#ifndef NO_CI4
					|| ch == ' ' && _lib_version == c_issue_4
#endif
					)
				{
					ch = *++sp;
				}
				if (ISDIGIT(ch))
				{
					while (++sp, ISDIGIT(*sp))
						;
					mfp->PTRNAME = sp;
				}
			}
			goto retzero;
		}
	}
	/*
	* Must be at the first nonzero digit.
	*/
gotdigit:;
	nz = 0;
	nfz = 0;
	cnt = 1;
	val = ch - '0';
	mfp->ndig = 1;
	for (;;)	/* all integer and fractional digits */
	{
		if ((ch = *++sp) == '0') /* wait to accumulate zeros */
		{
			nz++;
			nfz += step;
			continue;
		}
		if (!ISDIGIT(ch))
		{
			if (ch != _numeric[0] || step != 0)
				break;
			step = 1;
			if ((ch = *++sp) == '0')
			{
				nz++;
				nfz = 1;
				continue;
			}
			if (!ISDIGIT(ch))
				break;
		}
		/*
		* Nonzero digit.  Account for any skipped zeros.
		* Easiest just to duplicate the delayed code.
		*/
		if (nz != 0)
		{
			mfp->ndig += nz;
			mfp->exp -= nfz;
			do
			{
				if (++cnt > ULDIGS)
				{
					bp->pkt[bp->next] = val;
					if (++bp->next == bp->size)
					{
						bp = _mf_grow(bp, 0);
						if (bp == 0)
							goto retallo;
						mfp->bp = bp;
					}
					val = 0;
					cnt = 1;
					continue;
				}
				val *= 10;
			} while (--nz != 0);
			nfz = 0;
		}
		/*
		* Ready for the next nonzero digit.
		*/
		mfp->ndig++;
		mfp->exp -= step;
		if (++cnt > ULDIGS)
		{
			bp->pkt[bp->next] = val;
			if (++bp->next == bp->size)
			{
				if ((bp = _mf_grow(bp, 0)) == 0)
					goto retallo;
				mfp->bp = bp;
			}
			val = ch - '0';
			cnt = 1;
			continue;
		}
		val *= 10;
		val += ch - '0';
	}
	bp->pkt[bp->next++] = val;	/* slot available */
	nz -= nfz;
	mfp->exp += nz;
	/*
	* Handle optional exponent.
	*/
	mfp->PTRNAME = sp;	/* fallback position */
	if (ch == 'e' || ch == 'E')
	{
		int esign = 0;

		if ((ch = *++sp) == '-')
		{
			esign = 1;
			goto sign2;
		}
		if (ch == '+'
#ifndef NO_CI4
			|| ch == ' ' && _lib_version == c_issue_4
#endif
			)
		{
		sign2:;
			ch = *++sp;
		}
		if (ISDIGIT(ch))
		{
			val = ch - '0';
			while (ch = *++sp, ISDIGIT(ch))
			{
				if (val <= EXP_OFLOW)
				{
					val *= 10;
					val += ch - '0';
				}
			}
			mfp->PTRNAME = sp;
			if (esign == 0)
			{
				if (val > EXP_OFLOW || mfp->exp > 0
					&& mfp->exp > EXP_OFLOW - val)
				{
					goto retover;
				}
				mfp->exp += val;
			}
			else
			{
				if (val > EXP_OFLOW || mfp->exp < 0
					&& -mfp->exp > EXP_OFLOW - val)
				{
					goto retunder;
				}
				mfp->exp -= val;
			}
		}
	}
	mfp->kind = MFK_REGULAR;
	return;
#ifndef NO_NCEG_FPE
rethex:;
	mfp->kind = MFK_HEXSTR;
	return;
retdnan:;
	mfp->kind = MFK_DEFNAN;
	return;
retvnan:;
	mfp->kind = MFK_VALNAN;
	return;
retinf:;
	mfp->kind = MFK_INFINITY;
	return;
#endif
reterr:;
	setoserror(EINVAL);
retallo:;
	mfp->sign = 0;
retzero:;
	mfp->kind = MFK_ZERO;
	return;
retover:;
	mfp->kind = MFK_OVERFLOW;
	return;
retunder:;
	mfp->kind = MFK_UNDERFLOW;
	return;
}
