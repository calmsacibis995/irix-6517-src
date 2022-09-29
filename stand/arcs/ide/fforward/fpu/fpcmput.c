/*
 * fpu/fcmput.c
 *
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.5 $"

#include <math.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define	FLASH_COUNT	100000

/*
 * this program is intended as a diagnostic for the floating point unit
 * given a list of "infinite" series, it executes them a specified number
 * of times and compares the result gotten at run-time with an expected
 * result.  discrepancies are reported.
 *
 * instructions for using the fpudiag:
 *
 * type 'fpudiag' with no options to run the existing diagnostic
 *
 * to add a diagnostic function, write the code to compute the nth term of 
 * the sum or product series when given n.  then append an entry to the	
 * SERIES array which describes your new function and the series it
 * contributes to.  (this should be easy to do; just look at existing
 * entries.)  set n_siterations and n_diterations to reasonable values, to 
 * control how many terms of the series will be calculated.
 * finally, run fpudiag with the -e option to evaluate the series, in both
 * single and double precision, to the number of terms specified.
 *
 * finally, edit the SERIES array to include the evaluated values, so that
 * when fpudiag is run as a diagnostic, it will be happy with the results.
 *
 * general notes:
 * only convergent series are employed here.  this has several implications
 * for order of evaluation, and for floating point precision.  note that we
 * are more interested in getting deterministic results than correct ones.
 *
 * for SIGMA, or additive, series, terms must approach zero for the series
 * to converge.  thus, we evaluate the series from high n to low n, so that
 * the tiny terms at the "right-hand end" are not lost to rounding when
 * added to a pre-existing series.  this is a better exercise of the fpu,
 * since it is adding denorms to denorms instead of denorms to norms (which
 * in most cases would leave the norms unchanged).
 *
 * for example: suppose (as a contrived example) we are adding the series
 *     2 = 1 + 1/2 + 1/4 + 1/8 + 1/16 + ...
 * that is, the nth term is 1 / 2^n.  for the sake of explanation, suppose
 * further that we evaluate the series from n = 1 to n = 48.  take the
 * first 16 terms to get 2 - (2^-16), which is representable.  adding the
 * 17th term, we get (2 - (2^-16)) + (2^-17), which rounds to (2 - (2^-16)).
 *
 * the same thing happens for all further terms: they have *no effect* on
 * the continuing sum.
 *
 * the solution?  evaluate from high n to low n.  then the sum proceeds up
 * through denorms to norms, and by the time we reach the term 2^-16,
 * we have already accrued a sum of approximately 2^-16, and the numbers
 * accrue correctly.
 *
 * as an example, see sigma_pi_1() below.
 *
 * for PI, or multiplicative, series, the situation is similar although more
 * complicated.  again, only convergent series are used.  thus as we move to
 * the right hand end of the series, the terms there should be of the form
 * (1 + epsilon), where epsilon converges to zero.  thus as epsilon gets
 * tiny, (1 + epsilon) rounds to one and we have the same problem: late terms
 * have *no effect* on the multiplicative series.
 *
 * here, though, there are two solutions, both different from the solution we
 * we used in case of additive series.
 *
 * one solution is to treat each term as literally of the form (1 + epsilon),
 * and store it that way.  to first order,
 *     (1 + eps1) * (1 + eps2) ~ (1 + eps1 + eps2),
 * and we can store the running series product itself in the form (1 + epsilon)
 * provided none of the epsilons get too large (say, > 10^-10).
 * if we want greater accuracy, we can store the series to second order since
 *     (1 + eps1) * (1 + eps2) = (1 + eps1 + eps2 + (eps1 * eps2)).
 *
 * a second, simpler solution (which is used here) is to treat each product
 * series as the quotient of two product terms, which are each computed
 * separately to the desired precision and then divided as the last step in
 * the series evaluation.
 * as examples, see pi_pi_1_numer() and pi_pi_1_denom() below.
 */

#define SIGMA_TYPE  1	    /* series is a sum of terms		*/
#define PI_TYPE	    2	    /* series is a product of terms	*/


typedef struct series_struct {
	char *series_name;
	char *series_desc;
	double (*nth_numer)(int);	/* nth numerator, 1-relative */
	double (*nth_denom)(int);  	/* nth denorminator, 1-relative */
	int op_type;			/* how to compose successive terms */
	double multiplier;		/* constant term which multiplies
					   whole series */
	double offset;			/* this is added to series AFTER
					   multiplication */
	int n_siterations;		/* how many terms should be calc'ed
					   for float */
	int I_s_result;
	float s_result;	    		/* the desired (convergent) result as a
					   float */
	int n_diterations;		/* how many terms should be calc'ed
					   for double */
	int I_d_result[2];
	double d_result;		/* the desired (convergent) result as a
					   double  */
} SERIES;

double pi_sigma_1 (int), pi_sigma_2 (int);
double pi_pi_1_numer (int), pi_pi_1_denom (int);

SERIES series[] = {
	{   
		"pi = 4 * (1 - 1/3 + 1/5 - 1/7 + ...)",
		"series: sum of quotients producing pi",
		pi_sigma_1,
		NULL,
		SIGMA_TYPE,
		4.0,
		0.0,
		100 * 1000,
		0x40490fb0,
		3.1415827,
		1000 * 1000,
		{
			0x400921fa,
			0xce0c7013
		},
		3.1415916535897934,
	},

	{   
		"pi = 3 + 4 * ((1 / (2.3.4)) - (1 / (4.5.6)) + (1 / (6.7.8)) - ...)",
		"series: sum of reciprocal products producing pi",
		pi_sigma_2,
		NULL,
		SIGMA_TYPE,
		4.0,
		3.0,
		100 * 1000,
		0x40490fdb,
		3.1415927,
		1000 * 1000,
		{
			0x400921fb,
			0x54442d18
		},
		3.1415926535897931,
	},

	{
		"pi = 2 * (2 * 2 * 4 * 4 * 6 * ...) / (1 * 3 * 3 * 5 * 5 * ...)",
		"series: quotient of products producing pi",
		pi_pi_1_numer,
		pi_pi_1_denom,
		PI_TYPE,
		2.0,
		0.0,
		100 * 1000,
		0x40490cf1,
		3.1414242,
		1000 * 1000,
		{
			0x400921fa,
			0x816fcadb
		},
		3.1415910827866873,
	},
	
	{
		NULL
	},
};

/* ODD(n), EVEN(n) return (+1) iff n is odd (even), -1 otherwise */
#define ODD(n)	(((n) & 0x1) ? (1.0) : (-1.0))
#define EVEN(n)	(((n) & 0x1) ? (-1.0) : (1.0))


/* given n (1 <= n <= infinity), return value of nth term in series:*/
/*								    */
/*	    pi =  sigma  (4 / (2n - 1))				    */
/*		 n=1,inf					    */
/*								    */
double
pi_sigma_1(int n)
{
	return ODD(n) * (1.0 / (2.0 * n - 1.0));
}


/* given n (1 <= n <= infinity), return value of nth term in series:*/
/*								    */
/*  (pi-3)/4 =    sigma  [1 / (2n . (2n+1) . (2n+2))]		    */
/*		 n=1,inf					    */
/*								    */
double
pi_sigma_2(int n)
{
	return ODD(n) * (1.0 / ((2.0 * n) * (2.0 * n + 1.0) * (2.0 * n + 2.0)));
}


/* given n (1 <= n <= infinity), return value of nth term in series:*/
/*		        2.2.4.4.6.6. ...			    */
/*	    pi/2 =    --------------------			    */
/*		       1.3.3.5.5.7.7. ...			    */
/*								    */
double
pi_pi_1_numer(int n)
{
	return (2.0 * ((n + 1) / 2));
}


double
pi_pi_1_denom(int n)
{
	return (2.0 * (n / 2) + 1.0);
}

int fpudiag(void);

int
fpcmput()
{
	register k_machreg_t status;

	/* enable cache and fpu - cache ecc errors enabled */
	status = GetSR();
	status |= SR_CU0 | SR_CU1;
	SetSR(status);

	/* clear cause register */
	set_cause(0);

	/* clear fpu status register */
	SetFPSR(0);

	return fpudiag();
}


#define EPSILON (1E-10)
#define R_EPSILON (1E10)

fpudiag()
{
	SERIES *sptr;

	float s_numer, s_denom, s_result;
	double d_numer, d_denom, d_result;

	float s_abs_numer, s_abs_denom;
	double d_abs_numer, d_abs_denom;

	double d_nth_numer, d_nth_denom;

	int n;
	int fpu_error = 0;

	for (sptr = series; sptr->series_name != NULL; sptr++) {
		sptr->s_result = *((float *)(&sptr->I_s_result));
		sptr->d_result = *((double *)(&sptr->I_d_result));

		d_result = (sptr->op_type == SIGMA_TYPE) ? 0.0 : 1.0;
		s_result = (float) d_result;
		s_numer = s_denom = 1.0;
		d_numer = d_denom = d_nth_numer = d_nth_denom = 1.0;
	
		/* double precision */
		for (n = sptr->n_diterations; n >= 1; n--) {
			if ((n % FLASH_COUNT) == 0)
				busy(1);
			d_nth_numer = (sptr->nth_numer)(n);
			if (sptr->nth_denom != NULL)
				d_nth_denom = (sptr->nth_denom)(n);

			if (sptr->op_type == SIGMA_TYPE)
				d_result += d_nth_numer;
			else {	/* PI_TYPE */
				d_numer *= d_nth_numer;
				d_denom *= d_nth_denom;

				d_abs_numer = fabs(d_numer);
				d_abs_denom = fabs(d_denom);

				if ((d_abs_numer <= EPSILON)
				    || (d_abs_numer >= R_EPSILON)
				    || (d_abs_denom <= EPSILON)
				    || (d_abs_denom >= R_EPSILON)) {
					d_result *= (d_numer / d_denom);
					d_numer = d_denom = 1.0;
				}
			}
		}

		/* single precision */
		for (n = sptr->n_siterations; n >= 1; n--) {
			if ((n % FLASH_COUNT) == 0)
				busy(1);
			d_nth_numer = (sptr->nth_numer)(n);
			if (sptr->nth_denom != NULL)
				d_nth_denom = (sptr->nth_denom)(n);

			if (sptr->op_type == SIGMA_TYPE)
				s_result += (float)d_nth_numer;
			else {	/* PI_TYPE */
				s_numer *= (float)d_nth_numer;
				s_denom *= (float)d_nth_denom;

				s_abs_numer = (float)fabs((double)s_numer);
				s_abs_denom = (float)fabs((double)s_denom);

				if ((s_abs_numer <= EPSILON)
				    || (s_abs_numer >= R_EPSILON)
				    || (s_abs_denom <= EPSILON)
				    || (s_abs_denom >= R_EPSILON)) {
					s_result *= (s_numer / s_denom);
					s_numer = s_denom = 1.0;
				}
			}
		}

		if (sptr->op_type == PI_TYPE) {
			s_result *= (s_numer / s_denom);
			d_result *= (d_numer / d_denom);
		}

		s_result *= sptr->multiplier;
		s_result += sptr->offset;

		d_result *= sptr->multiplier;
		d_result += sptr->offset;

		if (s_result != sptr->s_result || d_result != sptr->d_result)
			fpu_error = 1;
	}

	busy(0);
	return fpu_error;
}
