; Tune Type 0 = Bach fugue #2 5 notes
; Tune Type 1 = Bach fugue #2 5 notes, backwards
; Tune Type 2 = G Flat  (Played when graphics fails ie when graphics are flat)
	section	Hollywood_Boot_Tune
	page	200
	opt	mex,cex,cc

; DSP address
DSP_BCR		equ	x:$fffe		; bus control register
DSP_PBC		equ	x:$ffe0		; port b control register
DSP_PCC		equ	x:$ffe1		; ssi port c control register
DSP_PBDDR	equ	x:$ffe2		; port b data direction register
DSP_PBD		equ	x:$ffe4		; port b data register
DSP_CRA		equ	x:$ffec		; ssi control register a
DSP_CRB		equ	x:$ffed		; ssi control register b
DSP_SSISR	equ	x:$ffee		; ssi status register
DSP_TX		equ	x:$ffef		; ssi transmit register
DSP_HEADPHONE_MDAC_L	equ	y:$fffc
DSP_HEADPHONE_MDAC_R	equ	y:$fffd

; Pitches of notes.
nz:		equ	0.0
na:		equ	220.0
nas:		equ	233.08188075904495820100
nb:		equ	246.94165062806205590986
nc:		equ	261.62556530059863466908
ncs:		equ	277.18263097687209623640
nd:		equ	293.66476791740756024638
nds:		equ	311.12698372208091071552
ne:		equ	329.62755691286992971007
nf:		equ	349.22823143300388441417
nfs:		equ	369.99442271163439889650
ng:		equ	391.99543598174929404191
ngs:		equ	415.30469757994513847141

vol:	equ	(1/.41)		; Volume control

; Communications area.

dsp_state:      equ     y:$8000
actel_rev:      equ     y:$8001
audio_volume:   equ     y:$8002
tune_type	equ	y:$8003

; Note generation macros.
freq:	macro	freq0,freq1,freq2,freq3
	dc	(freq0*256.0/48000.0)/8388608.0
	dc	(freq1*256.0/48000.0)/8388608.0
	dc	(freq2*256.0/48000.0)/8388608.0
	dc	(freq3*256.0/48000.0)/8388608.0
	endm

ramp:	macro	amp0,amp1,amp2,amp3,dur
	dc	@lng(@frc((amp0*vol-end0)/dur),@frc((amp1*vol-end1)/dur))
	dc	@lng(@frc((amp2*vol-end2)/dur),@frc((amp3*vol-end3)/dur))
	dc	@lng(0,dur)
end0:	set	amp0*vol
end1:	set	amp1*vol
end2:	set	amp2*vol
end3:	set	amp3*vol
	endm

endpt:	macro	amp0,amp1,amp2,amp3
end0:	set	amp0*vol
end1:	set	amp1*vol
end2:	set	amp2*vol
end3:	set	amp3*vol
	endm

note1:	macro
	freq  	nf,nf,nf,nf
	ramp	.0,.0,.0,.0,81
	ramp	.0,.0,.0,.0,10000
	ramp	.0,.0,.0,.0,5001
	endm

note2:	macro
	freq	nc,nc*4,nz,nz
	ramp	.2,.01,.0,.0,(200/2)
	ramp	.2,.01,.0,.0,(5000)
	ramp	.0,.0,.0,.0,(200/2)
	endm

note3:	macro
	freq	nc,ne,nz,nz
	ramp	.1,.1,.0,.0,(200/2)
	ramp	.05,.05,.0,.0,(5000)
	ramp	.0,.0,.0,.0,(200/2)
	endm

note4:	macro
	freq	nc,ne,ng,nz
	ramp	.1,.1,.1,.0,(200/2)
	ramp	.05,.05,.0,.05,(5000)
	ramp	.0,.0,.0,.0,(200/2)
	endm

note5:	macro
	freq	nc,ne,ng,nc*2
	ramp	.1,.1,.1,.1,(200/2)
	ramp	.05,.05,.05,.05,(5000)
	ramp	.0,.0,.0,.0,(48000)
	endm

note6:	macro
	freq	nfs,nfs*2,nfs*4,nfs/2
	ramp	.1,.1,.1,.1,(200/2)
	ramp	.05,.05,.05,.05,(5000)
	ramp	.0,.0,.0,.0,(48000)
	endm

	org	p:$e000
Start:
	movep	#>$4,x:<<DSP_BCR		; 4 wait states per I/O access
	movec	#<6,omr				; enable rom

	movep	#$0,x:DSP_PBC		;Configure Port b
	movep	#$7fff,x:DSP_PBDDR

	movep	#$0,x:DSP_PCC
	movep	#$6000,x:DSP_CRA	;Configure the ssi.
	movep	#$3100,x:DSP_CRB
	movep	#$1F8,x:DSP_PCC		;Make port c be the ssi

        movep   #$6600,x:DSP_PBD        ;Clear the HAL1 simulation register
        movep   #$2600,x:DSP_PBD
        movep   #$6600,x:DSP_PBD

	movep	#$6201,x:DSP_PBD	;Reset the AES transceiver
	movep	#$2201,x:DSP_PBD
	movep	#$6201,x:DSP_PBD

	movep	#$6900,x:DSP_PBD	;Disable the A/D convertor
	movep	#$2900,x:DSP_PBD
	movep	#$6900,x:DSP_PBD

	movep	#$6a8d,x:DSP_PBD	;no diag mode, use aes clock, go
	movep	#$2a8d,x:DSP_PBD
	movep	#$6a8d,x:DSP_PBD

	movep	#$6C00,x:DSP_PBD	;Misc xmit control
	movep	#$2C00,x:DSP_PBD
	movep	#$6C00,x:DSP_PBD

        movep   #$7f00,x:DSP_PBDDR      ;Read the revision level.
        movep   #$4500,x:DSP_PBD
        do      #100,_revloop           ; (10 usec delay).
        nop
_revloop:
        movep   x:DSP_PBD,a1
        movep   #$6500,x:DSP_PBD
        movep   #$7fff,x:DSP_PBDDR

        move    a1,y:actel_rev          ;Tell the MIPS about it.
        move    #1,a1
        move    a1,y:dsp_state

;       Skip the tune if the volume is set to zero.

        move    y:audio_volume,a
        tst     a
        jeq     done

;	Download the executable code into the DSP's internal SRAM.

	move	#PSpaceMovable,r1
	move	#$40,r2
	do	#(PSpaceEnding-PSpaceBegining),PSpaceDownloadLoopEnd
	move	p:(r1)+,a1
	move	a1,p:(r2)+
PSpaceDownloadLoopEnd:

;	Download the y space code into the DSP's internal SRAM.

	move	#YSpaceMovable,r1
	move	#$0,r2
	do	#(YSpaceEnding-YSpaceBegining),YSpaceDownloadLoopEnd
	move	p:(r1)+,a1
	move	a1,y:(r2)+
YSpaceDownloadLoopEnd:

;	Download the l space code into the DSP's internal SRAM.

	move	#LSpaceMovable,r1
	do	#(LSpaceEnding-LSpaceBegining),LSpaceDownloadLoopEnd
	move	l:(r1)+,a10
	move	a10,l:(r2)+
LSpaceDownloadLoopEnd:

;       Set the dacs to zero.

        clr a
_tx1:   jclr    #6,x:DSP_SSISR,_tx1     ; Output left channel.
        move    a1,x:DSP_TX
_tx2:   jclr    #6,x:DSP_SSISR,_tx2     ; Output right channel.
        move    a1,x:DSP_TX
_tx3:   jclr    #6,x:DSP_SSISR,_tx3     ; Wait till all done.

;       Let them settle.
        move    #3200,b1
        do      b,_gainwait     ; Wait
        nop
_gainwait:

;       Slowly bring up the gain so that there's no click.

        clr a
gainloop:
        clr b
        move    #1000,b1
        do      b,_gainwait     ; Wait
        nop
_gainwait:
        clr b
        move    #1,b1           ; Add 1 to the volume
        add     b,a
        move    a1,y:DSP_HEADPHONE_MDAC_L       ; Put it into the dacs
        move    a1,y:DSP_HEADPHONE_MDAC_R
        move    y:audio_volume,b
        cmp     a,b
        jgt     gainloop

;	Clear the fractions.

	clr	a
	move	a1,y:frac0
	move	a1,y:frac1
	move	a1,y:frac2
	move	a1,y:frac3
	move	a,l:amp0	;x:amp0 and y:amp1
	move	a,l:amp2 	;x:amp2 and y:amp3
	move	#$100,a1
	move	a1,y:r0save
	move	#$120,a1
	move	a1,y:r1save
	move	#$140,a1
	move	a1,y:r2save
	move	#$160,a1
	move	a1,y:r3save

;	Decode the tune type.

	clr a
	clr b	y:tune_type,a1
	tst a
	jne	nottune0
	move	#tab0,r1
	jmp	foundatune
nottune0:
	move	#1,b1
	cmp	a,b
	jne	nottune1
	move	#tab1,r1
	jmp	foundatune
nottune1:
	move	#2,b1
	cmp	a,b
	jne	nottune2
	move	#tab2,r1
	jmp	foundatune
nottune2:
	jmp	done	;If it's not tune 0, 1 or 2, then don't play.
foundatune:

;	Jump into the DSP's internal SRAM.

	move	r1,y:NoteTablePtr ; Set up pointers into the tone tables.
	jmp	PSpaceBegining
	
done:   move    #2,a1                   ; Tell the MIPS that we are done.
        move    a1,y:dsp_state
sleep:  jmp     sleep

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The following code will be relocated into the internal SRAM's P space      ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PSpaceMovable:
	org	pli:$40,p:
PSpaceBegining:

nextnote:
	move	#$ffff,m1
	move	y:NoteTablePtr,r1
	move	m1,m2
	move	y:(r1)+,a
	tst	a a,r2
	jlt	done
	move	r1,y:NoteTablePtr

	move	#inc0,r1
	do	#6,freqloopend		; Set up the freqs and the increments
	move	l:(r2)+,a10
	move	a10,l:(r1)+
freqloopend:
	move	y:(r2)+,a1
	move	a1,y:LoopCount
	move	r2,y:sectionptr
	jsr	PlayIt			; Output the attack

	move	#incamp0,r1
	move	y:sectionptr,r2
	do	#2,sustainloopend
	move	l:(r2)+,a10
	move	a10,l:(r1)+
sustainloopend:
	move	y:(r2)+,a1
	move	a1,y:LoopCount
	move	r2,y:sectionptr
	jsr	PlayIt			; Output the sustain

	move	#incamp0,r1
	move	y:sectionptr,r2
	do	#2,decayloopend
	move	l:(r2)+,a10
	move	a10,l:(r1)+
decayloopend:
	move	y:(r2)+,a1
	move	a1,y:LoopCount
	jsr	PlayIt			; Output the decay

	jmp	nextnote

PlayIt:
	move	#255,m0
	move	m0,m1
	move	m0,m2
	move	m0,m3

	move	y:r0save,r0
	move	y:r1save,r1
	move	y:r2save,r2
	move	y:r3save,r3

	do	y:LoopCount,SoundLoopEnd

	clr a	l:inc0,x
	clr b	y:frac0,a0
	add	x,a
	move	a1,n0
	move	a0,a1
	lsr a	a0,y:frac0
	move	a1,y1			; y1-frac0

	move	y:(r0+n0),x1		; x1=sampleL
	move	(r0)+
	tfr	x1,a y:(r0+n0),b 	; b=sampleR, a=sampleL
	sub	a,b	(r0)-		; b= (sampleR-sampleL)
	move	b,x0			; x0 = (sampleR-sampleL)
	macr	y1,x0,a (r0)+n0	; a = sampleL - frac0*(sampleR-sampleL)

	move	x:amp0,x0 a,y0
	mpyr	x0,y0,b x:incamp0,a
	add	x0,a b1,y:sample0
	move	a,x:amp0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	clr a	l:inc1,x
	clr b	y:frac1,a0
	add	x,a
	move	a1,n1
	move	a0,a1
	lsr a	a0,y:frac1
	move	a1,y1

	move	y:(r1+n1),x1		; x1=sampleL
	move	(r1)+
	tfr	x1,a y:(r1+n1),b 	; b=sampleR, a=sampleL
	sub	a,b	(r1)-		; b= (sampleR-sampleL)
	move	b,x0			; x0 = (sampleR-sampleL)
	macr	y1,x0,a (r1)+n1		; a = sampleL - frac1*(sampleR-sampleL)

	move	y:amp1,y0 a,x1
	mpyr	y0,x1,b y:incamp1,a
	add	y0,a  b1,y:sample1
	move	a,y:amp1
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	clr a	l:inc2,x
	clr b	y:frac2,a0
	add	x,a
	move	a1,n2
	move	a0,a1
	lsr a	a0,y:frac2
	move	a1,y1

	move	y:(r2+n2),x1		; x1=sampleL
	move	(r2)+
	tfr	x1,a y:(r2+n2),b 	; b=sampleR, a=sampleL
	sub	a,b	(r2)-		; b= (sampleR-sampleL)
	move	b,x0			; x0 = (sampleR-sampleL)
	macr	y1,x0,a (r2)+n2		; a = sampleL - frac2*(sampleR-sampleL)

	move	x:amp2,x0 a,y0
	mpyr	x0,y0,b x:incamp2,a
	add	x0,a b1,y:sample2
	move	a,x:amp2
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	clr a	l:inc3,x
	clr b	y:frac3,a0
	add	x,a
	move	a1,n3
	move	a0,a1
	lsr a	a0,y:frac3
	move	a1,y1

	move	y:(r3+n3),x1		; x1=sampleL
	move	(r3)+
	tfr	x1,a y:(r3+n3),b 	; b=sampleR, a=sampleL
	sub	a,b	(r3)-		; b= (sampleR-sampleL)
	move	b,x0			; x0 = (sampleR-sampleL)
	macr	y1,x0,a (r3)+n3	; a = sampleL - frac3*(sampleR-sampleL)

	move	y:amp3,y0 a,x1
	mpyr	y0,x1,b y:incamp3,a
	add	y0,a b1,y:sample3
	move	a,y:amp3
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	move	y:sample2,a
	add	b,a	y:sample1,b
	add	b,a	y:sample0,b
	add	b,a

tx1:	jclr	#6,x:DSP_SSISR,tx1	; Output left channel.
	move	a1,x:DSP_TX
tx2:	jclr	#6,x:DSP_SSISR,tx2	; Output right channel.
	move	a1,x:DSP_TX
	nop
	nop
SoundLoopEnd:
	move	r0,y:r0save
	move	r1,y:r1save
	move	r2,y:r2save
	move	r3,y:r3save
	rts
PSpaceEnding:

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The following code will be relocated into the internal SRAM's Y space      ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	org	p:
YSpaceMovable:
		org	yli:$0,p:
YSpaceBegining:
r0save:	dc	$100
r1save:	dc	$180
r2save:	dc	$140
r3save:	dc	$1c0
tab0:
	dc	Note1
	dc	Note2
	dc	Note3
	dc	Note4
	dc	Note5
	dc	-1
tab0end:
tab1:
	dc	Note1
	dc	Note5
	dc	Note4
	dc	Note3
	dc	Note2
	dc	-1
tab1end:
tab2:
	dc	Note1
	dc	Note6
	dc	-1
tab2end:
NoteTablePtr 	equ	tab2end
sectionptr 	equ	NoteTablePtr+1
LoopCount:	equ	sectionptr+1
frac0:		equ	LoopCount+1
frac1:		equ	frac0+1
frac2:		equ	frac1+1
frac3:		equ	frac2+1
sample0:	equ	frac3+1
sample1:	equ	sample0+1
sample2:	equ	sample1+1
sample3:	equ	sample2+1
YSpaceEnding:	equ	frac3+1
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The following code will be relocated into the internal SRAM's L space      ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	org	p:
LSpaceMovable:
	org	lli:@cvs(l,YSpaceEnding),l:@cvs(l,LSpaceMovable)
LSpaceBegining:
	endpt	0,0,0,0
Note1:		note1
Note2:		note2
Note3:		note3
Note4:		note4
Note5:		note5
Note6:		note6
amp0:		dc	@lng(0,0)
amp1		equ	amp0
amp2:		dc	@lng(0,0)
amp3		equ	amp2
inc0:		equ	amp3+1
inc1:		equ	inc0+1
inc2:		equ	inc1+1
inc3:		equ	inc2+1
incamp0		equ	inc3+1
incamp1		equ	incamp0
incamp2		equ	incamp1+1
incamp3		equ	incamp2
LSpaceEnding:	equ	incamp3+1
	endsec
