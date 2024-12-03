/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

STATIC void prf_internal(const char *fmt, va_list adx, vnode_t *vp,
	int prt_where, int prt_type, char *sprintf_buf, int sbuf_len,
	int layer_flag);
STATIC void loutput(char c, struct tty *tp);

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * pretty much suspended. Printf should not be used for chit-chat.
 */
void
printf(char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	prf(fmt, adx, (vnode_t *)NULL, PRW_CONS | PRW_BUF, 0);
	va_end(adx);
}

void
dri_printf(char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	prf(fmt, adx, (vnode_t *)NULL, PRW_CONS | PRW_BUF, 0);
	va_end(adx);
}

void
lprintf(char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	prf(fmt, adx, (vnode_t *)NULL, PRW_CONS | PRW_BUF, 1);
	va_end(adx);
}

STATIC void
loutput(char c, struct tty *tp)
{
	if (prt_where & PRW_BUF) {
		putbuf[putbufndx++ % putbufsz] = c;
		wakeup(putbuf);
	}

	if (prt_where & PRW_CONS)
		putc(c, &tp->t_outq);
}

/*
 * layers_internal() handles lprintf().
 */
STATIC void
layers_internal()
{
	register		c;
	register uint		*adx;
	char			*s;
	register struct tty	*tp;
	register int		sr;	/* saved interrupt level */
	struct tty		*errlayer();

	tp = errlayer();	/* get tty pointer to error layer */
	sr = splhi();		/* ??? */
	if (cfreelist.c_next == NULL) { /* anywhere to buffer output? */
		splx(sr);		/* back to where we were */
		return;			/* nope, just return	*/
	}
	adx = (u_int *)x1;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			xtvtproc(tp, T_OUTPUT);	/* ??? */
			splx(sr);		/* back to where we were */
			return;
		}
		loutput(c, tp);
	}
	c = *fmt++;
	if (c == 'd' || c == 'u' || c == 'o' || c == 'x')
		lprintn((long)*adx, c=='o'? 8: (c=='x'? 16:10), tp);
	else if (c == 's') {
		s = (char *)*adx;
		while (c = *s++) {
			loutput(c, tp);
		}
	} else if (c == 'D') {
		lprintn(*(long *)adx, 10, tp);
		adx += (sizeof(long) / sizeof(int)) - 1;
	}
	adx++;
	goto loop;
}

void
prf(const char *fmt, va_list adx, vnode_t *vp, int prt_where,
	int layer_flag)
{
	prf_internal(fmt, adx, vp, prt_where, 0, (char *)NULL, 0,
		layer_flag);
}

/*
 * This function was copied directly from Solaris 2.6.
 * Modified to add support for virtual terminals.
 *
 * prt_where directs the output according to:
 *	PRW_STRING -- output to sprintf_buf
 * 		the caller is responsible for providing a large enough buffer
 *	PRW_BUF -- output to system buffer
 *	PRW_CONS -- output to console
 *
 * If PRW_STRING is on, PRW_CONS is not allowed simultaneously.
 *
 * prt_type is ultimately needed for calling strlog
 */
STATIC void
prf_internal(const char *fmt, va_list adx, vnode_t *vp, int prt_where,
	int prt_type, char *sprintf_buf, int sbuf_len, int layer_flag)
{
	int b, c, i;
	uint64_t ul;
	int64_t l;
	char *s;
	int any;
	char consbuf[LBUFSZ+1];	/* extra char to NULL terminate for strlog() */
	char *linebuf;	/* ptr to beginning of output buf */
	char *lbp;		/* current buffer pointer */
	int pad;
	int width;
	int ells;
	struct tty *tp;	/* tty handling - needed for layers */
	int sr;	/* saved interrupt level */

	/*
	 * If we are not printing to the console only, then or in
	 * the SL_CONSOLE flag so that strlog is called in writekmsg.
	 */
	if (prt_where != PRW_CONS)
		prt_type |= SL_CONSOLE;

	if (prt_where & PRW_STRING) {
		/*
		 * Make sure that PRW_CONS is off, both cannot work at the
		 * same time. Built in protection.
		 */
		prt_where &= ~PRW_CONS;
		lbp = sprintf_buf;
		linebuf = sprintf_buf;
	} else {
		lbp = consbuf;
		linebuf = consbuf;
	}

	/*
	 * Handle layer (virtual terminal) support.
	 */
	if (layer_flag) {
		tp = errlayer();
		sr = splhi();
		if (cfreelist.c_next == NULL) {
			splx(sr);
			return;
		}
	}

loop:
	while ((c = *fmt++) != '%') {
		lbp = PRINTC(c);
		if (c == '\0') {
			if (layer_flag) {
				xtvtproc(tp, T_OUTPUT);
				splx(sr);
			}
			return;
		}
		if (layer_flag)
			loutput(c, tp);
	}

	c = *fmt++;
	for (pad = ' '; c == '0'; c = *fmt++)
		pad = '0';

	for (width = 0; c >= '0' && c <= '9'; c = *fmt++)
		width = width * 10 + c - '0';

	for (ells = 0; c == 'l'; c = *fmt++)
		ells++;

	switch (c) {
	case 'd':
	case 'D':
		b = 10;
		if (ells == 0)
			l = (int64_t)va_arg(adx, int);
		else if (ells == 1)
			l = (int64_t)va_arg(adx, long);
		else
			l = (int64_t)va_arg(adx, int64_t);
		if (l < 0) {
			lbp = PRINTC('-');
			width--;
			ul = -l;
		} else {
			ul = l;
		}
		goto number;

	case 'p':
		ells = 1;
		/*FALLTHROUGH*/
	case 'x':
	case 'X':
		b = 16;
		goto u_number;

	case 'u':
		b = 10;
		goto u_number;

	case 'o':
	case 'O':
		b = 8;
u_number:
		if (ells == 0)
			ul = (uint64_t)va_arg(adx, u_int);
		else if (ells == 1)
			ul = (uint64_t)va_arg(adx, u_long);
		else
			ul = (uint64_t)va_arg(adx, uint64_t);
number:
		if (layer_flag)
			lbp = lprintn((uint64_t)ul, b, width, pad,
				lbp, linebuf, vp, prt_where, prt_type, sbuf_len, tp);
		else
			lbp = printn((uint64_t)ul, b, width, pad,
				lbp, linebuf, vp, prt_where, prt_type, sbuf_len);
		break;

	case 'c':
		b = va_arg(adx, int);
		for (i = 24; i >= 0; i -= 8)
			if ((c = ((b >> i) & 0x7f)) != 0) {
				if (c == '\n')
					lbp = PRINTC('\r');
				lbp = PRINTC(c);
			}
		break;

	case 'b':
		b = va_arg(adx, int);
		s = va_arg(adx, char *);
		lbp = printn((uint64_t)(unsigned)b, *s++, width, pad,
		    lbp, linebuf, vp, prt_where, prt_type, sbuf_len);
		any = 0;
		if (b) {
			while ((i = *s++) != 0) {
				if (b & (1 << (i-1))) {
					lbp = PRINTC(any? ',' : '<');
					any = 1;
					for (; (c = *s) > 32; s++)
						lbp = PRINTC(c);
				} else
					for (; *s > 32; s++)
						;
			}
			if (any)
				lbp = PRINTC('>');
		}
		break;

	case 's':
		s = va_arg(adx, char *);
		if (!s) {
			/* null string, be polite about it */
			s = "<null string>";
		}
		while ((c = *s++) != 0) {
			if (c == '\n')
				lbp = PRINTC('\r');
			lbp = PRINTC(c);
		}
		break;

	case '%':
		lbp = PRINTC('%');
		break;
	}
	goto loop;
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
STATIC char *
printn(uint64_t n, int b, int width, int pad, char *lbp, char *linebuf,
	vnode_t *vp, int prt_where, int prt_type, int sbuf_len)
{
	char prbuf[22];	/* sufficient for a 64 bit octal value */
	char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
		width--;
	} while (n);
	while (width-- > 0)
		*cp++ = pad;
	do {
		lbp = PRINTC(*--cp);
	} while (cp > prbuf);
	return (lbp);
}

/*
 * This function is called by the ASSERT() macro in sys/debug.h
 * when an assertion failure occurrs.
 */
int
assfail(char *a, char *f, int l)
{
	cmn_err(CE_PANIC, "*** ASSERTION FAILED ! ***\n"
		"expr: %s\nfile: %s\nline: %d\n", a, f, l);
}
