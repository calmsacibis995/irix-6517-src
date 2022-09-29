/*
 * Part of this was defined in <sys/types.h> but not all of it,
 * and that really sucks.
 * In bonsai tree all are defined, but just for the backward compatability
 * lets do the define checking here.
 */
#if !defined(uint8_t)
typedef	unsigned char		uint8_t;
#endif
#if !defined(uint16_t)
typedef	unsigned short		uint16_t;
#endif
#if !defined(uint32_t)
typedef	unsigned int		uint32_t;
#endif
#if !defined(int64_t)
typedef	signed long long int	int64_t;
#endif
#if !defined(uint64_t)
typedef	unsigned long long int	uint64_t;
#endif
#if !defined(intmax_t)
typedef signed long long int	intmax_t;
#endif
#if !defined(uintmax_t)
typedef unsigned long long int	uintmax_t;
#endif
#if !defined(intptr_t)
typedef signed long int		intptr_t;
#endif
#if !defined(uintptr_t)
typedef unsigned long int	uintptr_t;
#endif
