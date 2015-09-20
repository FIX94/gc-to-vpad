#I am so sorry for my bad and lengthy asm

.globl _start
.globl DoAnalogConv

.extern _main
_start:
	mr %r3, %r31 #original line
	mr %r4, %r27 #for data inject
	b _main

.set tmp, 0
.set dst, 3
.set src, 4
.set val, 5

.set ftmp, 0
.set fval, 1
.set fp01, 2
.set fn01, 3
.set fp10, 4
.set fn10, 5
.set zero, 6
.set prec, 7

IntToFloat:
	lis tmp, 0x4330
	stw tmp, 0xC(%r1)
	lis tmp, 0x8000
	stw tmp, 0x10(%r1)
	lfd ftmp, 0xC(%r1)
	lis tmp, 0x4330
	stw tmp, 0xC(%r1) # store upper half
	xoris val, val, 0x8000 # flip sign bit
	stw val, 0x10(%r1) # store lower half
	lfd fval, 0xC(%r1) # float load double of value
	fsub fval, fval, ftmp # subtract 0x4330000080000000
	frsp fval, fval
	blr

FloatClamp:
	fcmpo 1, fval, fp01
	bgt 1, p10lim
	fcmpo 1, fval, fn01
	blt 1, n10lim
	fmr fval, zero
	b fcend
n10lim:
	fsubs fval, fval, fn01
	fcmpo 1, fval, fn10
	bgt 1, fcend
	fmr fval, fn10
	b fcend
p10lim:
	fsubs fval, fval, fp01
	fcmpo 1, fval, fp10
	blt 1, fcend
	fmr fval, fp10
fcend:
	blr

DoAnalogConv:
	stwu %r1, -0x20(%r1)
	mflr tmp
	stw tmp, 0x24(%r1)

	lis tmp, 0x3dcc #float for about 0.1
	stw tmp, 0xC(%r1)
	lfs fp01, 0xC(%r1)

	lis tmp, 0xbdcc #float for about -0.1
	stw tmp, 0xC(%r1)
	lfs fn01, 0xC(%r1)

	lis tmp, 0x3f80 #float for 1.0
	stw tmp, 0xC(%r1)
	lfs fp10, 0xC(%r1)

	lis tmp, 0xbf80 #float for -1.0
	stw tmp, 0xC(%r1)
	lfs fn10, 0xC(%r1)

	li tmp, 0 #float for 0
	stw tmp, 0xC(%r1)
	lfs zero, 0xC(%r1)

	lis tmp, 0x42aa #float for 85 (main stick 1.1 precision)
	stw tmp, 0xC(%r1)
	lfs prec, 0xC(%r1)

	lbz val, 0(src) #left stick x unsigned char
	addic val, val, -0x80 #make it -0x80 to 0x7F
	bl IntToFloat #fval now contains float represent
	fdivs fval, fval, prec #change to -1 to 1
	bl FloatClamp #makes sure we have deadzone
	stfs fval, 0(dst) #left stick x float

	lbz val, 1(src) #left stick y unsigned char
	addic val, val, -0x80 #make it -0x80 to 0x7F
	bl IntToFloat #fval now contains float represent
	fdivs fval, fval, prec #change to -1 to 1
	bl FloatClamp #makes sure we have deadzone
	stfs fval, 4(dst) #left stick y float

	lis tmp, 0x4296 #float for 75 (c-stick 1.1 precision)
	stw tmp, 0xC(%r1)
	lfs prec, 0xC(%r1)

	lbz val, 2(src) #right stick x unsigned char
	addic val, val, -0x80 #make it -0x80 to 0x7F
	bl IntToFloat #fval now contains float represent
	fdivs fval, fval, prec #change to -1 to 1
	bl FloatClamp #makes sure we have deadzone
	stfs fval, 8(dst) #right stick x float

	lbz val, 3(src) #right stick y unsigned char
	addic val, val, -0x80 #make it -0x80 to 0x7F
	bl IntToFloat #fval now contains float represent
	fdivs fval, fval, prec #change to -1 to 1
	bl FloatClamp #makes sure we have deadzone
	stfs fval, 12(dst) #right stick y float

	lwz tmp, 0x24(%r1)
	mtlr tmp
	addi %r1, %r1, 0x20
	blr
