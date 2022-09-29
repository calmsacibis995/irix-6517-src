#include <sys/types.h>
#include <sys/systm.h>

/* ARGSUSED */
unsigned long sgi_dfs_func_0001(
	int x,
	int y,
	int z,
	unsigned char **in,	/* >= length bytes of inputtext */
	unsigned char **out,	/* >= length bytes of outputtext */
	register long length,	/* in bytes */
	__uint32_t key[32],	/* precomputed key schedule */
	unsigned char iv[8])	/* 8 bytes of ivec */
{
    memset( out, 0, 8) ;
    return 0;
}

/* ARGSUSED */
int sgi_dfs_func_0002(
	int x,
	int y,
	int z,
	unsigned char **k,
	__uint32_t s[32])
{
    memset( s, 0, 128) ;
    return 0;
}

/* ARGSUSED */
int sgi_dfs_func_0003(
	int x,
	int y,
	int z,
	unsigned char **in,
	unsigned char **out,
	__int32_t length,
	__uint32_t schedule[32],
	unsigned char ivec[8],
	int encrypt)
{
    memcpy( out, in, length ) ;
    return 0;
}

/* ARGSUSED */
int sgi_dfs_func_0004(
	int x,
	int y,
	int z,
	unsigned char **in,
	unsigned char **out,
	__uint32_t schedule[32],
	int encrypt)
{
    memcpy( out, in, 8) ;
    return 0;
}

/* ARGSUSED */
int sgi_dfs_func_0005(
	int x,
	int y,
	int z,
	unsigned char key[8])
{
   return 0;
}
