<!-- Public document type declaration subset. Typical invocation:
<!ENTITY % atimath PUBLIC "-//ArborText//ELEMENTS Math Equation Structures//EN">
%atimath;
-->

<!-- Declarations for ArborText Equations (based on AAP math)
$Header: /proj/irix6.5.7m/isms/eoe/books/CrayMsg_PG/entities/RCS/ati-math.elm,v 1.4 1998/04/28 21:34:02 aclardy Exp $

NOTE: Dtgen excludes ati-math tags from the <docname>.menu and
<docname>.tags files it builds since the user cannot manipulate
these tags directly.  The tag exclusion algorithm requires that
the first and last math elements (in the order they are defined
in this file) be named <fd> and <rm> respectively.

If these assumptions are invalidated, then some math elements may
be included into the menus, or some of the DTD's elements might be 
excluded from the menus.
-->

<!ENTITY % p.em.ph	"b|it|rm">
<!ENTITY % p.fnt.ph	"blkbd|ig|sc|ge|ty|mit">
<!ENTITY % sp.pos	"vmk|vmkr|vsp|hsp|tu">
<!ENTITY % f-cs		"a|%p.em.ph|%p.fnt.ph|g|bg|%sp.pos">
<!ENTITY % f-cstxt	"#PCDATA|%f-cs">
<!ENTITY % f-scs	"rf|inc|v|dy|fi">
<!ENTITY % limits	"pr|in|sum">
<!ENTITY % f-bu		"fr|rad|lim|ar|stk|cases|eqaln|fen">
<!ENTITY % f-ph		"unl|ovl|unb|ovb|sup|inf">
<!ENTITY % f-butxt	"%f-bu|%limits|%f-cstxt|%f-scs|%f-ph|phr">
<!ENTITY % f-phtxt	"#PCDATA|%p.em.ph">
<!ENTITY % f-post       "par|sqb|llsqb|rrsqb|cub|ceil|fl|ang
                            |sol|vb|uc|dc">
<!ENTITY % f-style      "s|d|t|da|dot|b|bl|n">

<!ELEMENT fd		- - (fl)*>
<!ELEMENT fl		O O (%f-butxt)*>

  <![IGNORE [
  <!ELEMENT fd		- - (la?,fl)+>
  <!ELEMENT la		- - (%f-cstxt;|%f-ph;)*>
  <!ATTLIST la		loc		CDATA	#IMPLIED>
  ]]>

<!ELEMENT f		- - (%f-butxt)*>

<!ELEMENT fr		- - (nu,de)>
<!ATTLIST fr		shape		CDATA	#IMPLIED
			align		CDATA	#IMPLIED
			style		CDATA	#IMPLIED>
<!ELEMENT (nu|de)	O O (%f-butxt)*>
  <![IGNORE [
  <!ELEMENT lim		- - (op,ll,ul,opd?)>
  ]]>
<!ELEMENT lim		- - (op,ll?,ul?,opd?)>
<!ATTLIST lim		align		(r|c)	#IMPLIED>
  <![IGNORE [
  <!ELEMENT op		- - (%f-cstxt|rf|%f-ph) -(tu)>
  ]]>
<!ELEMENT op		- - (%f-cstxt|rf|%f-ph)* -(tu)>
<!ELEMENT (ll|ul)	O O (%f-butxt)*>
<!ELEMENT opd		- O (%f-butxt)*>
  <![IGNORE [
  <!ELEMENT (%limits)	- - (ll,ul,opd?)>
  ]]>
<!ELEMENT (%limits)	- - (ll?,ul?,opd?)>
<!ATTLIST (%limits)	align		CDATA	#IMPLIED>
<!ELEMENT rad		- - (rcd,rdx?)>
<!ELEMENT rcd		O O (%f-butxt)*>
<!ELEMENT rdx		- O (%f-butxt)* -(tu)>
  <![IGNORE [
  <!ELEMENT fen		- - ((%f-butxt)*,(cp,(%f-butxt)*)*,rp)>
  ]]>
<!ELEMENT fen		- - (%f-butxt|cp|rp)*>
<!ATTLIST fen		lp		(%f-post;)	vb
			style		(%f-style;)     s>
<!ELEMENT (cp|rp)	- O EMPTY>
<!ATTLIST (cp|rp)	post		(%f-post;)      vb
			style		(%f-style;)	s>
<!ELEMENT ar		- - (arr+)>
<!ATTLIST ar		cs		CDATA	#IMPLIED
			rs		CDATA	#IMPLIED
			ca		CDATA	#IMPLIED>
<!ELEMENT arr		- O (arc+)>
<!ELEMENT arc		O O (%f-butxt)*>
<!ATTLIST arc		align		CDATA	#IMPLIED>
<!ELEMENT cases		- - (arr+)>
<!ELEMENT eqaln		- - (eqline+)>
<!ELEMENT eqline	- - (%f-butxt)*>
<!ELEMENT stk		- - (lyr+)>
<!ELEMENT lyr		O O (%f-butxt)* -(tu)>
<!ATTLIST lyr		align		CDATA	#IMPLIED>
<!ELEMENT ach		- - (%f-butxt)*>
<!ATTLIST ach		atom		CDATA	#IMPLIED>
<!ELEMENT (sup|inf)	- - (%f-butxt)* -(tu)>
<!ATTLIST (sup|inf)	loc		CDATA	#IMPLIED>
<!ELEMENT (unl|ovl)	- - (%f-butxt)*>
<!ATTLIST (unl|ovl)	style		CDATA	#IMPLIED>
<!ELEMENT (unb|ovb)	- - (%f-butxt)*>
<!ELEMENT a		- - (ac,ac) -(tu)>
<!ATTLIST a		valign		CDATA	#IMPLIED>
  <![IGNORE [
  <!ELEMENT ac		O O (%f-cstxt|%f-scs)* -(sup|inf)>
  ]]>
<!ELEMENT ac		O O (%f-cstxt|%f-scs|sup|inf)*>
<!ELEMENT (%f-scs)	- O (%f-cstxt|sup|inf)* -(tu|%limits|%f-bu|%f-ph)>
<!ELEMENT phr		- O (%f-phtxt)*>
<!ELEMENT vmk		- O EMPTY>
<!ATTLIST vmk		id		CDATA	#IMPLIED>
<!ELEMENT vmkr		- O EMPTY>
<!ATTLIST vmkr		rid		CDATA	#IMPLIED>
<!ELEMENT (hsp|vsp)	- O EMPTY>
<!ATTLIST (hsp|vsp)	sp		CDATA	#IMPLIED>
<!ELEMENT tu		- O EMPTY>
<!ELEMENT (g|bg)	- - (#PCDATA)>
<!ELEMENT (%p.fnt.ph;)	- - (%f-cstxt)*>
<!ELEMENT (%p.em.ph;)	- - (%f-cstxt)*>

<!ENTITY % ISOamsa PUBLIC
 "ISO 8879:1986//ENTITIES Added Math Symbols: Arrow Relations//EN" "iso-amsa.ent">
%ISOamsa;

<!ENTITY % ISOamsb PUBLIC
 "ISO 8879:1986//ENTITIES Added Math Symbols: Binary Operators//EN" "iso-amsb.ent">
%ISOamsb;

<!ENTITY % ISOamsn PUBLIC
 "ISO 8879:1986//ENTITIES Added Math Symbols: Negated Relations//EN" "iso-amsn.ent">
%ISOamsn;

<!ENTITY % ISOamso PUBLIC
 "ISO 8879:1986//ENTITIES Added Math Symbols: Ordinary//EN" "iso-amso.ent">
%ISOamso;

<!ENTITY % ISOamsr PUBLIC
 "ISO 8879:1986//ENTITIES Added Math Symbols: Relations//EN" "iso-amsr.ent">
%ISOamsr;

<!ENTITY % ISOcyr1 PUBLIC "ISO 8879:1986//ENTITIES Russian Cyrillic//EN" "iso-cyr1.ent">
%ISOcyr1;

<!ENTITY % ISOdia PUBLIC "ISO 8879:1986//ENTITIES Diacritical Marks//EN" "iso-dia.ent">
%ISOdia;

<!ENTITY % ISOlat1 PUBLIC "ISO 8879:1986//ENTITIES Added Latin 1//EN" "iso-lat1.ent">
%ISOlat1;

<!ENTITY % ISOlat2 PUBLIC "ISO 8879:1986//ENTITIES Added Latin 2//EN" "iso-lat2.ent">
%ISOlat2;

<!ENTITY % ISOnum PUBLIC
 "ISO 8879:1986//ENTITIES Numeric and Special Graphic//EN" "iso-num.ent">
%ISOnum;

<!ENTITY % ISOpub PUBLIC "ISO 8879:1986//ENTITIES Publishing//EN" "iso-pub.ent">
%ISOpub;

<!ENTITY % ISOtech PUBLIC "ISO 8879:1986//ENTITIES General Technical//EN" "iso-tech.ent">
%ISOtech;

<!ENTITY % ATIeqn1 PUBLIC "-//ArborText//ENTITIES Equation1//EN" "iso-eqn1.ent">
%ATIeqn1;





