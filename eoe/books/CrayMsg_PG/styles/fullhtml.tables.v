<!-- 	Cray Research, A Silicon Graphics Company
	Software Publications
	DocBook DTD stylesheet, text-only tables version
	August 1996
	Tom Cox
-->


<sheet >



<?INSTED COMMENT: GROUP admonition>

<group name="admonition">
	<break-before>	Line	</>
	<text-before><blockquote></>
	<text-after><join(/)blockquote></>
</group>

<style name="CAUTION" group="admonition">
	<text-before><blockquote><img src="/icons/caution.gif" alt="[caution]" align=left><b>Caution: <join(/)b></>
</style>

<style name="IMPORTANT" group="admonition">
	<text-before><blockquote><b>Important: <join(/)b></>
</style>

<style name="NOTE" group="admonition">
	<text-before><blockquote><b>Note: <join(/)b></>
</style>

<style name="TIP" group="admonition">
	<text-before><blockquote><b>Tip: <join(/)b></>
</style>

<style name="WARNING" group="admonition">
	<text-before><blockquote><b>Warning: <join(/)b></>
</style>



<?INSTED COMMENT: GROUP code>

<group name="code">
	<foreground>	coral	</>
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
	<left-indent>	+=25	</>
	<break-before>	Line	</>
	<text-before><dd><p></>
	<text-after><join(/)p><join(/)dd></>
</group>

<style name="DEFLISTENTRY,LISTITEM" group="dd">
</style>

<style name="GLOSSDEF" group="dd">
	<left-indent>	150	</>
	<width>	200	</>
	<break-after>	Line	</>
</style>

<style name="HEAD2" group="dd">
</style>

<style name="REVHISTORY,REVISION,DATE" group="dd">
</style>

<style name="REVHISTORY,REVISION,REVREMARK" group="dd">
</style>



<?INSTED COMMENT: GROUP displayhead>

<group name="displayhead">
	<break-before>	Line	</>
	<text-before><p align=center></>
	<text-after><join(/)p></>
</group>

<style name="ABSTRACT,TITLE" group="displayhead">
</style>

<style name="DEFLIST,TITLE" group="displayhead">
	<text-before><p align=center></>
</style>

<style name="EQUATION,TITLE" group="displayhead">
</style>

<style name="EXAMPLE,TITLE" group="displayhead">
	<text-before><p align=center>Example gcnum(ancestor(example)). </>
</style>

<style name="FIGURE,TITLE" group="displayhead">
	<text-before><p align=center>Figure gcnum(ancestor(figure)). </>
</style>

<style name="IMPORTANT,TITLE" group="displayhead">
</style>



<?INSTED COMMENT: GROUP dl>

<group name="dl">
	<foreground>	RoyalBlue4	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before>if(ancestor(para),<br><br>,)<dl></>
	<text-after><join(/)dl></>
</group>

<style name="DEFLIST" group="dl">
</style>

<style name="GLOSSENTRY" group="dl">
</style>

<style name="REVHISTORY" group="dl">
	<text-before> <h2>Record of Revision <join(/)h2><dl></>
</style>



<?INSTED COMMENT: GROUP dt>

<group name="dt">
	<break-before>	Line	</>
	<text-before><dt><p></>
	<text-after><join(/)p><join(/)dt></>
</group>

<style name="GLOSSENTRY,GLOSSTERM" group="dt">
	<text-before><dt><p><em></>
	<text-after><join(/)em><join(/)p><join(/)dt></>
</style>

<style name="HEAD1" group="dt">
</style>

<style name="REVHISTORY,REVISION,REVNUMBER" group="dt">
</style>

<style name="TERM" group="dt">
</style>



<?INSTED COMMENT: GROUP em>

<group name="em">
	<font-slant>	Italics	</>
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



<?INSTED COMMENT: GROUP h1>

<group name="h1">
	<font-family>	lucida	</>
	<font-weight>	Bold	</>
	<font-size>	24	</>
	<foreground>	blue3	</>
	<line-spacing>	24	</>
	<break-before>	Line	</>
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
	<font-family>	lucida	</>
	<font-weight>	Bold	</>
	<font-size>	16	</>
	<foreground>	blue3	</>
	<line-spacing>	16	</>
	<break-before>	Line	</>
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
	<font-family>	lucida	</>
	<font-weight>	Bold	</>
	<foreground>	blue3	</>
	<break-before>	Line	</>
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
	<font-family>	lucida	</>
	<font-weight>	Bold	</>
	<font-slant>	Italics	</>
	<foreground>	blue3	</>
	<break-before>	Line	</>
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
	<font-family>	lucida	</>
	<font-weight>	Medium	</>
	<font-slant>	Italics	</>
	<foreground>	blue3	</>
	<break-before>	Line	</>
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
	by exactly one blank line. This is the best way to handle all
	the different combinations that are used. The contents of each
	li is contained in a p to guarantee the spacing.
  -->
<group name="li">
	<left-indent>	+=15	</>
	<first-indent>	-15	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before><li><p></>
	<text-after><join(/)p><join(/)li></>
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
	<font-family>	fixed	</>
	<text-before><code></>
	<text-after><join(/)code></>
</group>

<style name="COMMAND" group="mono">
	<text-after><join(/)code>if(attr(sectionref),if(eq(attr(sectionref),blank),,\(attr(sectionref)\)),)</>
</style>

<style name="FILENAME" group="mono">
</style>

<style name="HARDWARE" group="mono">
	<foreground>	SandyBrown	</>
	<text-before><kbd></>
	<text-after><join(/)kbd></>
</style>

<style name="INTERFACE" group="mono">
	<break-after>	Line	</>
	<text-before><code>if(eq(tag(ancestor()),interface),\ -> ,)</>
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
	<font-family>	fixed	</>
	<foreground>	DarkOrange3	</>
	<break-before>	Line	</>
	<text-before><pre></>
	<text-after><join(/)pre></>
</group>

<style name="INTERFACEDEFINITION" group="monodisplay">
</style>

<style name="LITERALLAYOUT" group="monodisplay">
</style>

<style name="PROGRAMLISTING" group="monodisplay">
</style>

<style name="SCREEN" group="monodisplay">
</style>

<style name="SYNOPSIS" group="monodisplay">
	<text-before> <p><code> </>
	<text-after> <join(/)code> </>
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
	<space-before>	14	</>
	<break-before>	Line	</>
	<break-after>	Line	</>
	<text-before><p></>
	<text-after><join(/)p></>
</group>

<style name="LEGALNOTICE,PARA" group="p">
</style>

<style name="LISTITEM,PARA" group="p">
</style>

<style name="PARA" group="p">
</style>



<?INSTED COMMENT: GROUP procedure>

<group name="procedure">
</group>

<style name="PROCEDURE" group="procedure">
	<left-indent>	+=10	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before><h3>Procedure gcnum(me()): if(not(typechild(title)),<join(/)h3><ol>,)</>
	<text-after><join(/)ol></>
</style>

<!-- Procedures took some finagling; notice where h3 and ol open.
  -->
<style name="PROCEDURE,TITLE" group="procedure">
	<font-family>	lucida	</>
	<font-weight>	Bold	</>
	<foreground>	blue3	</>
	<break-before>	Line	</>
	<text-before><!-- h3 already opened by procedure element --></>
	<text-after><join(/)h3><ol></>
</style>

<style name="STEP" group="procedure">
	<left-indent>	+=15	</>
	<first-indent>	-15	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before><li><p></>
	<text-after><join(/)p><join(/)li></>
</style>

<style name="SUBSTEPS" group="procedure">
	<left-indent>	+=10	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before>if(ancestor(substeps),<ol type=1>,<ol type=a>)</>
	<text-after><join(/)ol></>
</style>



<?INSTED COMMENT: GROUP samp>

<group name="samp">
	<foreground>	burlywood4	</>
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
	<text-before><p>[Equation gcnum()] </>
</style>

<style name="INLINEEQUATION" group="suppressed">
	<text-before>[Inline equation gcnum()] </>
</style>

<style name="NOTE,TITLE" group="suppressed">
</style>

<style name="SCREENINFO" group="suppressed">
</style>

<style name="WARNING,TITLE" group="suppressed">
</style>



<?INSTED COMMENT: GROUP ul>

<!--	The first list within a paragraph needs space added before it. -->
<group name="ul">
	<left-indent>	+=10	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<text-before>if(ancestor(para),<p>,)<ul></>
	<text-after><join(/)ul></>
</group>

<style name="ITEMIZEDLIST" group="ul">
</style>



<?INSTED COMMENT: GROUP wrapper>

<!-- 	The wrapper group includes elements whose primary purpose is to 
	contain other elements, usually with little formatting of their own.
  -->
<group name="wrapper">
	<break-before>	Line	</>
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

<style name="SCREENSHOT" group="wrapper">
</style>

<style name="SECTION" group="wrapper">
</style>

<style name="TGROUP" group="wrapper">
</style>

<?INSTED COMMENT: GROUP xrefs>

<group name="xrefs">
	<foreground>	red1	</>
	<score>	Under	</>
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
default         [Click here]

Programming note: The select statement breaks unless the DEFAULT item 
of gamut() is protected by join().
-->
	<select>	XREF.if(attr(xreflabel, me()),XREFLABEL,gamut(if(eq(tag(idmatch(attr(linkend))),title), tag(ancestor(idmatch(attr(linkend)))), tag(idmatch(attr(linkend)))), 'appendix chapter equation example figure inlineequation para preface procedure section step table','APPENDIX CHAPTER EQUATION EXAMPLE FIGURE INLINEEQUATION PARA PREFACE PROCEDURE SECTION STEP TABLE', join('DEFAULT')))	</>
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
	<font-family>	times	</>
	<font-weight>	Medium	</>
	<font-slant>	Roman	</>
	<font-video>	Regular	</>
	<font-size>	14	</>
	<line-spacing>	17	</>
</style>

<style name="#FOOTER">
	<text-before><A HREF="http:/docs/help/help_all.html"><img src="/images/help.gif" alt="Help " align=left><join(/)A><A HREF="http:/"><img src="/images/home.gif" alt="Home" align=left><join(/)A><br><br><br><br></>
	<text-after><P><SMALL>Copyright (c) 1997 <A HREF="http://www.cray.com">Cray Research, Inc.<join(/)A><join(/)SMALL></>
</style>

<style name="#HEADER">
	<text-before><body bgcolor="#ffffff"><img src="/images/craylogo.gif" alt="Cray Logo" align=right><br><br><br><br></>
</style>

<style name="#QUERY">
	<break-before>	Line	</>
</style>

<style name="#ROOT">
	<break-before>	Line	</>
</style>

<style name="#SDATA">
	<text-before>gamut('trade  mdash  bull','(TM)  --  *',join('&',attr(name).';'))</>
</style>

<style name="#TAGS">
	<font-weight>	Bold	</>
	<foreground>	purple	</>
	<break-before>	Line	</>
</style>

<style name="ACKNOWLEDGEMENTS">
	<break-before>	Line	</>
</style>

<style name="ALT-TITLE">
	<break-before>	Line	</>
	<break-after>	Line	</>
</style>

<style name="BLOCKQUOTE">
	<left-indent>	+=10	</>
	<space-before>	14	</>
	<break-before>	Line	</>
	<break-after>	Line	</>
	<text-before><blockquote></>
	<text-after><join(/)blockquote></>
</style>

<style name="CITETITLE">
	<foreground>	DarkOrchid2	</>
	<text-before><cite></>
	<text-after><join(/)cite></>
</style>

<style name="CMDSYNOPSIS">
	<break-after>	Line	</>
</style>

<style name="COLSPEC">
	<break-before>	Line	</>
</style>

<style name="COMMENT">
        <text-before>[Cray review comment]<em></>
        <text-after><join(/)em>[End Cray review comment]</>
</style>

<style name="COPYRIGHT">
	<break-before>	Line	</>
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
	<foreground>	BlueViolet	</>
	<hide>	Children	</>
	<script>	ebt-reveal stylesheet="fullhtml.v" title="Footnote"	</>
	<text-before>[Footnote gcnum()]</>
</style>

<style name="GRAPHIC">
	<space-before>	12	</>
	<icon-position>	Right	</>
	<break-before>	Line	</>
	<inline>	raster filename="attr(entityref)"	</>
</style>

<style name="INDEX">
	<hide>	All	</>
</style>

<style name="INDEXTERM">
	<hide>	Text	</>
</style>

<style name="LINEANNOTATION">
	<font-family>	lucida	</>
	<break-before>	Line	</>
	<break-after>	Line	</>
</style>

<style name="LINK">
	<font-weight>	Bold	</>
	<foreground>	red3	</>
	<score>	Under	</>
	<break-before>	None	</>
	<text-before> <a href="attr(href)"> </>
	<text-after> join('<','/a>') </>
</style>

<style name="NEWLINE">
	<break-after>	Line	</>
	<text-before><br></>
</style>

<style name="PUBNUMBER">
	<text-before>Publication </>
	<text-after><h2>Notices<join(/)h2></>
</style>

<style name="PUBNUMBER,PUBTYPE">
	<break-before>	Line	</>
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

<style name="REVEND">
        <text-after><p>**** Change end ****<p></>
</style>

<style name="REVNUMBER">
	<space-before>	12	</>
	<break-before>	Line	</>
</style>

<style name="REVST">
        <text-before><p>**** Change start ****<p></>
</style>

<style name="ROW">
	<foreground>	DarkGreen	</>
	<text-before> <tr valign=top> </>
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
	<left-indent>	+=15	</>
	<break-before>	Line	</>
	<text-after>if(islast(),,<br>)</>
</style>

<style name="SIMPLELIST.MEMBER.INLINE">
	<text-before>if(isfirst(),,if(islast(),join(' ')and ,join(',',' ')))</>
</style>

<style name="SUBSCRIPT">
	<text-before>_\(</>
	<text-after>)</>
</style>

<style name="SUPERSCRIPT">
	<text-before>^\(</>
	<text-after>)</>
</style>

<style name="TABLE">
	<foreground>	DarkOliveGreen4	</>
	<break-before>	Line	</>
	<text-before><p align=center>Table gcnum(). content(typechild(title))<br><br><pre></>
	<text-after><join(/)pre></>
</style>

<!-- TABLE,TITLE belongs in group suppressed -->
<style name="TABLE,TITLE">
	<hide>	Children	</>
</style>

<!-- The variable ln is the line number.
  -->
<style name="TABLE.ROW.2COL">
	<foreground>	DarkGreen	</>
	<break-before>	Line	</>
	<text-before>mapword('0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20',ln,'if(ge(div(max(length(content(typechild(entry,me(),1))),length(content(typechild(entry,me(),2)))),36),var(ln)),<br>| left(if(ge(length(content(typechild(entry,me(),1))),mult(var(ln),36)),substr(content(typechild(entry,me(),1)),incr(mult(var(ln),36)),36),''),36,\ )\ | left(if(ge(length(content(typechild(entry,me(),2))),mult(var(ln),36)),substr(content(typechild(entry,me(),2)),incr(mult(var(ln),36)),36),''),36,\ )\ |,)')</>
	<text-after><br>if(eq(tag(ancestor()),thead),===============================================================================,+-----------------------------------------------------------------------------+)</>
</style>

<style name="TABLE.ROW.3COL">
	<foreground>	DarkGreen	</>
	<break-before>	Line	</>
	<text-before>mapword('0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20',ln,'if(ge(div(max(length(content(typechild(entry,me(),1))),length(content(typechild(entry,me(),2))),length(content(typechild(entry,me(),3)))),23),var(ln)),<br>| left(if(ge(length(content(typechild(entry,me(),1))),mult(var(ln),23)),substr(content(typechild(entry,me(),1)),incr(mult(var(ln),23)),23),''),23,\ )\ | left(if(ge(length(content(typechild(entry,me(),2))),mult(var(ln),23)),substr(content(typechild(entry,me(),2)),incr(mult(var(ln),23)),23),''),23,\ )\ | left(if(ge(length(content(typechild(entry,me(),3))),mult(var(ln),23)),substr(content(typechild(entry,me(),3)),incr(mult(var(ln),23)),23),''),23,\ )\ |,)')</>
	<text-after><br>if(eq(tag(ancestor()),thead),===============================================================================,+-----------------------------------------------------------------------------+)</>
</style>

<style name="TABLE.ROW.4COL">
	<foreground>	DarkGreen	</>
	<break-before>	Line	</>
	<text-before>mapword('0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20',ln,'if(ge(div(max(length(content(typechild(entry,me(),1))),length(content(typechild(entry,me(),2))),length(content(typechild(entry,me(),3))),length(content(typechild(entry,me(),4)))),16),var(ln)),<br>| left(if(ge(length(content(typechild(entry,me(),1))),mult(var(ln),16)),substr(content(typechild(entry,me(),1)),incr(mult(var(ln),16)),16),''),16,\ )\ | left(if(ge(length(content(typechild(entry,me(),2))),mult(var(ln),16)),substr(content(typechild(entry,me(),2)),incr(mult(var(ln),16)),16),''),16,\ )\ | left(if(ge(length(content(typechild(entry,me(),3))),mult(var(ln),16)),substr(content(typechild(entry,me(),3)),incr(mult(var(ln),16)),16),''),16,\ )\ | left(if(ge(length(content(typechild(entry,me(),4))),mult(var(ln),16)),substr(content(typechild(entry,me(),4)),incr(mult(var(ln),16)),16),''),16,\ )\ |,)')</>
	<text-after><br>if(eq(tag(ancestor()),thead),=============================================================================,+---------------------------------------------------------------------------+)</>
</style>

<style name="TABLE.ROW.5COL">
	<foreground>	DarkGreen	</>
	<break-before>	Line	</>
	<text-before>mapword('0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20',ln,'if(ge(div(max(length(content(typechild(entry,me(),1))),length(content(typechild(entry,me(),2))),length(content(typechild(entry,me(),3))),length(content(typechild(entry,me(),4))),length(content(typechild(entry,me(),5)))),12),var(ln)),<br>| left(if(ge(length(content(typechild(entry,me(),1))),mult(var(ln),12)),substr(content(typechild(entry,me(),1)),incr(mult(var(ln),12)),12),''),12,\ )\ | left(if(ge(length(content(typechild(entry,me(),2))),mult(var(ln),12)),substr(content(typechild(entry,me(),2)),incr(mult(var(ln),12)),12),''),12,\ )\ | left(if(ge(length(content(typechild(entry,me(),3))),mult(var(ln),12)),substr(content(typechild(entry,me(),3)),incr(mult(var(ln),12)),12),''),12,\ )\ | left(if(ge(length(content(typechild(entry,me(),4))),mult(var(ln),12)),substr(content(typechild(entry,me(),4)),incr(mult(var(ln),12)),12),''),12,\ )\ | left(if(ge(length(content(typechild(entry,me(),5))),mult(var(ln),12)),substr(content(typechild(entry,me(),5)),incr(mult(var(ln),12)),12),''),12,\ )\ |,)')</>
	<text-after><br>if(eq(tag(ancestor()),thead),=============================================================================,+---------------------------------------------------------------------------+)</>
</style>

<style name="TBODY">
	<foreground>	MediumSeaGreen	</>
	<text-before><tbody> </>
</style>

<style name="TBODY,ROW">
	<select>	switch(attr(cols,ancestor(tgroup)),2,TABLE.ROW.2COL,3,TABLE.ROW.3COL,4,TABLE.ROW.4COL,5,TABLE.ROW.5COL,default,TABLE.ROW.5COL)	</>
</style>

<style name="TBODY,ROW,ENTRY">
	<hide>	Children	</>
</style>

<style name="TD.NOSPAN">
	<text-before> <td rowspan=incr(attr(morerows))> </>
</style>

<style name="TD.SPAN">
	<text-before> <td cols=incr(sub(cnum(attrchild(colname,attr(nameend,                  attrchild(spanname,attr(spanname),ancestor(tgroup))),                  ancestor(tgroup))),cnum(attrchild(colname,attr(namest,                  attrchild(spanname,attr(spanname),ancestor(tgroup))),                  ancestor(tgroup))))) rowspan=incr(attr(morerows))> </>
</style>

<style name="TFOOT">
	<text-before> <tfoot> </>
</style>

<style name="TFOOT,ROW,ENTRY">
	<select>	if(attr(spanname),TH.SPAN,TH.NOSPAN)	</>
</style>

<style name="THEAD,ROW">
	<select>	switch(attr(cols,typechild(tgroup,ancestor(table))),2,TABLE.ROW.2COL,3,TABLE.ROW.3COL,4,TABLE.ROW.4COL,5,TABLE.ROW.5COL,default,TABLE.ROW.5COL)	</>
</style>

<style name="THEAD,ROW,ENTRY">
	<hide>	Children	</>
</style>

<style name="TOC">
	<hide>	Off	</>
</style>

</sheet>
