<!-- ..................................................................... -->
<!-- DocBook information pool redeclaration module for V2.3 .............. -->
<!-- Filename: rdbpool.mod ............................................... -->


<!-- This redeclaration module supports Cray Research, Inc., modifications -->
<!-- to DocBook V2.3. These modifications are listed as V1.1.............. -->




<!-- ..................................................................... -->

<!-- This redeclaration module contains the definitions for the following
     information units:

	=> legalnotice.mix


     In the DTD driver file referring to this redeclaration module, use
     an entity declaration that uses the public identifier shown below:

     <!ENTITY % rdbpool	PUBLIC	
      "-//Davenport//ELEMENTS DocBook Information Pool
      Redeclarations//EN"

-->

<!-- ..................................................................... -->

<!-- Remove list.class, admon.class, linespecific.class, and blockquote
     from legalnotice.mix	-->

<!ENTITY % local.legalnotice.mix 	" "	>
<!ENTITY % legalnotice.mix
		"%para.class;"	>



<!-- Redefine admon.mix to remove:
	* sidebar
	* bridgehead
-->

<!ENTITY % admon.mix
		"%list.class;
		| %linespecific.class;	| %synop.class;
		| %para.class;		| %informal.class;
		| %formal.class;	| procedure
		| anchor		| comment"	>


<!-- Redefine ubiq.mix to remove beginpage and add string ................ -->
<!-- Reason: beginpage is purely a print composition-related element and
     as such is not desireable in the Cray DocBook DTD ................... -->

<!ENTITY % ubiq.mix
		"%ndxterm.class; & string"	>

<!-- Redefine the entity tblexpt to remove informal table .............. -->
<!-- Reason: informaltable was removed from the DTD as a valid structure
     so its exclusion from the table element is not necessary and in fact
     generates a parser warning message ................................ -->

<!ENTITY % tblexpt
		"-(table)"			>


<!-- Redefine example.mix to add admonitions.class ..................... -->
<!-- Reason: In the current model, admonitions are not allowed as peers 
     to the other block-oriented elements. This would disallow the use
     of an admonition that related to the example as opposed to a component
     within an example ................................................. -->

<!ENTITY % local.example.mix	"">
<!ENTITY % example.mix
		 "%list.class;		| %linespecific.class;
		| %synop.class;		| %para.class;
		| %informal.class;	| %admon.class;
		| table | equation	  %local.example.mix;"	>

