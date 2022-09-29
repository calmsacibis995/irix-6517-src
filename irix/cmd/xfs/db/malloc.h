#ident "$Revision: 1.1 $"

extern void	*xcalloc(size_t nelem, size_t elsize);
extern void	xfree(void *ptr);
extern void	*xmalloc(size_t size);
extern void	*xrealloc(void *ptr, size_t size);
extern char	*xstrdup(const char *s1);
