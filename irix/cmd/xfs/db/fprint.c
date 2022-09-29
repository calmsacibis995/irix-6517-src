#ident	"$Revision: 1.12 $"

#include "versions.h"
#include <sys/types.h>
#include <sys/uuid.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_bmap_btree.h>
#if VERS >= V_62
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_attr_sf.h>
#else
#include <sys/fs/xfs_dir.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "inode.h"
#include "inobt.h"
#include "bit.h"
#include "print.h"
#include "output.h"
#include "sig.h"
#include "malloc.h"

/*ARGSUSED*/
int
fp_charns(
	void	*obj,
	int	bit,
	int	count,
	char	*fmtstr,
	int	size,
	int	arg,
	int	base,
	int	array)
{
	int	i;
	char	*p;

	assert(bitoffs(bit) == 0);
	assert(size == bitsz(char));
	dbprintf("\"");
	for (i = 0, p = (char *)obj + byteize(bit);
	     i < count && !seenint();
	     i++, p++) {
		if (*p == '\\' || *p == '\'' || *p == '"' || *p == '\?')
			dbprintf("\\%c", *p);
		else if (isgraph(*p) || *p == ' ')
			dbprintf("%c", *p);
		else if (*p == '\a' || *p == '\b' || *p == '\f' || *p == '\n' ||
			 *p == '\r' || *p == '\t' || *p == '\v')
			dbprintf("\\%c", *p + ('a' - '\a'));
		else
			dbprintf("\\%03o", *p & 0xff);
	}
	dbprintf("\"");
	return 1;
}

/*ARGSUSED*/
int
fp_num(
	void		*obj,
	int		bit,
	int		count,
	char		*fmtstr,
	int		size,
	int		arg,
	int		base,
	int		array)
{
	int		bitpos;
	int		i;
	int		isnull;
	__int64_t	val;

	for (i = 0, bitpos = bit;
	     i < count && !seenint();
	     i++, bitpos += size) {
		val = getbitval(obj, bitpos, size,
			(arg & FTARG_SIGNED) ? BVSIGNED : BVUNSIGNED);
		if ((arg & FTARG_SKIPZERO) && val == 0)
			continue;
		isnull = (arg & FTARG_SIGNED) || size == 64 ?
			val == -1LL : val == ((1LL << size) - 1LL);
		if ((arg & FTARG_SKIPNULL) && isnull)
			continue;
		if (array)
			dbprintf("%d:", i + base);
		if ((arg & FTARG_DONULL) && isnull)
			dbprintf("null");
		else if (size > 32)
			dbprintf(fmtstr, val);
		else
			dbprintf(fmtstr, (__int32_t)val);
		if (i < count - 1)
			dbprintf(" ");
	}
	return 1;
}

/*ARGSUSED*/
int
fp_sarray(
	void	*obj,
	int	bit,
	int	count,
	char	*fmtstr,
	int	size,
	int	arg,
	int	base,
	int	array)
{
	print_sarray(obj, bit, count, size, base, array,
		(const field_t *)fmtstr, (arg & FTARG_SKIPNMS) != 0);
	return 1;
}

/*ARGSUSED*/
int
fp_time(
	void	*obj,
	int	bit,
	int	count,
	char	*fmtstr,
	int	size,
	int	arg,
	int	base,
	int	array)
{
	int	bitpos;
	char	*c;
	int	i;

	assert(bitoffs(bit) == 0);
	for (i = 0, bitpos = bit;
	     i < count && !seenint();
	     i++, bitpos += size) {
		if (array)
			dbprintf("%d:", i + base);
		c = ctime((time_t *)((char *)obj + byteize(bitpos)));
		dbprintf("%24.24s", c);
		if (i < count - 1)
			dbprintf(" ");
	}
	return 1;
}

/*ARGSUSED*/
int
fp_uuid(
	void	*obj,
	int	bit,
	int	count,
	char	*fmtstr,
	int	size,
	int	arg,
	int	base,
	int	array)
{
	char	*bp;
	int	i;
	uuid_t	*p;
	uint_t	s;

	assert(bitoffs(bit) == 0);
	for (p = (uuid_t *)((char *)obj + byteize(bit)), i = 0;
	     i < count && !seenint();
	     i++, p++) {
		if (array)
			dbprintf("%d:", i + base);
		uuid_to_string(p, &bp, &s);
		dbprintf("%s", bp);
		xfree(bp);
		if (i < count - 1)
			dbprintf(" ");
	}
	return 1;
}
