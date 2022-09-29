/* __file__ =============================================== *
 *
 *  KeySyms converter. This is equivalent to a copy.
 */

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1','CZZ_TYPE()`)
define(`IDX_SCALAR_TYPE','CZZ_TYPE()`)
define(`KEYS_TABLE_TYPE','CZZ_NAME()`_keys_type)
')
CZZ_TNAME()

/* === Following found in __file__ line __line__ ============= */

#define KEYS_HASH_FUNC( h_key, h_val, h_num )                     \
     ( (                                                          \
          (((h_key) >> ((h_val)[0])) ^ ((h_key) << ((h_val)[1]))) \
         * ((h_val)[2]) + (h_num)                                 \
       ) % (h_num)                                                \
     )

typedef struct HASH_ELEMENT {
    unsigned int            key_val;
    int                     cnv_val;
} HASH_ELEMENT;

typedef struct HASH_PARAMS {
    unsigned int            func_vals[4];
    unsigned int            num_elements;
} HASH_PARAMS;

typedef struct KEYS_TABLE_TYPE {
    int			    version;
    int                     offset_to_hash_table;
    HASH_PARAMS             hash_params[1];
    unsigned int	    maxtval;
    IDX_SCALAR_TYPE	    ttable[1];
} KEYS_TABLE_TYPE;

/* === Previous code found in __file__ line __line__ ========= */


define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    ''KEYS_TABLE_TYPE``	  * p_keystab$1;
    'HASH_ELEMENT`        * p_hashtab$1;
    'HASH_ELEMENT`        * p_hash_entry$1;
    'IDX_SCALAR_TYPE`       hashval$1;
    'IDX_SCALAR_TYPE`       hash_num_count$1;
    'IDX_SCALAR_TYPE`	    xval$1;
    'IDX_SCALAR_TYPE`	    tblmax$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    p_keystab$1 = ( ''KEYS_TABLE_TYPE`` * ) handle[ 'czz_count(`czz_num')` ];
    tblmax$1 = p_keystab$1->maxtval;
    p_hashtab$1 = ( ''HASH_ELEMENT`` * ) (  p_keystab$1->offset_to_hash_table
                                          + (char *) p_keystab$1  );
    xval$1 = 0;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */

    if ( 'RETURN_VAR` > tblmax$1 ) {
          hashval$1 = KEYS_HASH_FUNC( 'RETURN_VAR`, 
                                       p_keystab$1->hash_params->func_vals, 
                                       p_keystab$1->hash_params->num_elements
                                    );


          p_hash_entry$1 = p_hashtab$1 + hashval$1;

          hash_num_count$1 = 0;

          while ( p_hash_entry$1->key_val != 'RETURN_VAR` ) {

                  if ( (!p_hash_entry$1->key_val) || 
                       (hash_num_count$1 > p_keystab$1->hash_params->num_elements) ) {

                        'OUT_CHAR_NOT_FOUND`
                         xval$1 = 0;
                         goto not_found$1;

                  } else {                  /* Potential colission ! */ 
                         p_hash_entry$1 ++;
                         hashval$1 ++;

                         if ( hashval$1 >= p_keystab$1->hash_params->num_elements ) {
                              hashval$1 = 0;
                              p_hash_entry$1 = p_hashtab$1;
                         }
                         hash_num_count$1 ++;
                  }
          }
          xval$1 = p_hash_entry$1->cnv_val;

    } else {
          xval$1 = p_keystab$1->ttable[ 'RETURN_VAR` ];
          if ( ! xval$1 && 'RETURN_VAR` ) {
                'OUT_CHAR_NOT_FOUND`
          }
    }

not_found$1:

    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',xval$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`KEYS_TABLE_TYPE')
    undefine(`IDX_SCALAR_TYPE')
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
