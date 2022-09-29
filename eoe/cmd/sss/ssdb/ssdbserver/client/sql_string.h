/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */
/* This file is originally from the mysql distribution. Coded by monty */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class String
{
  char *Ptr;
  uint32 str_length,Alloced_length;
  bool alloced;
public:
  String()
  { Ptr=0; str_length=Alloced_length=0; alloced=0; }
  String(uint32 length)
  { alloced=0; Alloced_length=0; (void) real_alloc(length); }
  String(const char *str)
  { Ptr=(char*) str; str_length=strlen(str); Alloced_length=0; alloced=0;}
  String(const char *str,uint32 len)
  { Ptr=(char*) str; str_length=len; Alloced_length=0; alloced=0;}
  String(char *str,uint32 len)
  { Ptr=(char*) str; Alloced_length=str_length=len; alloced=0;}
  String(const String &str)
  { Ptr=str.Ptr ; str_length=str.str_length ;
    Alloced_length=str.Alloced_length; alloced=0; }

  static void *operator new(size_t size) { return (void*) sql_alloc(size); }
  static void operator delete(void *ptr,size_t size __attribute__((unused))) /*lint -e715 */
    { sql_element_free(ptr); }
  ~String() { free(); }

  inline uint32 length() const { return str_length;}
  inline uint32 alloced_length() const { return Alloced_length;}
  inline char& operator [] (uint32 i) const { return Ptr[i]; }
  inline void length(uint32 len) { str_length=len ; }
  inline bool is_empty() { return (str_length == 0); }
  inline const char *ptr() const { return Ptr; }
  inline char *c_ptr()
  {
    if (!Ptr || Ptr[str_length])		/* Should be safe */
      (void) realloc(str_length);
    return Ptr;
  }
  inline char *c_ptr_quick()
  {
    if (Ptr && str_length < Alloced_length)
      Ptr[str_length]=0;
    return Ptr;
  }

  void set(String &str,uint32 offset,uint32 length)
  {
    free();
    Ptr=(char*) str.ptr()+offset; str_length=length; alloced=0;
    if (str.Alloced_length)
      Alloced_length=str.Alloced_length-offset;
    else
      Alloced_length=0;
  }
  inline void set(char *str,uint32 length)
  {
    free();
    Ptr=(char*) str; str_length=length; Alloced_length=length ; alloced=0;
  }
  inline void set(const char *str,uint32 length)
  {
    free();
    Ptr=(char*) str; str_length=length; Alloced_length=0 ; alloced=0;
  }
  inline void set_quick(char *str,uint32 length)
  {
    if (!alloced)
    {
      Ptr=(char*) str; str_length=length; Alloced_length=length;
    }
  }
  void set(longlong num);
  /* void set(long num); */
  void set(double num,uint decimals=2);
  inline void free()
    {
      if (alloced)
      {
	alloced=0;
	my_free(Ptr,MYF(0));
	Ptr=0;
      }
    }

  inline bool alloc(uint32 length)
  {
    if (length < Alloced_length)
      return 0;
    return real_alloc(length);
  }
  bool real_alloc(uint32 length);			// Empties old string
  bool realloc(uint32 length);
  inline void shrink(uint32 length)		// Shrink buffer
  {
    if (length < Alloced_length)
    {
      char *new_ptr;
      if (!(new_ptr=my_realloc(Ptr,length,MYF(0))))
      {
	(void) my_free(Ptr,MYF(0));
	real_alloc(length);
      }
      else
      {
	Ptr=new_ptr;
	Alloced_length=length;
      }
    }
  }
  bool is_alloced() { return alloced; }
  inline String& operator = (const String &s)
  {
    if (&s != this)
    {
      free();
      Ptr=s.Ptr ; str_length=s.str_length ; Alloced_length=s.Alloced_length;
      alloced=0;
    }
    return *this;
  }

  void copy();					// Alloc string if not alloced
  void copy(const String &s);			// Allocate new string
  void copy(const char *s,uint32 length);		// Allocate new string
  void append(const String &s);
  void append(const char *s,uint32 length=0);
  int strstr(const String &search,uint32 offset=0); // Returns offset to substring or -1
  int strrstr(const String &search,uint32 offset=0); // Returns offset to substring or -1
  void replace(uint32 offset,uint32 length,const String &to);
  inline bool append(char chr)
  {
    if (str_length < Alloced_length)
    {
      Ptr[str_length++]=chr;
    }
    else
    {
      if (realloc(str_length+1))
	return 1;
      Ptr[str_length++]=chr;
    }
    return 0;
  }
  void fill(uint32 max_length,char fill);
  void strip_sp();
  inline void caseup() { ::caseup(Ptr,str_length); }
  inline void casedn() { ::casedn(Ptr,str_length); }
  friend int sortcmp(const String *a,const String *b);
  friend int stringcmp(const String *a,const String *b);
  friend String *copy_if_not_alloced(String *a,String *b,uint32 length);
  friend int wild_case_compare(String &match,String &wild,char escape);
  friend int wild_compare(String &match,String &wild,char escape);
};
