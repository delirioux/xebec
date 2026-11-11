/*
 * Xebec - reset entry for the FM-1 app area.
 *
 * The second-stage loader maps the app at XIP 0x02000000 and enters it
 * 0x120 bytes in, past the app-area header. The first two words of the
 * entry follow the layout the loader expects (a jump over a return),
 * then we take over: interrupts off, our own stacks, and into C.
 */
	.section .text.start,"ax",@progbits
	.globl xb_start
xb_start:
	goto 1f
	rts
1:	cli
	sp = __xb_stack_top
	ssp = __xb_sys_stack_top
	call xb_crt_init
	call xb_main
	/* xb_main never returns; if it does, park here */
2:	goto 2b
