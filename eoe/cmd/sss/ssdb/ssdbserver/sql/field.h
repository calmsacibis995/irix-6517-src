/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.
   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/*
** Because of the function new_field all field classes that have static
** variables must declare the size_of() member function.
*/

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class Send_field;
struct st_cache_field;

class Field :public Sql_alloc {
  Field(const Item &);				/* Prevent use of theese */
  void operator=(Field &);
public:
  enum utype { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
	       CHECK,EMPTY,UNKNOWN,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,BIT_FIELD,
	       TIMESTAMP_FIELD,CAPITALIZE,BLOB_FIELD};
  char	*ptr;				// Position to field in record
protected:

  uchar		*null_ptr;		// Byte where null_bit is
  uint8		null_bit;		// And position to it

public:
  struct st_table *table;		// Pointer for table
  ulong query_id;			// For quick test of used fields
  key_map key_start,part_of_key;	// Which keys a field is in
  char	*table_name,*field_name;
  utype unireg_check;
  uint32 field_length;			// Length of field
  uint16 flags;

  Field(char *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,uint null_bit_arg,
	utype unireg_check_arg,char *field_name_arg,
	struct st_table *table_arg);
  virtual ~Field() {}
  virtual void store(const char *to,uint length)=0;
  virtual void store(double nr)=0;
  virtual void store(longlong nr)=0;
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
  virtual String *val_str(String*,String *)=0;
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  virtual bool eq(Field *field) { return ptr == field->ptr; }
  virtual uint32 pack_length() const { return (uint32) field_length; }
  virtual void reset(void) { bzero(ptr,pack_length()); }
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline int cmp(const char *str) { return cmp(ptr,str); }
  virtual int cmp(const char *,const char *)=0;
  virtual void sql_type(String &str) const =0;
  virtual uint size_of() const =0;			// For new field
  inline bool is_null(uint offset=0)
    { return null_ptr ? (null_ptr[offset] & null_bit ? 1 : table->null_row) : table->null_row; }
  inline void set_null(int offset=0)
    { if (null_ptr) null_ptr[offset]|= null_bit; }
  inline void set_notnull(int offset=0)
    { if (null_ptr) null_ptr[offset]&= ~null_bit; }
  inline bool maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  virtual void make_field(Send_field *)=0;
  virtual void sort_string(char *buff,uint length)=0;
  virtual bool optimize_range() { return 1; }
  virtual bool store_for_compare() { return 0; }
  inline Field *new_field(struct st_table *new_table)
    {
      Field *tmp= (Field*) sql_memdup((char*) this,size_of());
      tmp->table=new_table;
      tmp->key_start=tmp->part_of_key=0;
      return tmp;
    }
  inline void move_field(char *ptr_arg,uchar *null_ptr_arg,uint null_bit_arg)
    {
      ptr=ptr_arg; null_ptr=null_ptr_arg; null_bit=null_bit_arg;
    }
  inline void move_field(char *ptr_arg) { ptr=ptr_arg; }
  inline void get_image(char *buff,uint length)
    { memcpy(buff,ptr,length); }
  inline void set_image(char *buff,uint length)
    { memcpy(ptr,buff,length); }
  inline int cmp_image(char *buff,uint length)
    {
      if (binary())
	return memcmp(ptr,buff,length);
      else
	return my_casecmp(ptr,buff,length);
    }
  inline longlong val_int_offset(uint offset)
    {
      ptr+=offset;
      longlong tmp=val_int();
      ptr-=offset;
      return tmp;
    }
  bool send(String *packet);
  uint offset();				// Should be inline ...
  void copy_from_tmp(int offset);
  uint fill_cache_field(struct st_cache_field *copy);
  friend bool reopen_table(THD *,struct st_table *,bool);
  friend class Copy_field;
  friend class Item_avg_field;
  friend class Item_std_field;
  friend class Item_sum_num;
  friend class Item_sum_sum;
  friend class Item_sum_str;
  friend class Item_sum_count;
  friend class Item_sum_avg;
  friend class Item_sum_std;
  friend class Item_sum_min;
  friend class Item_sum_max;
};


class Field_num :public Field {
public:
  const uint8 decimals;
  bool zerofill,unsigned_flag;		// Purify cannot handle bit fields
  Field_num(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg, 
	    uint null_bit_arg, utype unireg_check_arg, char *field_name_arg,
	    struct st_table *table_arg,
	    uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	   unireg_check_arg, field_name_arg, table_arg),
     decimals(dec_arg),zerofill(zero_arg),unsigned_flag(unsigned_arg)
    {
      if (zerofill)
	flags|=ZEROFILL_FLAG;
      if (unsigned_flag)
	flags|=UNSIGNED_FLAG;
    }
  Item_result result_type () const { return REAL_RESULT; }
  void prepend_zeros(String *value);
  void add_zerofill_and_unsigned(String &res) const;
  friend class create_field;
  void make_field(Send_field *);
  uint size_of() const { return sizeof(*this); }
};


class Field_str :public Field {
public:
  Field_str(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uint null_bit_arg, utype unireg_check_arg, char *field_name_arg,
	    struct st_table *table_arg)
    :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	   unireg_check_arg, field_name_arg, table_arg)
    {}
  Item_result result_type () const { return STRING_RESULT; }
  friend class create_field;
  void make_field(Send_field *);
  uint size_of() const { return sizeof(*this); }
};


class Field_decimal :public Field_num {
public:
  Field_decimal(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		uint null_bit_arg,
		enum utype unireg_check_arg, char *field_name_arg,
		struct st_table *table_arg,
		uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
    { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  void reset(void);
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void overflow(bool negative);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
};


class Field_tiny :public Field_num {
public:
  Field_tiny(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
};


class Field_short :public Field_num {
public:
  Field_short(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
};


class Field_medium :public Field_num {
public:
  Field_medium(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_INT24;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_UINT24 : HA_KEYTYPE_INT24; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
};


class Field_long :public Field_num {
public:
  Field_long(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_long(uint32 len_arg,bool maybe_null, char *field_name_arg,
	     struct st_table *table_arg,bool unsigned_arg)
    :Field_num((char*) 0, len_arg, maybe_null ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_LONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONG_INT : HA_KEYTYPE_LONG_INT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
};


#ifdef HAVE_LONG_LONG
class Field_longlong :public Field_num {
public:
  Field_longlong(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,bool maybe_null, char *field_name_arg,
		 struct st_table *table_arg)
    :Field_num((char*) 0, len_arg, maybe_null ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,0)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
};
#endif

class Field_float :public Field_num {
public:
  Field_float(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, char *field_name_arg,
	      struct st_table *table_arg,
	       uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(float); }
  void sql_type(String &str) const;
};


class Field_double :public Field_num {
public:
  Field_double(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	       uint null_bit_arg,
	       enum utype unireg_check_arg, char *field_name_arg,
	       struct st_table *table_arg,
	       uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  Field_double(uint32 len_arg, bool maybe_null, char *field_name_arg,
	       struct st_table *table_arg, uint dec_arg)
    :Field_num((char*) 0, len_arg, maybe_null ? (uchar*) "": 0, (uint) 0,
	       NONE, field_name_arg, table_arg,dec_arg,0,0)
    {}
  enum_field_types type() const { return FIELD_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(double); }
  void sql_type(String &str) const;
};


/* Everything saved in this will disapper. It will always return NULL */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(char *ptr_arg, uint32 len_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_NULL;}
  void store(const char *to,uint length) { null[0]=1; }
  void store(double nr __attribute__((unused)))   { null[0]=1; }
  void store(longlong nr __attribute__((unused))) { null[0]=1; }
  double val_real(void)		{ return 0.0;}
  longlong val_int(void)	{ return 0;}
  String *val_str(String *value,String *value2)	{ value2->length(0); return value2;}
  int cmp(const char *a,const char* b) { return 0;}
  void sort_string(char *buff,uint length)  {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const { str.set("null",4); }
  uint size_of() const { return sizeof(*this); }
};


class Field_timestamp :public Field_num {
public:
  Field_timestamp(char *ptr_arg, uint32 len_arg,
		  enum utype unireg_check_arg, char *field_name_arg,
		  struct st_table *table_arg);
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 0; }
  void set_time();
  inline long get_time()
    {
      long tmp;
      longget(tmp,ptr);
      return tmp;
    }
  void fill_and_store(char *from,uint len);
};


class Field_year :public Field_tiny {
public:
  Field_year(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		unireg_check_arg, field_name_arg, table_arg, 1, 1)
    {}
  enum_field_types type() const { return FIELD_TYPE_YEAR;}
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  void sql_type(String &str) const;
};


class Field_date :public Field_str {
public:
  Field_date(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};

class Field_newdate :public Field_str {
public:
  Field_newdate(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
		enum utype unireg_check_arg, char *field_name_arg,
		struct st_table *table_arg)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum_field_types real_type() const { return FIELD_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};


class Field_time :public Field_str {
public:
  Field_time(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, 8, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};


class Field_datetime :public Field_str {
public:
  Field_datetime(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
		 enum utype unireg_check_arg, char *field_name_arg,
		 struct st_table *table_arg)
    :Field_str(ptr_arg, 19, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATETIME;}
#ifdef HAVE_LONG_LONG
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
#endif
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};


class Field_string :public Field_str {
  bool binary_flag;
public:
  Field_string(char *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
	       uint null_bit_arg,
	       enum utype unireg_check_arg, char *field_name_arg,
	       struct st_table *table_arg,bool binary_arg)
    :binary_flag(binary_arg),
     Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }
  Field_string(uint32 len_arg,bool maybe_null, char *field_name_arg,
	       struct st_table *table_arg, bool binary_arg)
    :binary_flag(binary_arg),
     Field_str(NULL,len_arg, maybe_null ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }

  enum_field_types type() const
  {
    return ((table && table->db_create_options & HA_OPTION_PACK_RECORD &&
	     field_length >= 4) ?
	    FIELD_TYPE_VAR_STRING : FIELD_TYPE_STRING);
  }
  enum ha_base_keytype key_type() const
    { return binary_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  bool binary() const { return binary_flag; }
  void reset(void) { bfill(ptr,field_length,' '); }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_STRING; }
};


class Field_blob :public Field_str {
  uint packlength;
  String value;					// For temporaries
  bool binary_flag;
public:
  Field_blob(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, char *field_name_arg,
	     struct st_table *table_arg,uint blob_pack_length,
	     bool binary_arg);
  enum_field_types type() const { return FIELD_TYPE_BLOB;}
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return (uint32) (packlength+sizeof(char*)); }
  void store_length(ulong number);
  ulong get_length(void);
  bool binary() const { return binary_flag; }
  inline void get_ptr(char **str)
    {
      memcpy(str,ptr+packlength,sizeof(char*));
    }
  inline void set_ptr(char *length,char *data)
    {
      memcpy(ptr,length,packlength);
      memcpy(ptr+packlength,&data,sizeof(char*));
    }
  void sql_type(String &str) const;
  void free() { value.free(); }
  friend void field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
};


class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		 uint null_bit_arg,
		 enum utype unireg_check_arg, char *field_name_arg,
		 struct st_table *table_arg,uint packlength_arg,
		 TYPELIB *typelib_arg)
    :typelib(typelib_arg),packlength(packlength_arg),
     Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {
      flags|=ENUM_FLAG;
    }
  enum_field_types type() const { return FIELD_TYPE_STRING; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return (uint32) packlength; }
  void store_type(ulonglong value);
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_ENUM; }
  bool optimize_range() { return 0; }
  bool binary() const { return 0; }
};


class Field_set :public Field_enum {
public:
  Field_set(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	    uint null_bit_arg,
	    enum utype unireg_check_arg, char *field_name_arg,
	    struct st_table *table_arg,uint32 packlength_arg,
	    TYPELIB *typelib_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		    unireg_check_arg, field_name_arg,
		    table_arg, packlength_arg,
		    typelib_arg)
    {}
  void store(const char *to,uint length);
  void store(double nr) { Field_set::store((longlong) nr); }
  void store(longlong nr);
  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return FIELD_TYPE_SET; }
};


/*
** Create field class for CREATE TABLE
*/

class create_field :public Sql_alloc {
public:
  char	*field_name;
  char	*change;				// If done with alter table
  char  *after;					// Put column after this one
  Item	*def;					// Default value
  enum	enum_field_types sql_type;
  uint32 length;
  uint decimals,flags,pack_length;
  Field::utype unireg_check;
  TYPELIB *interval;				// Which interval to use
  Field *field;					// For alter table

  uint8 row,col,sc_length,interval_id;		// For rea_create_table
  uint	offset,pack_flag;
  create_field() :after(0) {}
  create_field(Field *field);
};


/*
** A class for sending info to the client
*/

class Send_field {
 public:
  char *table_name,*col_name;
  uint length,flags,decimals;
  enum_field_types type;
  Send_field() {}
};


/*
** A class for quick copying data to fields
*/

class Copy_field :public Sql_alloc {
  void (*get_copy_func(Field *to,Field *from))(Copy_field *);
public:
  char *from_ptr,*to_ptr;
  uchar *from_null_ptr,*to_null_ptr;
  my_bool *null_row;
  uint	from_bit,to_bit;
  uint from_length,to_length;
  Field *from_field,*to_field;
  String tmp;					// For items

  Copy_field() {}
  ~Copy_field() {}
  void set(Field *to,Field *from,bool save);	// Field to field
  void set(char *to,Field *from);		// Field to string
  void (*do_copy)(Copy_field *);
  void (*do_copy2)(Copy_field *);		// Used to handle null values
};


Field *make_field(char *ptr, uint32 field_length,
		  uchar *null_pos, uint null_bit,
		  uint pack_flag, Field::utype unireg_check,
		  TYPELIB *interval, char *field_name,
		  struct st_table *table);
uint pack_length_to_packflag(uint type);
uint32 calc_pack_length(enum_field_types type,uint32 length);
bool set_field_to_null(Field *field);
uint find_enum(TYPELIB *typelib,const char *x, uint length);
ulonglong find_set(TYPELIB *typelib,const char *x, uint length);

/*
** The following are for the interface with the .frm file
*/

#define FIELDFLAG_DECIMAL		1
#define FIELDFLAG_BINARY		1	// Shares same flag
#define FIELDFLAG_NUMBER		2
#define FIELDFLAG_ZEROFILL		4
#define FIELDFLAG_PACK			120	// Bits used for packing
#define FIELDFLAG_INTERVAL		256
#define FIELDFLAG_BITFIELD		512	// mangled with dec!
#define FIELDFLAG_BLOB			1024	// mangled with dec!
#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_SUM			((uint) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint) 32768)// sql
#define FIELDFLAG_PACK_SHIFT		3
#define FIELDFLAG_DEC_SHIFT		8
#define FIELDFLAG_MAX_DEC		31
#define FIELDFLAG_NUM_SCREEN_TYPE	0x7F01
#define FIELDFLAG_ALFA_SCREEN_TYPE	0x7800

#define FIELD_SORT_REVERSE		16384

#define MTYP_TYPENR(type) (type & 127)	/* Remove bits from type */

#define f_is_dec(x)		((x) & FIELDFLAG_DECIMAL)
#define f_is_num(x)		((x) & FIELDFLAG_NUMBER)
#define f_is_zerofill(x)	((x) & FIELDFLAG_ZEROFILL)
#define f_is_packed(x)		((x) & FIELDFLAG_PACK)
#define f_packtype(x)		(((x) >> FIELDFLAG_PACK_SHIFT) & 15)
#define f_decimals(x)		(((x) >> FIELDFLAG_DEC_SHIFT) & FIELDFLAG_MAX_DEC)
#define f_is_alpha(x)		(!f_is_num(x))
#define f_is_binary(x)		((x) & FIELDFLAG_BINARY)
#define f_is_enum(x)	((x) & FIELDFLAG_INTERVAL)
#define f_is_bitfield(x)	((x) & FIELDFLAG_BITFIELD)
#define f_is_blob(x)		(((x) & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB)
#define f_is_equ(x)		((x) & (1+2+FIELDFLAG_PACK+31*256))
#define f_settype(x)		(((int) x) << FIELDFLAG_PACK_SHIFT)
#define f_maybe_null(x)		(x & FIELDFLAG_MAYBE_NULL)
