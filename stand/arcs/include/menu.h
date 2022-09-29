#ifndef _MENU_H
#define _MENU_H

#ident "$Revision: 1.13 $"

/*
 * Menu item definition
 */
typedef struct mitem_s {
	char *prompt;			/* Menu item text */
	int (*service)(void);		/* function to call when selected */
	int flags;			/* menu item flags */
	struct pcbm *bits;		/* color bitmap for button */
	} mitem_t;

#define M_INVALID	1
#define M_PASSWD	2
#define M_GUIONLY	4

/*
 * Menu definition
 */
typedef struct menu_s {
	mitem_t *item;		/* list of menu items */
	int  def;		/* default choice */
	char *title;		/* menu title */
	char *prompt;		/* prompt (e.g."Please choose a number: ") */
	char *help;		/* help text */
	int size;		/* number of items in item[] */
	} menu_t;
#endif

extern void menu_parser(menu_t *);
