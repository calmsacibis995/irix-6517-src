#ifndef __parser_h_
#define __parser_h_

#define MAXROWS 10
#define TABLE_TYPE 100

#define MAXERRBUF 2000
#define MAXSTRLEN 256
#define MAXIDLEN  80
#define TRUE 1
#define FALSE 0

#define FINDERROR  -1
#define FINDSUCCESS 0

struct mibNode {

		char 	name[32];
		int 	subid;
		int 	syntax;
		char 	parentname[32];
		char 	descr[3000];
		int 	access;     
		int     index;
		int 	table;

		struct  enumList *enum_list;
		struct  mibNode *child;
		struct  mibNode *sibling;
		struct  mibNode *parent;

                void*   group;
                void*   describe;
};

struct enumList {

		 int             value;
		 char            name[32];
		 struct enumList *next;
};

struct typeInfo {

		 char            typedefname[32];
		 int             syntaxtype;
		 struct enumList *list;
		 struct typeInfo *next;
};

#endif
