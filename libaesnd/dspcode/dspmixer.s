DMACR:			equ		0xffc9
DMABLEN:		equ		0xffcb
DMADSPM:		equ		0xffcd
DMAMMEMH:		equ		0xffce
DMAMMEML:		equ		0xffcf

ACCOEF:			equ		0xffa0

ACFMT:			equ		0xffd1
ACSAH:			equ		0xffd4
ACSAL:			equ		0xffd5
ACEAH:			equ		0xffd6
ACEAL:			equ		0xffd7
ACCAH:			equ		0xffd8
ACCAL:			equ		0xffd9
ACPDS:			equ		0xffda
ACYN1:			equ		0xffdb
ACYN2:			equ		0xffdc
ACDAT:			equ		0xffdd
ACGAN:			equ		0xffde

DIRQ:			equ		0xfffb
DMBH:			equ		0xfffc
DMBL:			equ		0xfffd
CMBH:			equ		0xfffe
CMBL:			equ		0xffff

DMADMEM:		equ		0x0
DMAIMEM:		equ		0x2
DMA_TO_DSP:		equ		0x0
DMA_TO_CPU:		equ		0x1

/* internal work memory layout */
MEM_REG:		equ		0x0
PB_MADDRH:		equ		MEM_REG
PB_MADDRL:		equ		MEM_REG+0x01

PB_ADDR:		equ		MEM_REG+0x200

/* parameter block layout */
OUTBUF_ADDRH:	equ		PB_ADDR
OUTBUF_ADDRL:	equ		PB_ADDR+0x01

SNDBUF_SADDRH:	equ		PB_ADDR+0x02
SNDBUF_SADDRL:	equ		PB_ADDR+0x03
SNDBUF_EADDRH:	equ		PB_ADDR+0x04
SNDBUF_EADDRL:	equ		PB_ADDR+0x05
SNDBUF_CADDRH:	equ		PB_ADDR+0x06
SNDBUF_CADDRL:	equ		PB_ADDR+0x07

YN1:			equ		PB_ADDR+0x08
YN2:			equ		PB_ADDR+0x09
PDS:			equ		PB_ADDR+0x0a

FREQ_SMPH:		equ		PB_ADDR+0x0b
FREQ_SMPL:		equ		PB_ADDR+0x0c

SMP_CNT:		equ		PB_ADDR+0x0d

SMP_L:			equ		PB_ADDR+0x0e
SMP_R:			equ		PB_ADDR+0x0f

VOLUME_L:		equ		PB_ADDR+0x10
VOLUME_R:		equ		PB_ADDR+0x11

DELAY_SMPH:		equ		PB_ADDR+0x12
DELAY_SMPL:		equ		PB_ADDR+0x13

FLAGS_SMPH:		equ		PB_ADDR+0x14
FLAGS_SMPL:		equ		PB_ADDR+0x15

LOOP_PDS:		equ		PB_ADDR+0x16
LOOP_YN1:		equ		PB_ADDR+0x17
LOOP_YN2:		equ		PB_ADDR+0x18

/* flags and buffers used */
MEM_SMP_BUF:		equ		0x0800
MEM_TMP_BUF:		equ		0x0a00
MEM_SND_BUF:		equ		0x0c00					//buffer for output sound data, will be DMA'd out to OUTBUF_SND

PB_STURCT_SIZE:		equ		64
NUM_SAMPLES:		equ		96						//process 2ms of sample data
DEF_FREQ_INT:		equ		0x0001

VOICE_FLAGL_PAUSE:	equ		0x0008
VOICE_FLAGL_LOOP:	equ		0x0010
VOICE_FLAGL_ONCE:	equ		0x0020

VOICE_FLAGH_END:	equ		0x0010
VOICE_FLAGH_STOP:	equ		0x0020
VOICE_FLAGH_RUN:	equ		0x4000

ACCL_FMT_8BIT:		equ		0x0019
ACCL_FMT_16BIT:		equ		0x000a

ACCL_GAIN_8BIT:		equ		0x0100
ACCL_GAIN_16BIT:	equ		0x0800

_start:
	nop
	nop
	jmp		exception1
	jmp		exception2
	jmp		exception3
	jmp		exception4
	jmp		exception5
	jmp		exception6
	jmp		exception7
	
	sbset	#0x02
	sbset	#0x03
	sbclr	#0x04
	sbset	#0x05
	sbset	#0x06

	s16
	clr15
	m0
	lri		$config,#0xff
	
	lri		$wr0,#0xffff
	lri		$wr1,#0xffff
	lri		$wr2,#0xffff
	lri		$wr3,#0xffff
	
main:
	si		@DMBH,#0xdcd1
	si		@DMBL,#0x0000
	si		@DIRQ,#0x01

recv_cmd:	
	clr		$acc0
	clr		$acc1
	
	call	wait_mail_sent
	call	wait_mail_recv
	
	lri		$acc0.m,#0xcdd1
	cmp
	jeq		sys_commands
	
	lri		$acc0.m,#0xface
	cmp
	jne		wait_commands
	
	lrs		$acc1.m,@CMBL
	
	cmpi	$acc1.m,#0x0010
	jeq		process_first_voice
	
	cmpi	$acc1.m,#0x0020
	jeq		process_next_voice
	
	cmpi	$acc1.m,#0x0080
	jeq		get_pb_address
	
	cmpi	$acc1.m,#0x0100
	jeq		send_samples
	
	cmpi	$acc1.m,#0xdead
	jeq		task_terminate
	
wait_commands:
	jmp		recv_cmd
	
sys_commands:
	lrs		$acc1.m,@CMBL
	
	cmpi	$acc1.m,#0x0001
	jeq		run_nexttask
	
	cmpi	$acc1.m,#0x0002
	jeq		0x8000
	halt
	
run_nexttask:
	s16
	clr			$acc0
	clr			$acc1
	call		wait_mail_acc0
	lrs			$acc0.l,@CMBL
	call		wait_mail_acc1
	lrs			$acc1.l,@CMBL
	call		wait_mail_acc1
	lrs			$acc1.m,@CMBL
	srs			@DMAMMEMH,$acc0.m
	srs			@DMAMMEML,$acc0.l
	si			@DMACR,#DMA_TO_CPU
	srs			@DMADSPM,$acc1.m
	srs			@DMABLEN,$acc1.l
	clr			$acc0
	clr			$acc1
	call		wait_mail_acc0
	lrs			$acc0.l,@CMBL
	mrr			$ix0,$acc0.m
	mrr			$ix1,$acc0.l
	call		wait_mail_acc1
	lrs			$acc1.l,@CMBL
	call		wait_mail_acc1
	lrs			$acc1.m,@CMBL
	mrr			$ix2,$acc1.m
	mrr			$ix3,$acc1.l
	clr			$acc0
	call		wait_mail_acc0
	lrs			$acc0.m,@CMBL
	mrr			$ar0,$acc0.m
	clr			$acc1
	call		wait_mail_acc1
	lrs			$acx0.l,@CMBL
	mrr			$acx0.h,$acc1.m
	call		wait_mail_acc0
	lrs			$acx1.l,@CMBL
	call		wait_mail_acc0
	lrs			$acx1.h,@CMBL
wait_dmem_dma:
	lrs			$acc0.m,@DMACR
	andf		$acc0.m,#0x0004
	jlnz		wait_dmem_dma
	jmp			0x80b5		
	halt

task_terminate:
	si		@DMBH,#0xdcd1
	si		@DMBL,#0x0003
	si		@DIRQ,#0x01
	jmp		recv_cmd

send_samples:
	lri		$ar0,#MEM_SND_BUF
	lris	$acx1.l,#DMA_TO_CPU
	lri		$acx0.l,#NUM_SAMPLES*4
	lr		$acc0.m,@OUTBUF_ADDRH
	lr		$acc0.l,@OUTBUF_ADDRL
	call	dma_data
	
	si		@DMBH,#0xdcd1
	si		@DMBL,#0x0004
	si		@DIRQ,#0x01
	
	jmp		recv_cmd

get_pb_address:
	call	wait_mail_recv
	
	lri		$ar2,#PB_MADDRH
	
	lrs		$acc0.m,@CMBH
	srri	@$ar2,$acc0.m
	
	lrs		$acc0.m,@CMBL
	srr		@$ar2,$acc0.m

	jmp		recv_cmd
	
process_next_voice:
	si		@DMACR,#DMA_TO_DSP
	call	dma_pb_block
	
	jmp		dsp_mixer

process_first_voice:
	si		@DMACR,#DMA_TO_DSP
	call	dma_pb_block
	
	lri		$ar0,#MEM_SND_BUF
	lri		$acc1.l,#0
	
	lri		$acx0.l,#NUM_SAMPLES*2
	loop	$acx0.l
	srri	@$ar0,$acc1.l

dsp_mixer:
	clr		$acc0

	lr		$acc1.m,@FLAGS_SMPL
	andcf	$acc1.m,#VOICE_FLAGL_PAUSE
	jlz		finish_voice
	
	lr		$acc1.m,@FLAGS_SMPH
	andcf	$acc1.m,#VOICE_FLAGH_RUN
	jlnz	finish_voice
	
	lri		$ar1,#SNDBUF_SADDRH
	lrri	$acc0.m,@$ar1
	lrrd	$acc0.l,@$ar1
	
	tst		$acc0
	jeq		finish_voice

no_change_buffer:
	lr		$acc1.l,@FLAGS_SMPL
	
	mrr		$acc1.m,$acc1.l
	andi	$acc1.m,#0x02
	addi	$acc1.m,#select_format
	mrr		$ar3,$acc1.m
	ilrri	$acc0.m,@$ar3
	ilrri	$acc1.m,@$ar3
	call	setup_accl
	
	mrr		$acc1.m,$acc1.l
	andi	$acc1.m,#0x07
	addi	$acc1.m,#select_mixer
	mrr		$ar3,$acc1.m
	ilrr	$acc1.m,@$ar3
	mrr		$ar3,$acc1.m

	lri		$ar1,#SMP_L
	clr'l	$acc0 : $acx0.h,@$ar1		//left last sample
	lrri	$acx1.h,@$ar1		//right last sample
	lrri	$ix2,@$ar1
	lrri	$ix3,@$ar1
	lrri	$acc0.h,@$ar1
	lrri	$acc0.m,@$ar1
		
	lri		$ar0,#MEM_SND_BUF
	lri		$acx0.l,#NUM_SAMPLES

	lri		$ar1,#FREQ_SMPH
	lri		$wr1,#0x0004
		
	tst		$acc0
	jeq		no_delay
	
	lri		$ix0,#2

	bloop	$acx0.l,delay_loop
	decm	$acc0.m
	jeq		exit_delay
delay_loop:
	addarn	$ar0,$ix0

exit_delay:
	mrr		$acx0.l,$st3
	sr		@DELAY_SMPH,$acc0.h
	sr		@DELAY_SMPL,$acc0.m
	
no_delay:
	s40
	bloop	$acx0.l,dspmixer_loop_end
	
	// right/left channel sample mix
	lrri	$acc1.m,@$ar0		// right channel
	lrr		$acc0.m,@$ar0		// left channel
	addr'dr	$acc1.m,$acx1.h : $ar0
	addr's	$acc0.m,$acx0.h : @$ar0,$acc1.m
	srri	@$ar0,$acc0.m

	clr'l	$acc0 : $acx1.h,@$ar1
	lrri	$acx1.l,@$ar1
	lrr		$acc0.l,@$ar1


	addax	$acc0,$acx1
	mrr		$acx0.l,$acc0.m
	srri	@$ar1,$acc0.l
	cmpis	$acc0.m,#DEF_FREQ_INT
	jrge	$ar3

	jmp		no_mix
		
mono_unsigned_mix:
	bloop	$acx0.l,mono_unsigned_mix_end
	lrs		$acc0.m,@ACDAT			//right channel
mono_unsigned_mix_end:
	mrr		$acc1.m,$acc0.m			//left channel
	xori	$acc0.m,#0x8000
	xori	$acc1.m,#0x8000
	
	jmp		mix_samples

mono_mix:
	bloop	$acx0.l,mono_mix_end
	lrs		$acc0.m,@ACDAT			//right channel
mono_mix_end:
	mrr		$acc1.m,$acc0.m			//left channel

	jmp		mix_samples
	
stereo_unsigned_mix:
	bloop	$acx0.l,stereo_unsigned_mix_end
	lrs		$acc0.m,@ACDAT			//right channel
stereo_unsigned_mix_end:
	lrs		$acc1.m,@ACDAT			//left channel
	xori	$acc0.m,#0x8000
	xori	$acc1.m,#0x8000

	jmp		mix_samples
	
stereo_mix:
	bloop	$acx0.l,stereo_mix_end
	lrs		$acc0.m,@ACDAT			//right channel
stereo_mix_end:
	lrs		$acc1.m,@ACDAT			//left channel

mix_samples:
	// multiply samples*volume
	mrr		$acx0.h,$ix2
	mulc	$acc0.m,$acx0.h			//left channel
	mrr		$acx1.h,$ix3
	mulcmv	$acc1.m,$acx1.h,$acc0	//right channel
	asl		$acc0,#8
	movp's	$acc1 : @$ar1,$acc0.m
	asl		$acc1,#8
	srrd	@$ar1,$acc1.m
no_mix:
	lrri	$acx0.h,@$ar1
dspmixer_loop_end:
	lrri	$acx1.h,@$ar1
				
mixer_end:
	s16
	
	lri		$wr1,#0xffff
	
	lri		$ar1,#PDS
	lrs		$acc0.m,@ACPDS
	srrd	@$ar1,$acc0.m
	lrs		$acc0.m,@ACYN2
	srrd	@$ar1,$acc0.m
	lrs		$acc0.m,@ACYN1
	srrd	@$ar1,$acc0.m
	lrs		$acc0.m,@ACCAL
	srrd	@$ar1,$acc0.m
	lrs		$acc0.m,@ACCAH
	srrd	@$ar1,$acc0.m

finish_voice:
	lr		$acc0.m,@FLAGS_SMPH
	ori		$acc0.m,#VOICE_FLAGH_END
	sr		@FLAGS_SMPH,$acc0.m
	
	si		@DMACR,#DMA_TO_CPU
	call	dma_pb_block

	si		@DMBH,#0xdcd1
	si		@DMBL,#0x0004
	si		@DIRQ,#0x01

	jmp		recv_cmd
	
dma_pb_block:
	lr		$acc0.m,@PB_MADDRH
	lr		$acc0.l,@PB_MADDRL
	srs		@DMAMMEMH,$acc0.m
	srs		@DMAMMEML,$acc0.l
	si		@DMADSPM,#PB_ADDR
	si		@DMABLEN,#PB_STURCT_SIZE

wait_dma_pb:
	lrs		$acc0.m,@DMACR
	andf	$acc0.m,#0x04
	jlnz	wait_dma_pb
	ret

dma_data:
	srs		@DMAMMEMH,$acc0.m
	srs		@DMAMMEML,$acc0.l
	sr		@DMADSPM,$ar0
	sr		@DMACR,$acx1.l
	sr		@DMABLEN,$acx0.l
	
wait_dma:
	lrs		$acc0.m,@DMACR
	andf	$acc0.m,#0x04
	jlnz	wait_dma
	ret
		
//setup_accl: acc0.m = format, acc1.m = gain, ar1 = sndbuf_start
setup_accl:
	srs		@ACFMT,$acc0.m
	srs		@ACGAN,$acc1.m

	lrri	$acc0.m,@$ar1
	srs		@ACSAH,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACSAL,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACEAH,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACEAL,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACCAH,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACCAL,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACYN1,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACYN2,$acc0.m
	lrri	$acc0.m,@$ar1
	srs		@ACPDS,$acc0.m

	si		@ACCOEF+0,#0
	si		@ACCOEF+1,#0
	si		@ACCOEF+2,#0
	si		@ACCOEF+3,#0
	si		@ACCOEF+4,#0
	si		@ACCOEF+5,#0
	si		@ACCOEF+6,#0
	si		@ACCOEF+7,#0
	si		@ACCOEF+8,#0
	si		@ACCOEF+9,#0
	si		@ACCOEF+10,#0
	si		@ACCOEF+11,#0
	si		@ACCOEF+12,#0
	si		@ACCOEF+13,#0
	si		@ACCOEF+14,#0
	si		@ACCOEF+15,#0

	ret
	
wait_mail_sent:
	lrs		$acc0.m,@DMBH
	andf	$acc0.m,#0x8000
	jlnz	wait_mail_sent
	ret
	
wait_mail_recv:
	lrs		$acc1.m,@CMBH
	andcf	$acc1.m,#0x8000
	jlnz	wait_mail_recv
	ret
	
wait_mail_acc0:
	lrs		$acc0.m,@CMBH
	andcf	$acc0.m,#0x8000
	jlnz	wait_mail_acc0
	ret
	
wait_mail_acc1:
	lrs		$acc1.m,@CMBH
	andcf	$acc1.m,#0x8000
	jlnz	wait_mail_recv
	ret

/* exceptions */	
exception1:		// Stack overflow
	rti
	
exception2:
	rti
	
exception3:
	rti
	
exception4:
	rti

exception5:		// Accelerator address overflow
	s16
	mrr		$st1,$acc0.m

	lrs		$acc0.m,@ACYN1
	srs		@ACYN1,$acc0.m
	lrs		$acc0.m,@ACYN2
	srs		@ACYN2,$acc0.m
	lrs		$acc0.m,@ACPDS
	srs		@ACPDS,$acc0.m

	mrr		$acc0.m,$st1
	rti
	
exception6:
	rti
	
exception7:		// External interrupt (message from CPU)
	rti

select_mixer:
	cw		mono_mix
	cw		stereo_mix
	cw		mono_mix
	cw		stereo_mix
	cw		mono_unsigned_mix
	cw		stereo_unsigned_mix
	cw		mono_unsigned_mix
	cw		stereo_unsigned_mix

select_format:
	cw		ACCL_FMT_8BIT
	cw		ACCL_GAIN_8BIT
	cw		ACCL_FMT_16BIT
	cw		ACCL_GAIN_16BIT
	