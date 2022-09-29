#ifndef MAC_UTIL_H
#define MAC_UTIL_H

#ident "$Revision: 1.1 $"

struct label_segment
{
	unsigned char  type;
	unsigned char  level;
	unsigned short count;
	unsigned short *list;
};
typedef struct label_segment label_segment, *label_segment_p;

int __check_setvalue(const unsigned short *, unsigned short);
int __segment_equal(const label_segment_p, const label_segment_p);
int __tcsec_dominate(const label_segment_p, const label_segment_p);
int __biba_dominate(const label_segment_p, const label_segment_p);

int __msen_from_text(label_segment_p, char *);
int __mint_from_text(label_segment_p, char *);

int __msen_to_text(label_segment_p, char *);
int __mint_to_text(label_segment_p, char *);

char *__mac_spec_from_alias(const char *);
char *__mac_alias_from_spec(const char *);

#endif /* MAC_UTIL_H */
