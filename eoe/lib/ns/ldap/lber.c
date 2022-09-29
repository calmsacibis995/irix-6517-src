#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <alloca.h>
#include <errno.h>
#include <unistd.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "lber.h"
#ifdef _SSL
#include <bsafe.h>
#include <ssl.h>
#endif

extern int errno;

int ber_make_obj(struct obj *o, long t, long a, void *d, struct obj *n)
{
	o->type 	= t;
	o->atype 	= a;
	o->data 	= d;
	o->next 	= n;

	return 0;
}

int ber_iovec_free(struct iovec_w *v)
{
	struct iovec_w *vn;

	while (v) {
		vn = v->next;
		free(v->iov.iov_base);
		free(v);
		v = vn;
	}
	
	return 0;
}

int ber_length_encode(char *out, unsigned long l)
{
	int	cnt;
	long	mask;

	if (l < 128) {
		out[0] = (unsigned char) l;
		return 1;
	}

	for (cnt = sizeof(long); cnt > 1; cnt--) {
		mask = 0xffL << ((cnt - 1) * 8);
		if (l & mask) break;
	}

	out[0] = 0x80 | (unsigned char) cnt;
	memcpy(out + 1, (char *) &l + sizeof(long) - cnt, cnt);

	return cnt + 1;
}

long ber_length_decode(struct ber *ber)
{
	unsigned char	c;
	unsigned int	cnt;
	long		ret;

	if (ber->end - ber->ptr < 1) return -1;

	c = *(ber->ptr++);
	if (c & 0x80) {
		/* long form */

		cnt = c & 0x7f;
		if (ber->end - ber->ptr < cnt) return 1;
	
		ret = 0;
		while (cnt--) {
			c = *(ber->ptr++);
			ret = 256 * ret + c;
		}		

		return ret;
	} else {
		/* short form */

		return c;
	}
}

int ber_encode_wrap(char **buf, struct obj *o)
{
	struct iovec_w	*vn, *vec = 0;
	char		*p;

	p = *buf = (char *) malloc(4096);

	ber_encode(&vec, o);
	
	for (vn = vec; vn; vn = vn->next) {
		memcpy(p, vn->iov.iov_base, vn->iov.iov_len);
		p += vn->iov.iov_len;
	}

	ber_iovec_free(vec);

	return p - *buf;
}	

int ber_write(sockbuf sb, struct obj *o)
{
	struct iovec_w	*vn, *vec = 0;
	char		*p, *buf;
	int		rc;

	ber_encode(&vec, o);

#ifdef _SSL
	if (sb.opt & LBER_OPT_SSL) {
		/* XXX check size */

		buf = (char *) alloca(4096);
		p = buf;

		for (vn = vec; vn; vn = vn->next) {
			memcpy(p, vn->iov.iov_base, vn->iov.iov_len);
			p += vn->iov.iov_len;
		}

		rc = ssl_write(sb.ssl, buf, p - buf);
	} else 
#endif
		rc = ber_writev(sb, vec);

	ber_iovec_free(vec);

	return rc;
}

int ber_encode(struct iovec_w **h, struct obj *o)
{
	long		*val, mask;
	unsigned long	len;
	int		rc, i, j, sign, cnt;
	struct iovec_w	*ret, *stp, *vec;
	struct obj	*o_r;
	char		*p_buf, *p, len_b[5];

	obj_int		*o_int;
	obj_bit		*o_bit;
	obj_string	*o_string;
	obj_oid		*o_oid;

	switch (((o->type & 0xc0) ? o->atype : o->type) & 0x1F) {

	case LBER_TYPE_BOOL:
	case LBER_TYPE_ENUM:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		ret->iov.iov_base = (void *) malloc(3 * sizeof(char));
		rc = ret->iov.iov_len = 3;

		val = (long *) o->data;

		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		*p++ = (char) 1;
		*p++ = (char) *val;
		break;

	case LBER_TYPE_INT:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		o_int = (obj_int *) o->data;
		sign = (*o_int < 0);
		
		for (cnt = sizeof(obj_int) - 1; cnt > 0; cnt--) {
			mask = 0xffL << (cnt * 8);

			if (sign) {
				if ((*o_int & mask) != mask) break;
			} else {
				if (*o_int & mask) break;
			}
		}

		mask = *o_int & (0x80L << (cnt * 8));
		if ((mask && ! sign) || (! mask && sign)) cnt++;

		cnt++;
		
		ret->iov.iov_base = (void *) malloc((2 + cnt) * sizeof(char));
		rc = ret->iov.iov_len = 2 + cnt;
		
		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		*p++ = (char) cnt;
		memcpy(p, (char *) o_int + sizeof(obj_int) - cnt, cnt);
		break;

	case LBER_TYPE_BIT:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		o_bit = (obj_bit *) o->data;

		cnt = ber_length_encode(len_b, o_bit->len + 1);

		rc = ret->iov.iov_len = cnt + o_bit->len + 2;
		ret->iov.iov_base = 
			(void *) malloc(ret->iov.iov_len * sizeof(char));

		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		memcpy(p, len_b, cnt);
		p[cnt] = (char) o_bit->pad;
		memcpy(p + cnt + 1, o_bit->data, o_bit->len);
		break;		

	case LBER_TYPE_OCTET:
	case LBER_TYPE_PSTR:
	case LBER_TYPE_TSTR:
	case LBER_TYPE_ISTR:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		o_string = (obj_string *) o->data;

		cnt = ber_length_encode(len_b, o_string->len);

		rc = ret->iov.iov_len = cnt + o_string->len + 1;
		ret->iov.iov_base =
			(void *) malloc(ret->iov.iov_len * sizeof(char));

		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		memcpy(p, len_b, cnt);
		memcpy(p + cnt, o_string->data, o_string->len);
		break;	

	case LBER_TYPE_NULL:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;
		rc = ret->iov.iov_len = 2;
		ret->iov.iov_base = (void *) malloc(2 * sizeof(char));
		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		*p++ = (char) 0;
		break;

	case LBER_TYPE_OID:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		o_oid = (obj_oid *) o->data;

		p_buf = (char *) alloca(256);
		p = p_buf;

		*p++ = 40 * o_oid->data[0] + o_oid->data[1];
		for (cnt = 2; cnt < o_oid->num; cnt++) {
			for (i = 4; i > 1; i--) {
				mask = 0x7fL << ((i - 1) * 7);
				if (mask & o_oid->data[cnt]) break;
			}
			for (j = i; j > 1; j--) {
				mask = 0x7fL << ((j - 1) * 7);
				*p++ = 0x80 | ((mask & o_oid->data[cnt]) 
					>> ((j - 1) * 7));
			}
			*p++ = 0x7f & o_oid->data[cnt];
		}

		len = p - p_buf;
		cnt = ber_length_encode(len_b, len);

		rc = ret->iov.iov_len = cnt + len + 1;
		ret->iov.iov_base =
			(void *) malloc(ret->iov.iov_len * sizeof(char));

		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		memcpy(p, len_b, cnt);
		memcpy(p + cnt, p_buf, len);
		break;

	case LBER_TYPE_SEQ:
		ret = (struct iovec_w *) malloc(sizeof(struct iovec_w));
		ret->next = (struct iovec_w *) 0;

		rc = len = 0;

		for (o_r = (struct obj *) o->data; o_r; o_r = o_r->next) {
			rc += ber_encode(&vec, o_r);
			stp = ret;
			while (stp->next) stp = stp->next;
			stp->next = vec;

			stp = vec;
			len += stp->iov.iov_len;
			while (stp = stp->next) len += stp->iov.iov_len;
		}

		cnt = ber_length_encode(len_b, len);

		ret->iov.iov_base = (void *) malloc((cnt + 1) * sizeof(char));
		rc += ret->iov.iov_len = cnt + 1;

		p = ret->iov.iov_base;
		*p++ = (char) o->type;
		memcpy(p, len_b, cnt);
		break;
	
	}

	*h = ret;
	return rc;
}

int ber_writev(sockbuf b, struct iovec_w *v)
{
	int		rc = 0, cnt = 0;
	struct iovec_w	*vn;
	struct iovec	iov[16];

	for (vn = v; vn; vn = vn->next) {
		iov[cnt++] = vn->iov;
		if (cnt == 16) {
			rc += writev(b.fd, iov, cnt);
			cnt = 0;
		}
	}
	if (cnt) 
		rc += writev(b.fd, iov, cnt);

	return rc;
}

/*
** ber_read_more
**
** this routine handles the "slow path" case that an individual
** ber packet was spread over more than one read.
*/
int ber_read_more(sockbuf *buf, int n)
{
	int	rc, r = 0, delta, n_read;
	char	b[LBER_BUF_SIZE], *old_buf;

#ifdef _SSL
	/* XXX fix ber_read_more for ssl option */
	if (buf->opt & LBER_OPT_SSL) {
		printf("ber_read_more fails for ssl opt\n");
		return -1;
	}
#endif

	while (r < n) {
		n_read = (n - r > LBER_BUF_SIZE) ? LBER_BUF_SIZE : n - r;
		rc = read(buf->fd, b, n_read);
		if (rc == -1) {
			return errno == EAGAIN ? -2 : -1;
		}

		if (buf->end + rc - buf->buf > buf->size) {
			old_buf = buf->buf;
			buf->size += rc;
			buf->buf = (char *) realloc(buf->buf, buf->size);
			delta = buf->buf - old_buf;
			buf->end += delta;
			buf->ptr += delta;
		}

		memcpy(buf->end, b, rc);
		buf->end += rc;

		r += rc;
	}

	return r;
}
		
/*
** ber_read
**
** returns:	-2	would block
**		-1	other error
**		0	0 read == close
**		> 0	O.K., # bytes read (will actually be >= 2)
*/
int ber_read(sockbuf *buf, struct ber *b)
{
	int		diff, rc, rt = 0;
	unsigned int	cnt;
	char		c;

	/* XXX 0 byte read is a close */

	if (buf->ptr == buf->end) {
#ifdef _SSL
		if (buf->opt & LBER_OPT_SSL)
			rc = ssl_read(buf->ssl, buf->buf, LBER_BUF_SIZE);
		else
#endif
			rc = read(buf->fd, buf->buf, LBER_BUF_SIZE);

		if ((rc < 0) && (errno != EAGAIN)) {
			nsd_logprintf(0, "ldap: ber_read: read()->%d, %s\n",
				      rc, strerror(errno));
		}

		if (rc == -1) return errno == EAGAIN ? -2 : -1;
		if (rc == 0) return 0;

		buf->ptr = buf->buf;
		buf->end = buf->buf + rc;
	}

	if ((diff = (buf->end - buf->ptr)) < 2) 
		if ((rc = ber_read_more(buf, 2 - diff)) < 0) return rc;

	b->tag = (unsigned char) *buf->ptr++;
	c = (unsigned char) *buf->ptr++;

	if (c & 0x80) {
		/* long form */

		cnt = c & 0x7f;

		if ((diff = (buf->end - buf->ptr)) < cnt) 
			if ((rc = ber_read_more(buf, cnt - diff)) < 0) 
				return rc;

		rt = cnt = c & 0x7f;
		b->length = 0;
		while (cnt--) {
			c = (unsigned char) *buf->ptr++;
			b->length = 256 * b->length + c;
		}
	} else {
		/* short form */

		b->length = (unsigned long) c;
	}

	/*
	** this is a strange, but valid case.  application beware!
	*/
	if (b->length == 0) {
		b->ptr = b->end = b->data = (char *) 0;
		return rt + 2;
	}

	if ((diff = (buf->end - buf->ptr)) < b->length)
		if ((rc = ber_read_more(buf, b->length - diff)) < 0) return rc;

	b->data = (char *) malloc(b->length);
	memcpy(b->data, buf->ptr, b->length);
	b->ptr = b->data;
	b->end = b->ptr + b->length;

	buf->ptr += b->length;

	return rt + b->length + 2;
}

int ber_read_string(struct ber *out, char *in, int len)
{
	char		c;
	int		cnt, l;

	if (len < 2) return 1;

	c = *in++;
	out->tag = c;

	c = *in++;
	len -= 2;
	if (c & 0x80) {
		cnt = c & 0x7f;
		if (len < cnt) return 1;
		len -= cnt;
	
		l = 0;
		while (cnt--) {
			c = *in++;
			l = 256 * l + c;
		}		
	} else {
		l = c;
	}

	if (len < l) return 1;	

	out->data = in;
	out->end = in + l;
	out->ptr = in;
	out->length = l;

	return 0;
}

int ber_skip_obj(struct ber *in)
{
	char		c;
	unsigned long	len;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);
	len = ber_length_decode(in);

	if (in->end - in->ptr < len) return 1;
	in->ptr += len;

	return 0;
}
	
int ber_get_int(obj_int *out, struct ber *in)
{
	char		c;
	int		cnt, i;
	unsigned long	len;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);

/* take this check out?
	if (c != LBER_TYPE_INT) return 1;
*/

	c = *(in->ptr++);
	if (c & 0x80) return 1;

	len = (unsigned long) c;

	if (len > sizeof(obj_int)) return 1;

	if (in->end - in->ptr < len) return 1;

	*out = 0;
	memcpy((char *) out + sizeof(obj_int) - len, in->ptr, len);

	in->ptr += len;

	if (((0x80 << ((len - 1) * 8)) & *out)
		&& len < sizeof(obj_int)) {
		for(i = sizeof(obj_int) - 1; i > len - 1; i--)
			*out |= (0xffL << (i * 8));
	}

	return 0;
}

int ber_get_bit(obj_bit *out, struct ber *in)
{
	char	c;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);
	if (c != LBER_TYPE_BIT) return 1;

	out->len = ber_length_decode(in) - 1;

	if (in->end - in->ptr < out->len + 1) return 1;

	out->pad = *(in->ptr++);

	if (out->len > 0) out->data = (char *) malloc(out->len);
	memcpy(out->data, in->ptr, out->len);

	return 0;
}

int ber_get_string(obj_string *out, struct ber *in, int type)
{
	char	c;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);
	if (c != type) return 1;

	out->len = ber_length_decode(in);

	if (in->end - in->ptr < out->len || out->len < 0) return 1;

	out->data = (char *) malloc(out->len + 1);
	memcpy(out->data, in->ptr, out->len);
	*(out->data + out->len) = (char) 0;

	in->ptr += out->len;

	return 0;
}

int ber_get_null(struct ber *in)
{
	char	c;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);
	if (c != LBER_TYPE_NULL) return 1;

	c = *(in->ptr++);
	if (c != 0) return 1;

	return 0;
}

int ber_get_oid(obj_oid *out, struct ber *in)
{
	char	*p, c;
	int	cnt = 0;
	long	val, len, tmp[40];

	if (in->end - in->ptr < 2) return 1;
	
	c = *(in->ptr++);
	if (c != LBER_TYPE_OID) return 1;

	len = ber_length_decode(in);

	if (in->end - in->ptr < len) return 1;

	c = *(in->ptr);
	tmp[cnt++] = c / 40;
	tmp[cnt++] = c % 40;

	for (p = in->ptr + 1; p < in->ptr + len; p++) {
		val = *p & 0x7f;
		while (*p & 0x80) val = 128 * val + (*(++p) &0x7f);
		tmp[cnt++] = val;
	}

	in->ptr += len;

	out->data = (long *) malloc(cnt * sizeof(long));
	memcpy(out->data, tmp, cnt * sizeof(long));

	out->num = cnt;

	return 0;
}

int ber_peek_tag(struct ber *b)
{
	char	c;

	if (b->end - b->ptr < 1) return -1;

	c = *(b->ptr);
	return c;
}

int ber_get_sequence(struct ber *out, struct ber *in)
{
	char 	c;

	if (in->end - in->ptr < 2) return 1;

	c = *(in->ptr++);
	out->tag = (unsigned long) c;

	out->length = ber_length_decode(in);

	if (in->end - in->ptr < out->length) return 1;

	out->data = in->ptr;
	out->ptr = in->ptr;
	out->end = in->ptr + out->length;

	in->ptr += out->length;

	return 0;
}

int ber_get_time(obj_time *out, struct ber *in)
{
	int		len, tmp;
	struct tm	tm;
	char		c;

	if (in->end - in->ptr < 2) return 1;
	
	c = *(in->ptr++);
	if (c != LBER_TYPE_TIME) return 1;

	len = ber_length_decode(in);

	if (in->end - in->ptr < len) return 1;
	if (len < 11 || len > 17) return 1;

	tmp = 10 * (*(in->ptr++) - 0x30);
	tmp += (*(in->ptr++) - 0x30);
	tm.tm_year = tmp < 0 ? tmp + 100 : tmp;

	tm.tm_mon = 10 * (*(in->ptr++) - 0x30);
	tm.tm_mon += (*(in->ptr++) - 0x30);
	tm.tm_mon -= 1;
	tm.tm_mday = 10 * (*(in->ptr++) - 0x30);
	tm.tm_mday += (*(in->ptr++) - 0x30);
	tm.tm_hour = 10 * (*(in->ptr++) - 0x30);
	tm.tm_hour += (*(in->ptr++) - 0x30);
	tm.tm_min = 10 * (*(in->ptr++) - 0x30); 
	tm.tm_min += (*(in->ptr++) - 0x30);

	if (len == 13 || len == 17) {
		tm.tm_sec = 10 * (*(in->ptr++) - 0x30);
		tm.tm_sec += (*(in->ptr++) - 0x30);	
	} else
		tm.tm_sec = 0;

	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = 0;

	*out = mktime(&tm);

	switch (c = *(in->ptr++)) {

	case 'Z':
		break;
	case '+':
	case '-':
		if (in->end - in->ptr < 4) return -1;
		tmp = 60 * 10 * (*(in->ptr++) - 0x30);
		tmp += 60 * (*(in->ptr++) - 0x30);
		tmp += 10 * (*(in->ptr++) - 0x30);
		tmp += (*(in->ptr++) - 0x30);
		*out += (c == '+') ? -1 * 60 * tmp : 60 * tmp;
		break;
	default:
		return -1;

	}

	ctime(out);
	*out -= timezone;		/* mktime adjusts for local
					   timezone.  we need to 
					   adjust back
					*/
	return 0;
}
