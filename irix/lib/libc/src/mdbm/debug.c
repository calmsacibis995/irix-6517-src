#include "synonyms.h"
#include "common.h"

char *
p64(unsigned long long num) 
{
	static	char	buf[20][8];
	static	int	nextbuf = 0;
	char	*s = buf[nextbuf++];
	char	*tag = "\0KMGTPS";
	double	amt = num;
	int	i;
	char	*t;
	
	for (i = 0; i < 7 && amt >= 512; i++) {
		amt /= 1024;
	}
	sprintf(s, "%.2f", amt);
	for (t = s; t[1]; t++);
	while (*t == '0') *t-- = 0;
	if (*t == '.') t--;
	t[1] = tag[i]; t[2] = 0;
	if (nextbuf == 7) nextbuf = 0;
	return (s);
}

static int *count; 
static uint64 byte_used; 
static uint64 page_used; 
static uint64 e_count; /* number of entry */

void
stat_page(MDBM *db, char *pag)
{
	int n;

	if (!(n = NUM(pag))) return;
	
	count[n/2]++;
	byte_used += OFF(pag);
	e_count += NUM(pag);
	page_used++;
}

void
chk_page(MDBM *db, char *pag)
{
        int     i, n;
        uint16  koff = 0, doff;

	if ((pag < db->m_db) || (pag > (db->m_db + (MAX_NPG(db) * PAGE_SIZE(db)))))
                printf("bad page address %d(0x%x)\n", pag, pag);
	if ((int )(TOP(db, pag) - db->m_db) % PAGE_SIZE(db))
		printf("bad page address (algn) %d(0x%x)\n", pag, pag);
        if (NUM(pag) > MAX_FREE(db))
                printf("page (%d): bad n, n=%d\n", PNO(pag), NUM(pag));
        if ((OFF(pag) + INO_TOTAL(pag)) > PAGE_SIZE(db))
                printf("page (%d): bad off, off=%d\n", PNO(pag), OFF(pag));
        n = -NUM(pag);
        for (i = -1; i > n; i -= 2) {
                doff = ntohs(_INO(pag, i));
                if (doff > MAX_FREE(db)) {
                        printf("page (%d) (ent %d): bad doff, doff=%d\n", PNO(pag), -i, doff);
                        continue;
                }
                if ((doff - koff)  <= 0) {
			if ((PNO(pag) != 0) || (i != -1)) {
                        	printf("page (%d): bad key size, key size=%d\n", PNO(pag), doff - koff);
                        	continue;
			}
                }
                koff = ntohs(_INO(pag, i-1));
                if (koff > MAX_FREE(db)) {
                        printf("page (%d)(ent %d) : bad koff, koff=%d\n", PNO(pag), -(i-1), koff);
                        continue;
                }
                if ((koff - doff)  < 0) {
                        printf("page (%d): bad val size, val size=%d\n", PNO(pag), koff - doff);
                        continue;
                }
        }
}

void
dump_page(MDBM *db, char *pag)
{
	datum   key;
	datum   val;
	int	i, n;
	uint16	koff = 0, doff;
	char buf[1024];
	char *p;

	chk_page(db, pag);

	n = -NUM(pag);
	p  = TOP(db, pag);

	printf("total number of entry in page(%d): %d(%d)\n", PNO(pag), -n, -n/2);
	for (i = -1; i > n; i -= 2) {
		key.dptr = p + koff;
		doff = ntohs(_INO(pag, i));
		key.dsize = doff - koff;
		memcpy(buf, key.dptr, key.dsize);
#if 0
		buf[key.dsize] = 0;
		printf("pageno %u:          kentry[%d](sz=%d))(off=%d)(ptr=%d)= %s\n", PNO(pag), -i, key.dsize, doff, key.dptr, buf);
#else
		buf[key.dsize -1] = 0;
		printf("pageno %u:          kentry[%d](sz=%d))(off=%d)(ptr=%d)= %s\n", PNO(pag), -i, key.dsize, doff, key.dptr, buf);
#endif
		val.dptr = p + doff;
		koff = ntohs(_INO(pag, i-1));
		val.dsize = koff - doff;
#if 0
		memcpy(buf, val.dptr, val.dsize);
		buf[val.dsize] = 0;
		printf("pageno %u:          ventry[%d](sz=%d))(off=%d)(ptr=%d)= %s\n", PNO(pag), -(i-1), val.dsize, doff, val.dptr, &buf[6]);
#endif
	}
}


void
count_page(MDBM *db, char *pag)
{
	e_count += NUM(pag);
}

/*
 *  recursive tree walk function
 */
void
walk_all_node(MDBM *db, uint64 dbit, uint64 pageno, int level, void (*f)(MDBM *, char *))
{

	if (dbit < (MAX_NPG(db) - 1) && GETDBIT(db->m_dir, dbit)) {
		dbit <<= 1;
		walk_all_node(db, ++dbit, pageno, level+1, f); /*left*/
		walk_all_node(db, ++dbit, (pageno | (1LL<<level)), level+1, f); /* right */
	} else  (*f)(db,PAG_ADDR(db, pageno));
}

void
mdbm_dump_all_page(MDBM *db)
{
	walk_all_node(db, 0, 0, 0, dump_page);
}

void
mdbm_chk_all_page(MDBM *db)
{
	walk_all_node(db, 0, 0, 0, chk_page);
}

void
mdbm_stat_all_page(MDBM *db)
{
	e_count = 0;
	count = calloc(PAGE_SIZE(db)/2, sizeof(int));
	page_used = byte_used = 0;
	walk_all_node(db, 0, 0, 0, stat_page);
	printf("MMAP SPACE page efficiency: %lld %%; (%lld/%lld)\n", (uint64) (page_used * 100LL / (uint64) MAX_NPG(db)), page_used, (uint64) MAX_NPG(db)); 
	printf("MMAP SPACE byte efficiency: %lld %%; (%s/%s)\n", (uint64) (byte_used * 100LL / ((uint64) MAX_NPG(db) * PAGE_SIZE(db))), p64(byte_used), p64((uint64) (MAX_NPG(db) * PAGE_SIZE(db)))); 
	printf("DISK SPACE efficiency:      %lld %%; (%s/%s) bytes used\n", (uint64) (byte_used * 100LL / (page_used * (uint64) PAGE_SIZE(db))), p64(byte_used), p64((page_used * (uint64) PAGE_SIZE(db)))); 
	e_count -= 2; /* igonore the header block */
	e_count >>= 1; /* divide by two */
	printf("Total number of entry: %lld\n", e_count);
	printf("Maximum B-tree level:  %d\n", db->m_npgshift);
	printf("Minimum B-tree level:  %d\n", min_level(db));
}

uint64
count_all_page(MDBM *db)
{
	e_count = 0;
	walk_all_node(db, 0LL, 0LL, 0, count_page);
	e_count -= 2; /* igonore the header block */
	e_count >>= 1; /* divide by two */
	return e_count;
}
