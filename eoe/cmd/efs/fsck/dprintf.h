#ident "$Revision: 1.3 $"

# ifdef DEBUG
extern short DEBUG;
#	define dprintf(x)	(DEBUG?printf x:0)
#	define ASSERT(c)	if(!(c))_assert("c",__FILE__,__LINE__)
# else  /* DEBUG */
#	define dprintf(x)
#	define ASSERT(c)
# endif /* DEBUG */
