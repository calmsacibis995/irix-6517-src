#ifndef _load_h
#define _load_h

#define LOAD_CPU_MAX		128
#define LOAD_DATA_SIZE(ncpus)	((ncpus) * 5)
#define LOAD_DATA_MAX		LOAD_DATA_SIZE(LOAD_CPU_MAX)

extern	int		load_cpu_get(void);
extern	void		load_cpu_set(int ncpus_override);

extern	u_char	       *load_graph(int include_gfx,
				   double decay_per_second,
				   double update_period);

#endif /* _load_h */
