#ident "$Revision: 1.7 $"

/*
 * Convert a 32 bit mask to a sigset_t and visa-versa
 */
#define	mask2set(mask, setp) \
	((mask) == -1 ? (unsigned int)sigfillset(setp) : (((setp)->__sigbits[0]) = (unsigned int)(mask)))
#define set2mask(setp) ((setp)->__sigbits[0])

#if !defined(_BSD_SIGNALS)
#define MAXBITNO (32)
#define sigword(n) ((n-1)/MAXBITNO)
/* bit mask position within a word, to be used with sigword() or word 0 */
#undef sigmask	/* make sure local sigmask is used */
#define sigmask(n) (1L<<((n-1)%MAXBITNO))
#endif
