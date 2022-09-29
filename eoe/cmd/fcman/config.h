#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef enum {
  VT_INTEGER=0,
  VT_BOOLEAN,
  VT_STRING,
  VT_STRINGS
} vtype_t;

typedef struct {
  char   *vt_name;		/* Name of variable */
  vtype_t vt_type;		/* Type of parameters expected */
  int     vt_offset;		/* Offset in vars table to put values */
} vt_struct_t;
typedef vt_struct_t *vt_t;

#define OFFSET(_p_type, _field) ((uint) &(((_p_type *)NULL)->_field))
#define SIZE(_p_type, _field)   ((uint) sizeof(((_p_type *)NULL)->_field))

typedef struct {
  uint   ac_pollperiod;		/* Agent poll period */
  uint   ac_flashperiod;	/* Agent LED flash period */
  char  *ac_preremovalco;	/* Agent pre-removal callout */
  char  *ac_postremovalco;	/* Agent post-removal callout */
  char  *ac_postinsertco;	/* Agent post-insert callout */
  char  *ac_stat_change_co;	/* Agent status changed callout */
  uint   ac_debug;		/* Agent debug level */
  uint	 ac_allow_remote;	/* Agent allows remote connections */
} agent_cfg_struct_t;
typedef agent_cfg_struct_t *agent_cfg_t;

int  cfg_read(char *filename, void *vars, vt_t vt);
void cfg_parseerror(const char *s, ...);

extern vt_struct_t agent_vt[];

#endif
