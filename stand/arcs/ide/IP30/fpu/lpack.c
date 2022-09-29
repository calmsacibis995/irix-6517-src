
/*  -- translated by f2c (version of 21 October 1993  13:46:10).  */
/* :set ts=4 */

#ident "$Revision: 1.6 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include "libsc.h"
#include "libsk.h"
#include "uif.h"

#define ABS(x)		((x)>0.?(x):-(x))
#define WORKSIZE	100

static int lpackd_(void);
static int matgen(double *a, int lda, int n, double *b, double *norma);
static int dgefa(double *a, int lda, int n, long *ipvt, long *info);
static int dgesl(double *a, int lda, int n, long *ipvt, double *b);
static int daxpy(int n, double da, double *dx, double *dy);
static int dscal(int n, double da, double *dx);
static int idamax(int n, double *dx);
static double epslon(void);
static int dmxpy(int n1, double *y, int n2, int ldm, double *x,double *m);


int
lpackd(void)
{
	int i;
	int failed;
	ulong oldSR = GetSR();

	clear_nofault();

	msg_printf(VRB|PRCPU,"lpackd test\n");

	SetSR(oldSR | SR_CU1);

	for(i=0;i<75;i++) {
		failed = lpackd_();
		if (failed) {
			msg_printf(VRB|PRCPU, "lpackd test failed\n");
			SetSR(oldSR);
			return(1);
		}
		/* if(Chkpe() == 1) return(1); */
	}

/*	msg_printf(VRB|PRCPU,"\nStart swapped cache lpackd\n");
	for(i=0;i<50;i++) {
		swap_cache();
		failed=lpackd_();
		if (failed == 1 || Chkpe() == 1) {
			unswap_cache();
			return(1);
		}
	}
	unswap_cache();
*/

	msg_printf(VRB|PRCPU, "lpackd test passed\n");

	SetSR(oldSR);

	return(0);
}

#ifdef NOTDEF
lpackd_swap()
{
	int i;
	int failed;
	clear_nofault();
	findfp();
	msg_printf(VRB,"\nStart swapped cache lpackd\n");
	for(i=0;i<50;i++) {
		swap_cache();
		failed=lpackd_();
		if (failed == 1 || Chkpe() == 1) {
			unswap_cache();
			return(1);
		}
	}
	unswap_cache();
        printf("\n");
	return(0);
}
#endif


static int
lpackd_(void)
{
    /* Local variables */
    long *ipvt;
    double *a, *b, *x;
    double resid, norma;
    double normx;
    double residn;
    int lda;
    long info;
    double eps;
    int i, n;
    int ret_val = 1;
#define DMABUFSIZE      517120

    lda = WORKSIZE;
    n = WORKSIZE;
    {
	a = (double*)get_chunk(2*DMABUFSIZE);
	b = (double*)(a + 2*(WORKSIZE*WORKSIZE*sizeof(double)));
	x = (double*)(b + 2*(WORKSIZE*WORKSIZE*sizeof(double)));
	ipvt =(long *)(x + 2*(WORKSIZE*WORKSIZE*sizeof(double)));

		matgen(a, lda, n, b, &norma);
    	dgefa(a, lda, n, ipvt, &info);
    	dgesl(a, lda, n, ipvt, b);

		/* compute a residual to verify results */
    	for (i = 0; i < n; i++) 
			x[i] = b[i];

    	matgen(a, lda, n, b, &norma);
    	for (i = 0; i < n; i++) 
			b[i] = -b[i];

    	dmxpy(n, b, n, lda, x, a);
    	resid = (double)0.;
    	normx = (double)0.;
    	for (i = 0; i < n; ++i) {
			resid = MAX(resid,ABS(b[i]));
			normx = MAX(normx,ABS(x[i]));
    	}

    	eps = epslon();
    	residn = resid / ((double)n * norma * normx * eps);

		if ((1.0e0-x[0]) < 1.0e-13 && (1.0e0-x[n-1]) < 1.0e-13 && 
	  	x[0] < 1.0e0 && x[n-1] < 1.0e0) 
			ret_val = 0;
		else {
			/* at this point, failure */
			msg_printf(DBG|PRCPU, "lpackd failed\n");
			msg_printf(DBG|PRCPU, "  x[0] = %lf, x[n-1] = %lf\n",
				x[0], x[n-1]);
			msg_printf(DBG|PRCPU, "  Residn = %lf, Resid = %lf\n",
				residn, resid);
		}
	}
	free(a);
	return ret_val;
}

      
static int
matgen(double *a, int lda, int n, double *b, double *norma)
{
    /* Local variables */
    int init, i, j;

    /* Function Body */
    init = 1325;
    *norma = (double)0.;
    for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) {
			init = (3125*init) % 65536;
	    	a[i+j*lda] = ((double)init - 32768.0)/16384.0;
	    	*norma = MAX(*norma, ABS(a[i+j*lda]));
		}
    }
    for (i = 0; i < n; i++) 
		b[i] = (double)0.;
    for (j = 0; j < n; j++) {
		for (i = 0; i < n; ++i) 
	    	b[i] += a[i+j*lda];
    }

    return 0;
}


/**************************************************************************
*     dgefa factors a double precision matrix by gaussian elimination.
*
*     dgefa is usually called by dgeco, but it can be called
*     directly with a saving in time if  rcond  is not needed.
*     (time for dgeco) = (1 + 9/n)*(time for dgefa) .
*
*     on entry
*
*        a       double precision(lda, n)
*                the matrix to be factored.
*
*        lda     integer
*                the leading dimension of the array  a .
*
*        n       integer
*                the order of the matrix  a .
*
*     on return
*
*        a       an upper triangular matrix and the multipliers
*                which were used to obtain it.
*                the factorization can be written  a = l*u  where
*                l  is a product of permutation and unit lower
*                triangular matrices and  u  is upper triangular.
*
*        ipvt    integer(n)
*                an integer vector of pivot indices.
*
*        info    integer
*                = 0  normal value.
*                = k  if  u(k,k) .eq. 0.0 .  this is not an error
*                     condition for this subroutine, but it does
*                     indicate that dgesl or dgedi will divide by zero
*                     if called.  use  rcond  in dgeco for a reliable
*                     indication of singularity.
*
**************************************************************************/
static int
dgefa(double *a, int lda, int n, long *ipvt, long *info)
{
    /* System generated locals */
    int a_dim1, a_offset;

    /* Local variables */
    int j, k, l;
    double t;
    int kp1, nm1;

    /* Parameter adjustments */
    --ipvt;
    a_dim1 = lda;
    a_offset = a_dim1 + 1;
    a -= a_offset;

    /* Function Body */
    *info = 0;
    nm1 = n - 1;
    if (nm1 < 1) {
   		ipvt[n] = n;
   		if (a[n + n * a_dim1] == 0.) 
			*info = n;
    } else {

    	for (k = 1; k <= n - 1; ++k) {
			kp1 = k + 1;

			l = idamax(n - k + 1, &a[k + k * a_dim1]) + k - 1;
			ipvt[k] = l;

			if (a[l + k * a_dim1] == 0.) {
				*info = k;
				continue;
			}

			if (l != k) {
				t = a[l+k*a_dim1];
				a[l+k*a_dim1] = a[k+k*a_dim1];
				a[k+k*a_dim1] = t;
			}

			t = -1. / a[k+k*a_dim1];
			dscal(n-k, t, &a[k+1+k*a_dim1]);

			for (j = kp1; j <= n; ++j) {
	    		t = a[l+j*a_dim1];
	    		if (l != k) {
	    			a[l+j*a_dim1] = a[k+j*a_dim1];
	    			a[k+j*a_dim1] = t;
	    		}

	    		daxpy(n-k, t, &a[k+1+k*a_dim1], &a[k+1+j*a_dim1]);
			}
    	}
	}

    return 0;
} 


/*************************************************************************
*
*     dgesl solves the double precision system
*     a * x = b  [or  trans(a) * x = b : not used]
*     using the factors computed by dgeco or dgefa.
*
*     on entry
*
*        a       double precision(lda, n)
*                the output from dgeco or dgefa.
*
*        lda     integer
*                the leading dimension of the array  a .
*
*        n       integer
*                the order of the matrix  a .
*
*        ipvt    integer(n)
*                the pivot vector from dgeco or dgefa.
*
*        b       double precision(n)
*                the right hand side vector.
*
*     on return
*
*        b       the solution vector  x .
*
*     error condition
*
*        a division by zero will occur if the input factor contains a
*        zero on the diagonal.  technically this indicates singularity
*        but it is often caused by improper arguments or improper
*        setting of lda .  it will not occur if the subroutines are
*        called correctly and if dgeco has set rcond .gt. 0.0
*        or dgefa has set info .eq. 0 .
**************************************************************************/
static int
dgesl(double *a, int lda, int n, long *ipvt, double *b)
{
    /* System generated locals */
    int a_dim1, a_offset;

    /* Local variables */
    int k, l;
    double t;
    int kb;

    /* Parameter adjustments */
    --b;
    --ipvt;
    a_dim1 = lda;
    a_offset = a_dim1 + 1;
    a -= a_offset;

    /* Function Body */
   	for (k = 1; k <= n - 1; k++) {
		l = ipvt[k];
		t = b[l];
		if (l != k) {
			b[l] = b[k];
			b[k] = t;
		}
		daxpy(n-k, t, &a[k + 1 + k * a_dim1], &b[k + 1]);
  	}

   	for (kb = 1; kb <= n; kb++) {
		k = n + 1 - kb;
		b[k] /= a[k + k * a_dim1];
		t = -b[k];
		daxpy(k-1, t, &a[k * a_dim1 + 1], &b[1] );
   	}

    return 0;
}



static int
daxpy(int n, double da, double *dx, double *dy)
{
    /* Local variables */
    int i;

    /* Function Body */
    if (da == 0.)
		return 0;
 
   	for (i = 0; i < n; i++) 
		dy[i] += (da * dx[i]);

    return 0;
}


static int dscal(int n, double da, double *dx)
{
    /* Local variables */
    int i;

    for (i=0; i < n; i++)
	dx[i] *= da;

    return 0;
}


static int
idamax(int n, double *dx)
{

    /* Local variables */
    double dmax;
    long i, ret_val;

    /* Function Body */
    ret_val = 1;

   	dmax = ABS(dx[0]);
   	for (i = 1; i < n; i++) {
		if (ABS(dx[i]) <= dmax) 
	    	continue;
		ret_val = i;
		dmax = ABS(dx[i]);
	}
	ret_val++;
    return ret_val;
}


static double epslon(void)
{
    /* Local variables */
    double a, b, c, eps;

    a = 1.3333333333333333;

	do {
		b = a - 1.;
		c = b + b + b;
    	eps = ABS(c-1.);
    } while (eps == 0.);

    return(eps);
}


static int dmxpy(int n1, double *y, int n2, int ldm, double *x, double *m)
{
    /* System generated locals */
    int m_dim1, m_offset, i__1, i__2;

    /* Local variables */
    int jmin, i, j;

    /* Parameter adjustments */
    m_dim1 = ldm;
    m_offset = m_dim1 + 1;
    m -= m_offset;
    --x;
    --y;

    /* Function Body */
    j = n2 % 2;
    if (j >= 1) {
		i__1 = n1;
		for (i = 1; i <= i__1; ++i) {
	    	y[i] += x[j] * m[i + j * m_dim1];
		}
    }

    j = n2 % 4;
    if (j >= 2) {
		i__1 = n1;
		for (i = 1; i <= i__1; ++i) {
		    y[i] = y[i] + x[j - 1] * m[i + (j - 1) * m_dim1] + x[j] * m[i + j 
		      * m_dim1];
		}
    }

    j = n2 % 8;
    if (j >= 4) {
		i__1 = n1;
		for (i = 1; i <= i__1; ++i) {
	   		y[i] = y[i] + x[j - 3] * m[i + (j - 3) * m_dim1] + x[j - 2] * m[i 
		      + (j - 2) * m_dim1] + x[j - 1] * m[i + (j - 1) * m_dim1] 
		      + x[j] * m[i + j * m_dim1];
		}
    }

    j = n2 % 16;
    if (j >= 8) {
		i__1 = n1;
		for (i = 1; i <= i__1; ++i) {
	   		y[i] = y[i] + x[j - 7] * m[i + (j - 7) * m_dim1] + x[j - 6] * m[i 
		      + (j - 6) * m_dim1] + x[j - 5] * m[i + (j - 5) * m_dim1] 
		      + x[j - 4] * m[i + (j - 4) * m_dim1] + x[j - 3] * m[i + (
		      j - 3) * m_dim1] + x[j - 2] * m[i + (j - 2) * m_dim1] + x[
		      j - 1] * m[i + (j - 1) * m_dim1] + x[j] * m[i + j * m_dim1];
		}
    }

    jmin = j + 16;
    i__1 = n2;
    for (j = jmin; j <= i__1; j += 16) {
		i__2 = n1;
		for (i = 1; i <= i__2; ++i) {
	   		y[i] = y[i] + x[j - 15] * m[i + (j - 15) * m_dim1] + x[j - 14] * 
		      m[i + (j - 14) * m_dim1] + x[j - 13] * m[i + (j - 13) * 
		      m_dim1] + x[j - 12] * m[i + (j - 12) * m_dim1] + x[j - 11]
		      * m[i + (j - 11) * m_dim1] + x[j - 10] * m[i + (j - 10) *
		      m_dim1] + x[j - 9] * m[i + (j - 9) * m_dim1] + x[j - 8] *
		      m[i + (j - 8) * m_dim1] + x[j - 7] * m[i + (j - 7) * 
		      m_dim1] + x[j - 6] * m[i + (j - 6) * m_dim1] + x[j - 5] * 
		      m[i + (j - 5) * m_dim1] + x[j - 4] * m[i + (j - 4) * 
		      m_dim1] + x[j - 3] * m[i + (j - 3) * m_dim1] + x[j - 2] * 
		      m[i + (j - 2) * m_dim1] + x[j - 1] * m[i + (j - 1) * 
		      m_dim1] + x[j] * m[i + j * m_dim1];
		}
    }

    return 0;
}

