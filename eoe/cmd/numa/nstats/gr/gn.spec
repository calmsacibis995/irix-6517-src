product gn
id "gn Description"
image sw
    id "Software"
    version 1
    order 9999
    subsys base default
        id "Base Software"
        replaces self
        exp gn.sw.base
        prereq (
        eoe.sw.unix      1232000000 maxint
        x_eoe.sw.eoe     1232000000 maxint
        motif_eoe.sw.eoe 1232000000 maxint
        ViewKit_eoe.sw.base 1232000000 maxint
        )
    endsubsys
    subsys optional
        id "Optional Software"
        replaces self
        exp gn.sw.optional
    endsubsys
endimage
image man
    id "Man Pages"
    version 1
    order 9999
    subsys manpages
        id "Man Pages"
        replaces self
        exp gn.man.manpages
    endsubsys
    subsys relnotes
        id "Release Notes"
        replaces self
        exp gn.man.relnotes
    endsubsys
    endimage
    endproduct
maint maint_gn
endmaint
