/*
 * List type used to initialize the display.
 */

/*
 * Some fundamental constants.  This makes the programming easier (and
 * the program smaller) but it loses some flexibility.
 */

# define	FIELDSIZE	20
# define	TEXTSIZE	18
# define	MARGIN		((FIELDSIZE - TEXTSIZE)/2)

/*
 * Main list element.  Holds all information about a thing that we are
 * displaying.
 */

typedef struct tmp10 {
	int	flags;		/* action flags */
	int	oflags;		/* detect state change */
	int	xpos;		/* screen x origin */
	int	ypos;		/* screen y origin */
	char	name[TEXTSIZE];		/* field name */
	char	format[TEXTSIZE];	/* format string */
	void	(*fillfunc)();	/* function when re-display called */
	long	fillarg;	/* single argument to function */
	struct tmp10	*next;	/* next element in list */
} Reg_t;

typedef struct {
	int	flags;		/* what flags should be */
	char	name[TEXTSIZE];		/* name of field */
	void	(*fillfunc)();	/* called only if header */
	long	fillarg;	/* only if header */
	Reg_t	*(*expandfunc)();/* function on expand request for header */
} initReg_t;

# define	RF_HEADER	01	/* if this is a header element */
# define	RF_VISIBLE	02	/* if physically visible */
# define	RF_ON		04	/* if logically visible */
# define	RF_EXPAND	010	/* expand header definition */
# define	RF_SUPRESS	020	/* elements hidden */
# define	RF_BOLD		040	/* bold highlight item */
# define	RF_USERF1	01000	/* user flags */
# define	RF_USERF2	02000	/* user flags */
# define	RF_USERF3	04000	/* user flags */

extern Reg_t	*pfollow(Reg_t *);
extern Reg_t	*pfind(int, int);
