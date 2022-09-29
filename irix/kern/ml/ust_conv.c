#include <sys/types.h>
#include <sys/ktime.h>
#include <sys/pda.h>
#include <sys/systm.h>

/*
 * UST-to-nanosecond conversion code.
 * 
 * PLEASE READ THESE COMMENTS BEFORE ADDING NEW CODE!
 *
 * Conversion from ticks to nanoseconds is accomplished by  multiplying by a 
 * numberin the form (n/d), where (n/d) is ideally 1000/(clock speed in MHz)
 * 
 * N and D are carefully selected according to the following considerations:
 *
 * 1. Remember that for most processors the actual clock speed of the counter
 * is the external clock speed of the processor (e.g. 100MHz for a "200MHz" 
 * processor).
 *
 * 2. The error between (N/D) and the actual conversion factor (1000/MHz) 
 * should be kept reasonably small. Remember that the typical processor 
 * crystal is only accurate to 50 or 100 ppm, so we don't have to be 
 * ludicrously accurate, just << 50ppm, if possible. This is sometimes a 
 * tradeoff with the third constraint.
 *
 * 3. N should be kept as small as possible while satisfying the second 
 * constraint. In other words, reduce the fraction. This is because we multiply 
 * then divide. Multiplication by a large N will cause overflow. The UST 
 * wrap-time must exceed the system up-time! Make n small enough so that you 
 * have months, if not years, before the value wraps. Here's the analysis
 * for the default case (1000/MHz). Let's assume a worst-case of 250MHz (really
 * 125 MHz, according to reminder #1):
 *		2 64^ p
 *		18446744073709551616	# number of counts in unsigned 64-bit #
 *		125000000 / p			
 *		147573952589.676412928	# number of seconds at 125MHz
 *		1000 / p			
 *		147573952.5896764129	# number of seconds when N=1000
 *		60 60 24 * * / p
 *		1708.0318586768		# number of days when N=1000
 *		365 / p
 *		4.6795393388		# number of years when N=1000
 * This should be plenty, including room for faster processors.
 *
 * 4. Remember that you need not use a huge N and D to get an accurate value
 * for N/D. For example, the "175Mhz" R4400 is really externally 87.3702 MHz. 
 * The ideal conversion factor is (1000 / 87.3702) = 11.4455500845. We want 
 * integral N and D such that N/D is very nearly the ideal value. One obvious
 * stab is (114455500/10000000). But the N here is way too large; it makes 
 * the conversion wrap after about 30 minutes. With some analysis, a better
 * fraction is (1156 / 101) = 11.4455445544, which is plenty accurate and 
 * requires years to wrap.
 *
 */

extern int is_fullhouse(void);
static unsigned int ust_tton_n = 1;
static unsigned int ust_tton_d = 1;

unsigned int ust_freq;

unsigned int update_ust_i(void)
{
	unsigned int i;
	int s = spl7();
	i = update_ust();
	splx(s);
	return i;
}

/*
 * get_ust_nano
 * Return Unadjusted System Time in nanoseconds.
 */
void
get_ust_nano(unsigned long long *ull)
{
	*ull = (ust_tton_n * ust_val) / ust_tton_d;
}

/*
 * convert ticks to nanoseconds. This MUST be the same algorithm as used
 * by get_ust_nano.
 */
void
ust_to_nano(unsigned long long val, unsigned long long *ull)
{
	*ull = (ust_tton_n * val) / ust_tton_d;
}

/*
 * get_ust
 * Return Unadjusted System Time in ticks.
 */
void
get_ust(unsigned long long *ull)
{
	*ull = ust_val;
}

/*
 * get updated ust in ticks
 *
 * Call this when NOT at splprof or spl7.
 * If you're ALREADY at splprof or spl7 just do this
 *	update_ust();
 *	snap = ust_val;
 */
stamp_t 
get_updated_ust_i(void)
{
	stamp_t snap;
	int s = spl7();
	update_ust();
	snap = ust_val; /* this only needed inside spl on MIPS1/2 */
	splx(s);
	return snap;	/* ust in ticks */
}


/* assumes that n is always bigger than d.  must revisit if this
 * is ever not the case.
 */
stamp_t
nano_to_ust(stamp_t nano)
{
	return (ust_tton_d * nano) / ust_tton_n;
}

/*
 * Following are the (machine-specific) initialization routines for
 * the frequency of UST and constants to convert to nanoseconds.
 * ust_init is called out of init_tbl, so it gets called after the
 * driver initialization routines.
 */
#if EVEREST
#include <sys/EVEREST/everest.h>
void
ust_init()
{
        ust_freq   = CYCLE_PER_SEC;
        ust_tton_n = NSEC_PER_CYCLE;
        ust_tton_d = 1;
}
#elif IP30
#include <sys/RACER/IP30.h>
void
ust_init()
{
        ust_freq   = HEART_COUNT_RATE;
        ust_tton_n = NSEC_PER_SEC/HEART_COUNT_RATE;
        ust_tton_d = 1;
}
#elif IP32
#include <sys/IP32.h>
void
ust_init()
{
        ust_freq   = MASTER_FREQ/64;
        ust_tton_n = 400321;
        ust_tton_d = 417;
}
/* this function is for use when converting a 32-bit UST value read from
 * a MACE register into a 64-bit UST value, as found in ust_val.  It is
 * crucial that all users of the MACE UST registers fill in the upper 32 bits
 * in a way which is consistent.  Otherwise, there is no meaningful way to
 * compare USTs from different devices!
 *
 * NOTE: MACE's 32-bit UST registers wrap approximately every 4000 seconds.
 * this function assumes that 'maceval' represents a UST time which is within
 * plus or minus half that interval of the current UST. 
 *
 * NOTE: we choose the main CPU-readable UST register in MACE as the master 
 * source of upper bits.  this function assumes that update_ust() or 
 * update_ust_i() has been called recently, so that ust_val contains a 
 * recent sampling of the current UST.  in timer.c for IP32, we call 
 * ust_val once per second, so you should have no problem calling this 
 * routine without having called update_ust_{,_i}() first.
 */
void
fill_ust_highbits(unsigned int mace32, unsigned long long *mace64)
{
  unsigned int ust_lo, ust_hi;

  /* --- fill in low 32 bits of *mace64 */

  *mace64 = (unsigned long long)mace32;

  /* --- fill in high 32 bits of *mace64 */
  /*
   * get a snap of ust_val.  because update_ust() is called from
   * onesec_maint, we know this ust_val is at most two seconds old,
   * which means we don't need to call update_ust() here.
   *
   * we have to spl because update_ust() is called at splprof, and
   * we cannot read 64-bit ust_val atomically from this mips2 C code.
   * XXX perhaps there should be a helper mips3/4 assembly routine 
   * to read ust_val atomically.
   */
  {
    int s = splprof();
    unsigned long long ust_val_snap = ust_val;
    splx(s);
    ust_lo = (unsigned int)ust_val_snap;
    ust_hi = *((unsigned long *)(&ust_val_snap));
  }
  /*
   * we now have two 32-bit MACE timer values: ust_lo (the main system
   * counter) and mace32 (the device-specific counter).  we need to 
   * determine the temporal order of these two counter values.  
   * the assumption is that both ust_lo and mace32 represent times
   * within 1/2 of the 32-bit counter wrap time (roughly 2000 seconds).
   * therefore we can tell whether 'ust_lo' is later or 'mace32' is
   * later by seeing which of (ust_lo-mace32) or (mace32-ust_lo) falls
   * within that interval.  note that we can NOT determine which is later
   * just by looking at (ust_lo < mace32).
   *
   * then, once we know the temporal order of mace32 and ust_lo,
   * we then check to see if the two values are separated by a wrap
   * about the full 32-bit range.  if not, then the upper 32 bits of
   * *mace64 should match those of ust_val_snap.  if so, the upper 32 
   * bits of *mace64 should differ from the upper 32 bits of 
   * ust_val_snap by 1 or -1.
   *
   * this code could undoubtedly be collapsed into one clever and
   * utterly incomprehensible expression involving xors and stuff.
   */
  if (ust_lo - mace32 < 2147483648U) /* this is 2^31, ~2000 seconds */
    {
      /* mace32 is temporally first, then ust_lo */
      if (ust_lo >= mace32) /* mace32->ust_lo does not wrap */
        *((unsigned long *)mace64) = ust_hi;
      else /* mace32->ust_lo wraps */
        *((unsigned long *)mace64) = ust_hi - 1;
    }
  else
    {
      /* ust_lo is temporally first, then mace32 */
      if (mace32 >= ust_lo) /* ust_lo->mace32 does not wrap */
        *((unsigned long *)mace64) = ust_hi;
      else /* ust_lo->mace32 wraps */
        *((unsigned long *)mace64) = ust_hi + 1;
    }
}
#elif SN
void
ust_init()
{
    ust_freq = CYCLE_PER_SEC;
    ust_tton_n = NSEC_PER_CYCLE;
    ust_tton_d = 1;
}
#elif IP26 || R4000 || R10000
void
ust_init()
{
	/*
	 * WARNING: Please don't add a new conversion constant until you
	 * read and understand the comment at the top of this file!
	 */
        int this_cpufreq;
        this_cpufreq = private.cpufreq;

        switch(this_cpufreq) {
#ifdef R4600
		case 66 :
		case 67 :
			if (is_fullhouse()) {
			    /*
			     * Indigo2 133MHz is 66.600 MHz
			     */
			    ust_freq   = 66600000;
			    ust_tton_n = 5000;
			    ust_tton_d = 333;
			}
			else {
			    /*
			     * 133Mhz R4600 is really 66.6666 MHz
			     */
			    ust_freq   = 66666600;
			    ust_tton_n = 15;
			    ust_tton_d = 1;
			}
			break;
#endif 			
#ifdef IP22
		case 87 :
			if (is_fullhouse()) {
			    /*
			     * Indigo2 175MHz is 87.5 MHz
			     */
			    ust_freq   = 87500000;
			    ust_tton_n = 80;
			    ust_tton_d = 7;
			}
			else {
			    /*
			     * Indy "175Mhz" R4400 is really 87.3702 MHz
			     * 1156 / 101 = 11.4455445544
			     * 1000 / 87.3702 = 11.4455500845
			     */
			    ust_freq   = 87370200;
			    ust_tton_n = 1156;
			    ust_tton_d = 101;
			}
			break;
#endif
#ifdef IP28
		/* other cases can be 120Mhz, 150Mhz, 190Mhz and 192Mhz */
		case 87:	/* 175Mhz.0 Mhz */
		case 88:
			ust_freq   = 87500000;
			ust_tton_n = 80;
			ust_tton_d = 7;
			break;
		case 97:	/* 195.0 Mhz */
		case 98:
			ust_freq   = 97500000;
			ust_tton_n = 400;
			ust_tton_d = 39;
			break;
#endif

                default :
			/*
			 * Assumes external CPU frequency is
			 * really integral. If not, add a case above.
			 */
        		ust_freq   = this_cpufreq * 1000000;
                        ust_tton_n = 1000;
                        ust_tton_d = this_cpufreq;
                        break;
        }
}
#endif /* PRODUCT/PROCESSOR type */
