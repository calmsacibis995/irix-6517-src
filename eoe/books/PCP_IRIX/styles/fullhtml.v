<!-- 	Silicon Graphics, Inc.
	Software Publications
	CRAYDOCBK DTD stylesheet
	Contact agd@sgi.com with questions
	Production Stylesheet
	January 5, 1999
	Version 2.7e
-->

<sheet>

<?INSTED COMMENT: GROUP admonition>

<group name="admonition">
	<text-before><blockquote></>
	<text-after><join(/)blockquote><br clear="all"></>
</group>



<style name="CAUTION" group="admonition">
	<text-before><blockquote><img src="/icons/caution.gif" alt="[caution]" align=left><b>Caution: <join(/)b></>
</style>

<style name="IMPORTANT" group="admonition">
	<text-before><blockquote><b>Important: <join(/)b></>
</style>

<style name="NOTE" group="admonition">
	<select>if(attr(role),NOTE.attr(role),NOTE.DEFAULT)</>
</style>

<style name="NOTE.DEFAULT" group="admonition">
	<text-before><blockquote><b>Note: <join(/)b></>
</style>

<style name="NOTE.ANSI-ISO" group="admonition">
	<text-before><blockquote><b>ANSI/ISO: <join(/)b></>
</style>

<style name="NOTE.OPENMP" group="admonition">
	<text-before><blockquote><b>OpenMP: <join(/)b></>
</style>

<style name="TIP" group="admonition">
	<text-before><blockquote><b>Tip: <join(/)b></>
</style>



<style name="WARNING" group="admonition">
	<text-before><blockquote><img src="/icons/warning.gif" alt="[Warning]" align=left><b>Warning: <join(/)b></>
</style>

<?INSTED COMMENT: GROUP code>

<group name="code">
	<text-before><code></>
	<text-after>join('<','/code>')</>
</group>

<style name="CLASSNAME" group="code">
</style>

<style name="FUNCTION" group="code">
</style>

<style name="KEYCODE" group="code">
</style>

<style name="OPTION" group="code">
</style>

<style name="OPTIONAL" group="code">
	<text-before>[</>
	<text-after>] </>
</style>

<style name="PARAMETER" group="code">
</style>

<style name="PROPERTY" group="code">
</style>

<style name="STRUCTFIELD" group="code">
</style>

<style name="STRUCTNAME" group="code">
</style>

<style name="SYSTEMITEM" group="code">
</style>

<style name="TOKEN" group="code">
</style>

<style name="TYPE" group="code">
</style>



<?INSTED COMMENT: GROUP dd>

<group name="dd">
	<text-before><dd><p></>
	<text-after><join(/)p><join(/)dd></>
</group>

<style name="GLOSSDEF" group="dd">
</style>

<style name="REVHISTORY,REVISION,DATE" group="dd">
</style>

<style name="REVHISTORY,REVISION,REVREMARK" group="dd">
</style>



<?INSTED COMMENT: GROUP deflist>
<!-- Format the deflist as a series of table cells -->

<style name="DEFLIST">
	<text-before>if(ancestor(para),<p>,)<table border=0 cellspacing=2 width=100%></>
	<text-after><join(/)table></>
</style>

<style name="HEAD1">
	<text-before><tr><th width="switch(attr(termlength,ancestor(deflist)), 'NEXTLINE',25%, 'STANDARD',40%, 'NARROW',25%, 'WIDE',60%, 'DEFAULT',40%)" align=left><u></>
	<text-after><join(/)u><join(/)th></>
</style>

<style name="HEAD2">
	<text-before><th width="switch(attr(termlength,ancestor(deflist)), 'NEXTLINE',75%, 'STANDARD',60%, 'NARROW',75%, 'WIDE',40%, 'DEFAULT',60%)" align=left><u></>
	<text-after><join(/)u><join(/)th><join(/)tr></>
</style>

<!-- Deflist Term -->
<style name="DEFLISTENTRY,TERM">
	<select>if(eq(attr(termlength, ancestor(DEFLIST)),NEXTLINE), DEFLIST_TERM.NEXTLINE, DEFLIST_TERM.STANDARD)</>
</style>

<style name="DEFLIST_TERM.STANDARD">
	<text-before>if(isfirst(),<tr valign=top align=left><td width="switch(attr(termlength,ancestor(deflist)), 'STANDARD',40%, 'NARROW',25%, 'WIDE',60%, 'DEFAULT',40%)">,<br>)</>
	<text-after>if(islast(),<join(/)td>,)</>
</style>

<style name="DEFLIST_TERM.NEXTLINE">
	<text-before>if(isfirst(),<tr valign=top align=left><td colspan=2>,<br>)</>
	<text-after>if(islast(),<join(/)td><join(/)tr>,)</>
</style>

<!-- Deflist Entry -->
<style name="DEFLISTENTRY,LISTITEM">
	<select>if(eq(attr(termlength, ancestor(DEFLIST)),NEXTLINE), DEFLIST_ENTRY.NEXTLINE, DEFLIST_ENTRY.STANDARD)</>
</style>

<style name="DEFLIST_ENTRY.STANDARD">
	<text-before><td width="switch(attr(termlength,ancestor(deflist)), 'STANDARD',60%, 'NARROW',75%, 'WIDE',40%, 'DEFAULT',60%)"></>
	<text-after><join(/)td><join(/)tr></>
</style>

<style name="DEFLIST_ENTRY.NEXTLINE">
	<text-before><tr><td width="25%">join(&)nbsp;<join(/)td><td></>
	<text-after><join(/)td><join(/)tr></>
</style>



<?INSTED COMMENT: GROUP displayhead>

<group name="displayhead">
	<text-before><p align=center></>
	<text-after><join(/)p></>
</group>

<style name="ABSTRACT,TITLE" group="displayhead">
</style>

<style name="DEFLIST,TITLE" group="displayhead">
	<text-before><caption></>
	<text-after><join(/)caption></>
</style>

<style name="EQUATION,TITLE" group="displayhead">
</style>

<style name="EXAMPLE,TITLE" group="displayhead">
	<text-before><p align=center>Example gcnum(ancestor(example)). </>
</style>

<style name="IMPORTANT,TITLE" group="displayhead">
</style>



<?INSTED COMMENT: GROUP dl>

<group name="dl">
	<text-before>if(ancestor(para),<br><br>,)<dl></>
	<text-after><join(/)dl></>
</group>

<style name="GLOSSENTRY" group="dl">
</style>

<style name="REVHISTORY" group="dl">
	<text-before> <h2>Record of Revision <join(/)h2><dl></>
</style>



<?INSTED COMMENT: GROUP dt>

<group name="dt">
	<text-before><dt><p></>
	<text-after><join(/)p><join(/)dt></>
</group>

<style name="GLOSSENTRY,GLOSSTERM" group="dt">
	<text-before><dt><p><em></>
	<text-after><join(/)em><join(/)p><join(/)dt></>
</style>

<style name="REVHISTORY,REVISION,REVNUMBER" group="dt">
</style>



<?INSTED COMMENT: GROUP em>

<group name="em">
	<text-before><em></>
	<text-after><join(/)em></>
</group>

<style name="ABBREV" group="em">
</style>

<style name="ACRONYM" group="em">
</style>

<style name="FIRSTTERM" group="em">
</style>

<style name="GLOSSTERM" group="em">
</style>

<style name="MARKUP" group="em">
</style>

<style name="TRADEMARK" group="em">
</style>


<?INSTED COMMENT: GROUP figure>

<group name="figure">
</group>

<style name="FIGURE" group="figure">
        <text-before><blockquote></>
        <text-after><join(/)blockquote></>
</style>

<style name="FIGURE,TITLE" group="figure">
	<text-before><P><B>Figure gcnum(ancestor(figure)).<join(/)B> </>
        <text-after><join(/)P><P></>
</style>

<style name="SCREENSHOT" group="figure">
        <text-before><blockquote></>
        <text-after><join(/)blockquote></>
</style>

<style name="SCREENSHOT,TITLE" group="figure">
	<text-before><P><B></>
        <text-after><join(/)B><join(/)P><P></>
</style>

<style name="GRAPHIC" group="figure">
	<inline>	raster filename="attr(entityref)"	</>
</style>


<?INSTED COMMENT: GROUP h1>

<group name="h1">
	<text-before><h1></>
	<text-after><join(/)h1></>
</group>

<style name="APPENDIX,TITLE" group="h1">
	<text-before><h1>Appendix format(gcnum(ancestor(appendix)),LETTER). </>
</style>

<style name="BOOK,TITLE" group="h1">
</style>

<style name="CHAPTER,TITLE" group="h1">
	<text-before><h1>Chapter gcnum(ancestor(chapter)). </>
</style>

<style name="GLOSSARY,TITLE" group="h1">
</style>

<style name="NEWFEATURES,TITLE" group="h1">
</style>

<style name="PART,TITLE" group="h1">
	<text-before><h1>Part gcnum(ancestor(part)). </>
</style>

<style name="PREFACE,TITLE" group="h1">
	<text-before><h1></>
</style>



<?INSTED COMMENT: GROUP h2>

<group name="h2">
	<text-before><h2></>
	<text-after><join(/)h2></>
</group>

<style name="APPENDIX,SECTION,TITLE" group="h2">
	<text-before><h2>format(gcnum(ancestor(appendix)),LETTER).cnum(ancestor(section)) </>
</style>

<style name="CHAPTER,SECTION,TITLE" group="h2">
	<text-before><h2>gcnum(ancestor(chapter)).cnum(ancestor(section)) </>
</style>

<style name="PREFACE,SECTION,TITLE" group="h2">
</style>



<?INSTED COMMENT: GROUP h3>

<group name="h3">
	<text-before><h3></>
	<text-after><join(/)h3></>
</group>

<style name="APPENDIX,SECTION,SECTION,TITLE" group="h3">
	<text-before><h3>format(gcnum(ancestor(appendix)),LETTER).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>

<style name="CHAPTER,SECTION,SECTION,TITLE" group="h3">
	<text-before><h3>gcnum(ancestor(chapter)).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>



<?INSTED COMMENT: GROUP h4>

<group name="h4">
	<text-before><h4></>
	<text-after><join(/)h4></>
</group>

<style name="APPENDIX,SECTION,SECTION,SECTION,TITLE" group="h4">
	<text-before><h4>format(gcnum(ancestor(appendix)),LETTER).cnum(ancestor(me(),section,3)).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>

<style name="CHAPTER,SECTION,SECTION,SECTION,TITLE" group="h4">
	<text-before><h4>gcnum(ancestor(chapter)).cnum(ancestor(me(),section, 3)).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>



<?INSTED COMMENT: GROUP h5>

<group name="h5">
	<text-before><h4></>
	<text-after><join(/)h4></>
</group>

<style name="APPENDIX,SECTION,SECTION,SECTION,SECTION,TITLE" group="h5">
	<text-before><h4>format(gcnum(ancestor(appendix)),LETTER).cnum(ancestor(me(),section,4)).cnum(ancestor(me(),section,3)).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>

<style name="CHAPTER,SECTION,SECTION,SECTION,SECTION,TITLE" group="h5">
	<text-before><h4>gcnum(ancestor(chapter)).cnum(ancestor(me(),section,4)).cnum(ancestor(me(),section,3)).cnum(ancestor(me(),section,2)).cnum(ancestor(section)) </>
</style>



<?INSTED COMMENT: GROUP li>

<!-- 	We want lists to have "open" spacing: every list item, every 
	paragraph within a list item and every list should be separated
	by exactly one blank line. Each element in the list will need
        to set blank spacing around it.
  -->


<group name="li">
	<text-before><li></>
	<text-after><join(/)li></>
</group>

<style name="ITEM" group="li">
</style>

<style name="LISTITEM" group="li">
</style>


<?INSTED COMMENT: GROUP mono>

<!-- This group includes all elements that change the font to a monospace 
     font. The HTML tag depends on function and desired appearance. The 
     default is <code>. The group monodisplay includes elements that set
     off monospace text from the body.
  -->
<group name="mono">
	<text-before><code></>
	<text-after><join(/)code></>
</group>

<!-- Note: Default ?? value is for unrecognized sectionref attribute values. -->
<style name="COMMAND" group="mono">
	<text-after><join(/)code>if(attr(sectionref),if(eq(attr(sectionref),blank),,\(gamut(attr(sectionref), '1 1B 1B-U 1M 1M-U 1X 1X-U 2 3 3C 3C-U 3F 3F-U 3G 3G-U 3I 3I-U 3K 3K-U 3L 3L-U 3M 3M-U 3N 3N-U 3O 3O-U 3R 3R-U 3S 3S-U 3X 3X-U 4 4P 4P-U 5 5X 5X-U 6 7 7D 7D-U 7X 7X-U 8 8E 8E-U','1 1B 1B 1M 1M 1X 1X 2 3 3C 3C 3F 3F 3G 3G 3I 3I 3K 3K 3L 3L 3M 3M 3N 3N 3O 3O 3R 3R 3S 3S 3X 3X 4 4P 4P 5 5X 5X 6 7 7D 7D 7X 7X 8 8E 8E', join('??'))\)),)</>
</style>

<style name="FILENAME" group="mono">
</style>

<style name="HARDWARE" group="mono">
	<text-before><kbd></>
	<text-after><join(/)kbd></>
</style>

<style name="INTERFACEDEFINITION" group="mono">
	<text-before><BR> <BR><DL><DT></>
	<text-after><join(/)DT><join(/)DL></>
</style>

<style name="INTERFACEDEFINITION,INTERFACE" group="mono">
        <text-before><DL><DT><code></>
	<text-after><join(/)DT><join(/)DL><join(/)code></>
</style>

<style name="INTERFACE,INTERFACE" group="mono">
        <text-before>if(contains(qtag(me()),INTERFACEDEFINITION),<DL><DT><code>\ -> ,\ -> )</>
        <text-after>if(contains(qtag(me()),INTERFACEDEFINITION),<join(/)code><join(/)DT><join(/)DL>,<join(/)code>)</>
</style>

<style name="INTERFACE" group="mono">
	<text-before><code></>
	<text-after><join(/)code></>
</style>

<style name="KEYCAP" group="mono">
</style>

<style name="LITERAL" group="mono">
</style>

<style name="USERINPUT" group="mono">
	<text-before><kbd><b></>
	<text-after><join(/)b><join(/)kbd></>
</style>



<?INSTED COMMENT: GROUP monodisplay>

<!-- See the comment for the group mono. -->
<group name="monodisplay">
	<text-before><pre></>
	<text-after><join(/)pre></>
</group>

<style name="LITERALLAYOUT" group="monodisplay">
</style>

<style name="PROGRAMLISTING" group="monodisplay">
</style>

<style name="SCREEN" group="monodisplay">
	<text-before>if(ancestor(para),<p>,)<table border cellpadding="10" cellspacing="0" width="720"><tr><td><pre></>
	<text-after><join(/)pre><join(/)td><join(/)tr><join(/)table><p></>
</style>

<!--
handle synopsis elements based upon:
 1. If the element has a synopsis ancestor, it is just formatted based on the format attribute
 2. If the element has synopsis children then it is a parent.  Draw the bounding box and let
	the child elements handle formatting
 3. it is a standalone synopsis - draw the bounding box and format properly

if the format attribute = yes then use <pre> to maintain the literal layout, otherwise use <code>

consecutive parent or standalone synopsis elements are separated with additional white space
-->

<style name="SYNOPSIS" group="monodisplay">
	<select>if(ancestor(synopsis), SYNOPSIS.NESTED, if(typechild(synopsis), SYNOPSIS.PARENT, SYNOPSIS.STANDALONE))</>
</style>

<style name="SYNOPSIS.STANDALONE" group="monodisplay">
	<text-before><p>if(eq(tag(lsibling()),synopsis),<br>,)<table border cellpadding="10" cellspacing="0"><tr><td>if(eq(attr(format), yes),<pre>,<code>)</>
	<text-after>if(eq(attr(format), yes),<join(/)pre>,<join(/)code>)<join(/)td><join(/)tr><join(/)table><join(/)p></>
</style>

<style name="SYNOPSIS.NESTED" group="monodisplay">
	<text-before><p>if(eq(attr(format), yes),<pre>,<code>)</>
	<text-after>if(eq(attr(format), yes),<join(/)pre>,<join(/)code>)<join(/)p></>
</style>

<style name="SYNOPSIS.PARENT" group="monodisplay">
	<text-before><p>if(eq(tag(lsibling()),synopsis),<br>,)<table border cellpadding="10" cellspacing="0"><tr><td></>
	<text-after><join(/)td><join(/)tr><join(/)table><br>if(ancestor(para),<join(/)p>,)</>
</style>

<?INSTED COMMENT: GROUP ol>

<!--	The first ol in a para needs spacing before it.
	See the li element for more info.
  -->
<group name="ol">
	<left-indent>	+=10	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before>if(ancestor(para),<p>,)<ol></>
	<text-after><join(/)ol></>
</group>

<style name="ORDEREDLIST" group="ol">
	<text-before>if(ancestor(para),<p>,)if(ancestor(orderedlist),if(ancestor(orderedlist,ancestor(orderedlist)),<ol type=i>,<ol type=a>),<ol type=1>)</>
</style>



<?INSTED COMMENT: GROUP p>

<group name="p">
	<text-before><p></>
	<text-after><join(/)p></>
</group>

<style name="LEGALNOTICE,PARA" group="p">
</style>

<style name="PARA" group="p">
</style>



<?INSTED COMMENT: GROUP procedure>

<group name="procedure">
</group>

<style name="PROCEDURE" group="procedure">
	<text-before><h3>Procedure gcnum(me()): if(not(typechild(title)),<join(/)h3>,)</>
	<text-after>if(typechild('STEP'),<join(/)ol>,)</>
</style>

<!-- Procedures took some finagling; notice where h3 and ol open.
  -->
<style name="PROCEDURE,TITLE" group="procedure">
	<text-before><!-- h3 already opened by procedure element --></>
	<text-after><join(/)h3></>
</style>

<style name="PROCEDURE,PARA" group="procedure">
	<text-before><p></>
	<text-after><join(/)p></>
</style>

<style name="STEP" group="procedure">
	<text-before>if(and(isfirst(), not(ancestor('SUBSTEPS'))),<ol>,)<li></>
	<text-after><join(/)li></>
</style>

<style name="SUBSTEPS" group="procedure">
	<text-before>if(ancestor(substeps),<ol type=1>,<ol type=a>)</>
	<text-after><join(/)ol></>
</style>




<?INSTED COMMENT: GROUP samp>

<group name="samp">
	<text-before><samp></>
	<text-after><join(/)samp></>
</group>

<style name="ACTION" group="samp">
</style>

<style name="APPLICATION" group="samp">
</style>

<style name="COMPUTEROUTPUT" group="samp">
</style>

<style name="ERRORNAME" group="samp">
</style>

<style name="ERRORTYPE" group="samp">
</style>

<style name="RETURNVALUE" group="samp">
</style>



<?INSTED COMMENT: GROUP seriespara>

<!-- The group seriespara gives space before a paragraph only if it's the
     the first in its series.
  -->
<group name="seriespara">
	<text-before>if(isfirst(),,<p>)</>
	<text-after>if(isfirst(),,<join(/)p>)</>
</group>

<style name="CAUTION,PARA" group="seriespara">
	<text-after>if(isfirst(),if(le(length(content()),85),<br><br><br>,),<join(/)p>)</>
</style>

<style name="FOOTNOTE,PARA" group="seriespara">
	<text-before>if(isfirst(),Footnote gcnum(ancestor(footnote))<p>,<p>)</>
</style>

<style name="IMPORTANT,PARA" group="seriespara">
</style>

<style name="NOTE,PARA" group="seriespara">
</style>

<style name="TIP,PARA" group="seriespara">
</style>

<style name="WARNING,PARA" group="seriespara">
</style>



<?INSTED COMMENT: GROUP suppressed>

<group name="suppressed">
	<hide>	Children	</>
</group>

<style name="CAUTION,TITLE" group="suppressed">
</style>

<style name="COLLECTIONS" group="suppressed">
</style>

<style name="EQUATION" group="suppressed">
	<inline>raster filename="join('eqn', gcnum(), '.gif')"</>
	<select>if(eq(file(join(dir(fig), '/', 'eqn', gcnum(), '.gif')),NONE),EQUATION.NOGIF,EQUATION)</>
</style>

<style name="EQUATION.NOGIF" group="suppressed">
	<text-before><p>[Equation gcnum()] </>
</style>

<style name="INLINEEQUATION" group="suppressed">
	<inline>raster filename="join('inlineeqn', gcnum(), '.gif')"</>
	<select>if(eq(file(join(dir(fig), '/', 'inlineeqn', gcnum(), '.gif')),NONE),INLINEEQUATION.NOGIF,INLINEEQUATION)</></style>

<style name="INLINEEQUATION.NOGIF" group="suppressed">
	<text-before>[Inline equation gcnum()] </>
</style>

<style name="NOTE,TITLE" group="suppressed">
</style>

<style name="TIP,TITLE" group="suppressed">
</style>

<style name="SCREENINFO" group="suppressed">
</style>

<style name="WARNING,TITLE" group="suppressed">
</style>



<?INSTED COMMENT: GROUP ul>

<!--	The first list within a paragraph needs space added before it. -->
<group name="ul">
	<text-before>if(ancestor(para),<p>,)<ul type=switch(mod(countword(ancestors(me()),x,'eq(tag(var(x)),itemizedlist)'),3), 0, DISC, 1, CIRCLE, 2, SQUARE, 'DEFAULT', DISC)></>
	<text-after><join(/)ul></>
</group>

<style name="ITEMIZEDLIST" group="ul">
</style>



<?INSTED COMMENT: GROUP msgset>

<!-- This group handles all of the tags that can occur within a msgset tag -->
<group name="msgset">
</group>

<style name="MSGSET" group="msgset">
	<text-before><p></>
	<text-after><join(/)p></>
</style>

<style name="MSGENTRY" group="msgset">
</style>

<style name="MSGINFO" group="msgset">
</style>

<style name="MSGNUMBER" group="msgset">
	<text-before><hr size=2 noshade><b>MSG NO: </>
	<text-after><join(/)b><hr size=2 noshade></>
</style>

<style name="MSGNO" group="msgset">
</style>

<!-- Add in forced whitespace before any msgsubno to replicate printed version -->
<style name="MSGSUBNO" group="msgset">
	<text-before>join(&,nbsp;)join(&,nbsp;)join(&,nbsp;)join(&,nbsp;)</>
</style>

<style name="MSGLEVEL" group="msgset">
	<text-before>Type: </>
	<text-after><br></>
</style>

<style name="MSGORIG" group="msgset">
	<text-before>Module: <code></>
	<text-after><join(/)code><br></>
</style>

<style name="MSGAUD" group="msgset">
	<text-before><b>Audience:<join(/)b> </>
	<text-after><br></>
</style>

<style name="MSGMAIN" group="msgset">
	<text-before><code></>
	<text-after><join(/)code></>
</style>

<style name="MSGTEXT" group="msgset">
</style>

<style name="MSGREL" group="msgset">
</style>

<style name="MSGSUB" group="msgset">
</style>

<style name="MSGEXPLAN" group="msgset">
</style>



<?INSTED COMMENT: GROUP wrapper>

<!-- 	The wrapper group includes elements whose primary purpose is to 
	contain other elements, usually with little formatting of their own.
  -->
<group name="wrapper">
</group>

<style name="APPENDIX" group="wrapper">
</style>

<style name="CHAPTER" group="wrapper">
</style>

<style name="DEFLISTENTRY" group="wrapper">
</style>

<style name="EXAMPLE" group="wrapper">
</style>

<style name="PART" group="wrapper">
</style>

<style name="SECTION" group="wrapper">
</style>

<style name="TGROUP" group="wrapper">
</style>

<?INSTED COMMENT: GROUP xrefs>

<group name="xrefs">
	<script>	ebt-link target=idmatch(id, attr(linkend))	</>
</group>

<style name="XREF" group="xrefs">
	<!-- XREF. This style selects an XREF.ELEMENT style based on 
several criteria.

if(xref has xreflabel attribute) 
  then hot-text is xreflabel;
else if(linkend of xref is a title) 
  then select style for linkend's ancestor;
else select style for linkend;

Each style, XREF.ELEMENT, generates customized hot-text, 
tweaking the target of cnum() based on whether linkend is a title
or not.

Element         Hot-text
=======         ========
preface         Preface
appendix        Appendix Z
chapter         Chapter 9
equation        Equation 9
example         Example 9
figure          Figure 9
inlineequation  Inline Equation 9
para            [Click here]
procedure       Procedure 9
section         Section 9.8.7 (see XREF.SECTION for details)
step            Procedure 9, Step 8
table           Table 9
screeshot	Screen Shot 8
glossary	Glossary
default         [Click here]

Programming note: The select statement breaks unless the DEFAULT item 
of gamut() is protected by join().
-->
	<select>XREF.if(attr(xreflabel, me()),XREFLABEL,gamut(if(eq(tag(idmatch(attr(linkend))),title), tag(ancestor(idmatch(attr(linkend)))), tag(idmatch(attr(linkend)))), 'appendix chapter equation example figure glossary inlineequation para preface procedure screenshot section step table','APPENDIX CHAPTER EQUATION EXAMPLE FIGURE GLOSSARY INLINEEQUATION PARA PREFACE PROCEDURE SCREENSHOT SECTION STEP TABLE', join('DEFAULT')))	</>
</style>

<style name="XREF.APPENDIX" group="xrefs">
	<text-before>Appendix format(gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend)))), LETTER)</>
</style>

<style name="XREF.CHAPTER" group="xrefs">
	<text-before>Chapter gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.DEFAULT" group="xrefs">
	<text-before>[Click here]</>
</style>

<style name="XREF.EQUATION" group="xrefs">
	<text-before>Equation gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.EXAMPLE" group="xrefs">
	<text-before>Example gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.FIGURE" group="xrefs">
	<text-before>Figure gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.GLOSSARY" group="xrefs">
	<text-before>Glossary</>
</style>

<style name="XREF.INLINEEQUATION" group="xrefs">
	<text-before>Inline Equation gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.PARA" group="xrefs">
	<text-before>[Click here]</>
</style>

<style name="XREF.PREFACE" group="xrefs">
	<text-before>Preface</>
</style>

<style name="XREF.PROCEDURE" group="xrefs">
	<text-before>Procedure gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.SCREENSHOT" group="xrefs">
	<text-before>Screen Shot gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.SECTION" group="xrefs">
	<!-- XREF.SECTION. Here's a little monster. The enumeration must handle the 
existence of ancestor sections and chapter/appendix/preface. The xref may point to 
either the section or its title.

If there exists       Then add
===============       ========
ancestor preface      Preface
[always]              Section
ancestor chapter      Chapter gcnum().
ancestor appendix     Appendix gcnum().
4-ancestor section    cnum().
3-ancestor section    cnum().
2-ancestor section    cnum().
1-ancestor section    cnum()
if(tag(me) != title)
  1-ancestor section  .
  [always]            cnum(me)
-->
	<text-before>if(ancestor(idmatch(attr(linkend)),preface),Preface Section ,Section )if(ancestor(idmatch(attr(linkend)),chapter),gcnum(ancestor(idmatch(attr(linkend)),chapter)).,)if(ancestor(idmatch(attr(linkend)),appendix),format(gcnum(ancestor(idmatch(attr(linkend)),appendix)),LETTER).,)if(ancestor(idmatch(attr(linkend)),section,4),cnum(ancestor(idmatch(attr(linkend)),section,4)).,)if(ancestor(idmatch(attr(linkend)),section,3),cnum(ancestor(idmatch(attr(linkend)),section,3)).,)if(ancestor(idmatch(attr(linkend)),section,2),cnum(ancestor(idmatch(attr(linkend)),section,2)).,)if(ancestor(idmatch(attr(linkend)),section,1),cnum(ancestor(idmatch(attr(linkend)),section,1)),)if(eq(tag(idmatch(attr(linkend))),title),,if(ancestor(idmatch(attr(linkend)),section,1),.,)cnum(idmatch(attr(linkend))))</>
</style>

<style name="XREF.STEP" group="xrefs">
	<text-before>Procedure gcnum(ancestor(idmatch(attr(linkend)),procedure)), Step cnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.TABLE" group="xrefs">
	<text-before>Table gcnum(if(eq(tag(idmatch(attr(linkend))),title), ancestor(idmatch(attr(linkend))), idmatch(attr(linkend))))</>
</style>

<style name="XREF.XREFLABEL" group="xrefs">
	<text-before>attr(xreflabel,me())</>
</style>



<?INSTED COMMENT: UNGROUPED STYLES FOLLOW>

<style name="#ANNOT">
	<break-before>	Line	</>
</style>

<style name="#DEFAULT">
</style>

<style name="#FOOTER">
	<text-before><A HREF="http:/docs/help/help_all.html"><img src="/images/help.gif" alt="Help " align=left><join(/)A><A HREF="http:/"><img src="/images/home.gif" alt="Home" align=left><join(/)A><br><br><br><br></>
	<text-after><P><SMALL>Copyright (c) 1998 <A HREF="http://www.cray.com">Cray Research, Inc.<join(/)A><join(/)SMALL></>
</style>


<style name="#HEADER">
	<text-before><join(/)head><body bgcolor="#FFFFFF" text="#000000" ALINK="#ff0000" VLINK="#551a8b" LINK="#0000ee"><img height="72" width="192" src="/images/craylogo.gif" alt="Cray Logo" align="right"> <br> <br> <br></>
</style>



<style name="#QUERY">
	<break-before>	Line	</>
</style>

<style name="#ROOT">
	<break-before>	Line	</>
</style>

<style name="#SDATA">
	<text-before>gamut('trade  mdash  bull  divide','(TM)  --  *',join('&',attr(name).';'))</>
</style>

<style name="#TAGS">
</style>

<style name="ACKNOWLEDGEMENTS">
	<text-before><h2>Notices<join(/)h2></>
</style>

<style name="ALT-TITLE">
</style>

<style name="BLOCKQUOTE">
	<text-before><blockquote></>
	<text-after><join(/)blockquote></>
</style>

<style name="CITETITLE">
	<text-before><cite></>
	<text-after><join(/)cite></>
</style>

<style name="CMDSYNOPSIS">
</style>

<style name="COLSPEC"></style>

<style name="SPANSPEC"></style>

<style name="COMMENT">
	<hide>  Children  </>
</style>

<style name="COPYRIGHT">
</style>

<style name="DOCBOOK">
	<font-family>	new century schoolbook	</>
	<font-size>	14	</>
</style>

<style name="EMPHASIS">
	<text-before><b></>
	<text-after><join(/)b></>
</style>

<style name="FOOTNOTE">
	<hide>	Children	</>
	<script>	ebt-reveal stylesheet="fullhtml.v" title="Footnote"	</>
	<text-before>[Footnote gcnum()]</>
</style>

<style name="INDEX">
	<hide>	All	</>
</style>

<style name="INDEXTERM">
	<hide>	Children	</>
</style>

<style name="LINEANNOTATION">
	<text-before><join(/)pre></>
	<text-after><pre></>
</style>

<style name="LINK">
	<text-before><a href="attr(href)"></>
	<text-after>join('<','/a>')</>
</style>

<style name="NEWLINE">
	<text-before>if(ancestor(para),if(content(ancestor(para)),join(' ','<br>'),),join(' ','<br>'))</>
	<text-after>if(ancestor(synopsis),join(' ',' '),)</>
</style>

<style name="PARTNUMBER">
	<text-before><p>Document Number </>
	<text-after><join(/)p></>
</style>

<style name="PARTNUMBER,CLASSCODE">
	<text-after>-</>
</style>

<style name="PARTNUMBER,BASE">
	<text-after>-</>
</style>

<style name="PUBNUMBER">
	<text-before><p></>
	<text-after><join(/)p></>
</style>

<style name="PUBNUMBER,PUBTYPE">
	<text-after>-</>
</style>

<style name="PUBNUMBER,STOCKNUM">
	<text-after> </>
</style>

<style name="PUBTYPE">
	<text-after>-</>
</style>

<style name="REPLACEABLE">
	<text-before><var></>
	<text-after><join(/)var></>
</style>

<style name="REVEND"></style>


<style name="REVNUMBER">
	<space-before>	12	</>
	<break-before>	Line	</>
</style>

<style name="REVST"></style>


<style name="ROW">
	<text-before><tr valign="top"></>
	<text-after><join(/)tr></>
</style>

<!-- 	The simplelist element contains member elements. There are three kinds
	of simplelist, set by the type attribute:
	(i) inline. The members are separated by commas within the line.
	(ii) address. A block address, currently formatted like the default.
	(iii) default. A vertical list, one column only; an indent is given by
	blockquote, and the elements are separated by br. It's assumed the
	members are not longer than a line.
-->
<style name="SIMPLELIST">
	<select>	if(eq(attr(type),inline),SIMPLELIST.INLINE,SIMPLELIST.DEFAULT)	</>
</style>

<style name="SIMPLELIST,MEMBER">
	<select>	if(eq(attr(type,ancestor(simplelist)),inline),SIMPLELIST.MEMBER.INLINE,SIMPLELIST.MEMBER.DEFAULT)	</>
</style>

<style name="SIMPLELIST.DEFAULT">
	<text-before><blockquote></>
	<text-after><join(/)blockquote></>
</style>

<!-- SIMPLELIST.INLINE is just a wrapper. -->
<style name="SIMPLELIST.INLINE">
</style>

<style name="SIMPLELIST.MEMBER.DEFAULT">
	<text-after>if(islast(),,<br>)</>
</style>

<style name="SIMPLELIST.MEMBER.INLINE">
	<text-before>if(isfirst(),,if(islast(),join(' ')and ,join(',',' ')))</>
</style>

<style name="SUBSCRIPT">
	<text-before><sub></>
	<text-after><join(/)sub></>
</style>

<style name="SUPERSCRIPT">
	<text-before><sup></>
	<text-after><join(/)sup></>
</style>

<style name="TABLE,TITLE">
	<text-before><p align=center>Table gcnum(ancestor(table)). </>
	<text-after><join(/)p><table cellpadding=10 border=if(eq(attr(frame,ancestor(table)),'all'),2,0)>if (eq (attr(frame,ancestor(table)), ALL),,<hr type=1>)</>
</style>

<style name="TABLE">
	<text-before>if(eq(typechild('title',me()),0), <p align=center>Table gcnum(me()).<join(/)p><table cellpadding=10 border=if(eq(attr(frame),'all'),2,0)>if(eq(attr(frame), ALL),,<hr type=1>),)</>
	<text-after><join(/)table>if(eq(attr(frame), ALL),,<hr type=2>)</>
</style>

<style name="ROW,ENTRY,PARA">
	<text-after><br></>
</style>

<style name="TBODY"></style>

<style name="ENTRY">
	<select>if(and(attr(namest),attr(nameend)),TD.COLSPEC,if(attr(spanname),TD.SPANSPEC,TD.NOSPAN))	</>
</style>

<style name="TD.NOSPAN">
	<text-before><if(ancestor(THEAD),th,td) if(attr(morerows),rowspan="incr(attr(morerows))",) if(attr(valign),valign="attr(valign)",) if(attr(align),align="switch(attr(align),left,left,right,right,center,center,'DEFAULT',left)",)> </>
	<text-after><join(/)if(ancestor(THEAD),th,td)></>
</style>

<style name="TD.COLSPEC">
	<text-before><if(ancestor(THEAD),th,td) colspan="incr(sub(cnum(attrchild('COLNAME', attr(nameend,me()), ancestor(TGROUP))) , cnum(attrchild('COLNAME', attr(namest,me()),ancestor(TGROUP)))))" if(attr(morerows),rowspan=incr(attr(morerows)),) if(attr(valign),valign="attr(valign)",) if(attr(align),align="switch(attr(align),left,left,right,right,center,center,'DEFAULT',left)",)> </>
	<text-after><join(/)if(ancestor(THEAD),th,td)></>
</style>

<style name="TD.SPANSPEC">
	<text-before><if(ancestor(THEAD),th,td) colspan="incr(sub(cnum(attrchild('COLNAME',attr(nameend,attrchild('SPANNAME',attr(spanname,me()),ancestor(TGROUP,me()))),ancestor(TGROUP,attrchild('SPANNAME',attr(spanname,me()),ancestor(TGROUP,me()))))), cnum(attrchild('COLNAME',attr(namest,attrchild('SPANNAME',attr(spanname,me()),ancestor(TGROUP,me()))),ancestor(TGROUP,attrchild('SPANNAME',attr(spanname,me()),ancestor(TGROUP,me())))))))" if(attr(morerows),rowspan=incr(attr(morerows)),) if(attr(valign),valign="attr(valign)",) if(attr(align),align="switch(attr(align),left,left,right,right,center,center,'DEFAULT',left)",)> </>
	<text-after><join(/)if(ancestor(THEAD),th,td)></>
</style>

<style name="TFOOT">
</style>

<style name="THEAD"></style>

<style name="TOC">
	<hide>	Off	</>
</style>

</sheet>
