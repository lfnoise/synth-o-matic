/*
 *  som_interp.c
 *
 *  The block interpreter, from the original interpreter.c (runit and the
 *  signal buffer pool) and Assign.c (variable access opcodes).
 *
 *  Every ugen produces BUFLEN samples per pass.  Ugens whose output turns out
 *  to be a scalar or a wavetable are computed once and then reused.
 */

#include <time.h>
#include "som_internal.h"

#define sigstack		(som_cur->x_sigstack)
#define sigstackptr		(som_cur->x_sigstackptr)
#define maxsigstack		(som_cur->x_maxsigstack)
#define buflist			(som_cur->x_buflist)
#define numbufs			(som_cur->x_numbufs)
#define freebuflist		(som_cur->x_freebuflist)
#define numfree			(som_cur->x_numfree)
#define numdone			(som_cur->x_numdone)

static void VarsToBufs(void);
static void BufsToVars(void);

/* ---------------------------------------------------------------------- */
/* maintain a list of buffers to minimize allocations                     */

#define BUFINC 10

static void
MoreBufSpace(void) {
	int i, j;

	i = numbufs;
	numbufs += BUFINC;
	if (buflist) {
		buflist = (SignalBufPtr *)reallocate(buflist, (long)numbufs * sizeof(SignalBufPtr));
		freebuflist = (int *)reallocate(freebuflist, (long)numbufs * sizeof(int));
	} else {
		buflist = (SignalBufPtr *)allocate((long)numbufs * sizeof(SignalBufPtr));
		freebuflist = (int *)allocate((long)numbufs * sizeof(int));
	}
	for (; i<numbufs; ++i) {
		buflist[i] = (SignalBufPtr)allocate(sizeof(SignalBuf));
		buflist[i]->type = sigSample;
		buflist[i]->bufnum = i;	/* index of buffer */
		buflist[i]->varid = -1;
	}

	i = numfree;
	numfree += BUFINC;
	for (j = numbufs - BUFINC; i<numfree; ++i, ++j) {
		freebuflist[i] = j;
	}
}

static void
InitBufList(void) {
	int i;
	numbufs = 0;
	numfree = 0;
	buflist = NULL;
	freebuflist = NULL;
	MoreBufSpace();
	for (i=0; i<numvars; ++i) {
		vars[i].buf = NULL;
	}
	maxsigstack = 100;
	sigstack = (SignalBufPtr *)allocate(maxsigstack*sizeof(SignalBufPtr));
	sigstackptr = 0;
}

static void
KillBufList(void) {
	int i;
	for (i=0; i<numbufs; ++i) {
		free(buflist[i]);
	}
	free(buflist);
	free(freebuflist);
	free(sigstack);
	buflist = NULL;
	freebuflist = NULL;
	sigstack = NULL;
	numbufs = 0;
	numfree = 0;
	for (i=0; i<numvars; ++i) {
		vars[i].buf = NULL;
	}
}

SignalBufPtr
NewBuffer(void) {
	register int i;

	if (numfree == 0) MoreBufSpace();
	i = freebuflist[--numfree];
	buflist[i]->type = sigSample;
	buflist[i]->varid = -1;
	return buflist[i];
}

static void
FreeBuffer(SignalBufPtr buf) {
	if (buf->varid < 0) {
		freebuflist[numfree++] = buf->bufnum;
		buf->varid = -99;
	}
}

static void
growsigstack(void) {
	if (sigstackptr >= maxsigstack) {
		maxsigstack += 100;
		sigstack = (SignalBufPtr *)reallocate(sigstack, maxsigstack*sizeof(SignalBufPtr));
	}
}

SignalBufPtr
popsig(void) {
	if (sigstackptr>0) {
		return sigstack[--sigstackptr];
	} else {
		dprintf("Stack Empty\r");
		longjmp(allocerr, 2);	/* port: the original went on with a NULL buffer */
		return NULL;
	}
}

SignalBufPtr
pushsig(void) {
	SignalBufPtr s;

	s = NewBuffer();
	growsigstack();
	return sigstack[sigstackptr++] = s;
}

void
pushcopy(SignalBufPtr s) {
	growsigstack();
	sigstack[sigstackptr++] = s;
}

/* ---------------------------------------------------------------------- */

static void
SetPFields(void) {
	if (pfields[3]) pfields[3]->dc = gSampleDur;
	if (globals[0]) globals[0]->dc = gSampleRate;
	if (globals[1]) globals[1]->dc = gSampleRate/2.;
	if (globals[2]) globals[2]->dc = gTempo;
}

static void
SetupUGens(void) {
	int i;
	UGen *u;

	cps2rad = twopi / gSampleRate;
	cps2inc = (1L<<(lobits+tablebits)) / gSampleRate;
	rad2inc = (1L<<(lobits+tablebits)) / twopi;
	cps2inc2 = (1L<<(lobits+tablebits2)) / gSampleRate;
	rad2inc2 = (1L<<(lobits+tablebits2)) / twopi;
	cps2lfinc = cps2inc * 32.0;
	rad2lfinc = rad2inc * 32.0;
	cps2lfinc2 = cps2inc2 * 32.0;
	rad2lfinc2 = rad2inc2 * 32.0;
	nyqlim = gSampleRate/2. - .001;
	pisr = pi / gSampleRate;
	pi2sr = twopi / gSampleRate;
	for (i=0,u=U; i<progsize; ++i,++u) {
		u->type = sigSample;		/* default type */
		u->state = dfUninitted;
		u->data = NULL;
		u->outbuf = NULL;
		u->time = 0;
	}
	for (i=0; i<numvars; ++i) {
		vars[i].data = NULL;
	}
}

static void
CleanupUGens(void) {
	int i;
	UGen *u;

	u = U;
	for (i=0; i<progsize; ++i, ++u) {
		if (u->data) {
			free(u->data);
			u->data = NULL;
		}
	}
	for (i=0; i<numvars; ++i) {
		if (vars[i].data) {
			free(vars[i].data);
			vars[i].data = NULL;
		}
	}
}

static double
seconds(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int
runit(SOMProgressFn progress, void *user) {
	UGen *u, *end;
	int numin;
	long i, imax;
	int j;
	double t0;
	int alerted = 0, errnum, result = SOMCalcOK;
	int trace = getenv("SOM_TRACE") != NULL;	/* the original's EXECUTEBUG */

	if (progsize == 0) {
		dprintf("Program has not been compiled successfully.\r");
		return SOMCalcError;
	}
	t0 = seconds();
	SetPFields();
	SetupUGens();
	InitBufList();
	u = U;
	end = U + progsize;
	numdone = 0;
	numout = 0;
	imax = gBufferedLength>>BUFLOG;
	som_cur->x_allocerr_armed = 1;
	if ((errnum = setjmp(allocerr))) {
		if (errnum == 1) {
			dprintf("Not Enough Memory\r");
		} else {
			dprintf("Execution Error\r");
		}
		result = SOMCalcError;
		goto cleanup;
	}
	for (i=0; i<imax; ++i,numout+=BUFLEN) {
		imax = gBufferedLength>>BUFLOG;
		if (progress && (*progress)(user, i, imax)) {
			result = SOMCalcStopped;
			dprintf("Stopped.\r");
			break;
		}
		VarsToBufs();
		for (u=U; u<end;u=U+nextpc) {
			nextpc = (u-U)+1;
			if (u->code == opFuncCall) numin = funcs[u->oper].numoperands;
			else numin = codes[u->code].numoperands;
			for (j=numin-1; j>=0; --j) {
				u->in[j] = popsig();
			}
			if (codes[u->code].selfserve) {
				(*u->func)(u);
				if (u->state != dfDone) {
					if (u->type == sigTable || u->type == sigScalar) {
						u->state = dfDone;
						numdone ++;
					} else u->state = dfExecuted;
				}
			} else {
				if (u->state != dfDone) {
					u->outbuf = pushsig();
					u->out = u->outbuf->out;
					(*u->func)(u);
					if (u->type == sigTable || u->type == sigScalar) {
						u->state = dfDone;
						numdone ++;
					} else u->state = dfExecuted;
				} else {
					pushcopy(u->outbuf);
				}
			}
			if (u->outbuf) u->outbuf->type = u->type;
			if (trace && i < 2) {
				dprintf("%ld %3ld %-10s %-10s in:", i, (long)(u-U), codes[u->code].name,
					u->code == opFuncCall ? funcs[u->oper].name : "");
				for (j=0; j<numin; ++j) dprintf(" %d", u->in[j] ? u->in[j]->bufnum : -1);
				if (u->outbuf) dprintf("  out: %d type %d [%g %g]", u->outbuf->bufnum, u->outbuf->type,
					u->outbuf->out[0], u->outbuf->out[100]);
				dprintf("\n");
			}
			for (j=0; j<numin; ++j) {
				if (u->in[j]!= NULL && u->in[j]->type != sigScalar && u->in[j]->type != sigTable) {
					FreeBuffer(u->in[j]);
				}
			}
			if (numfree > numbufs) {
				dprintf("Multiply Freed blocks !!\r");
				numdone = progsize; break;
			}
			if (numbufs > 200 && !alerted) {
				dprintf("Zillions of blocks !!\r");
				alerted = 1;
			}
		}
		BufsToVars();
		if (numdone >= progsize) break;
	}
	if (progress && result == SOMCalcOK) (*progress)(user, imax, imax);
	t0 = seconds() - t0;
	if (t0 <= 0.) t0 = 1e-6;
	dprintf("%ld samples in %.2f seconds  =  %.0f smp/sec   (%.1fx real time)\r",
		gBufferedLength, t0, gBufferedLength/t0, (gBufferedLength/t0)/gSampleRate);
	cleanup:
	som_cur->x_allocerr_armed = 0;
	CleanupUGens();
	KillBufList();
	return result;
}

int
som_calculate(SOMContext *ctx, SOMProgressFn progress, void *user) {
	SOM_ENTER(ctx);
	return runit(progress, user);
}

int
som_first_output_sample(SOMContext *ctx) {
	int i;
	SOM_ENTER(ctx);
	for (i=0; i<numvars; ++i) {
		if (vars[varx[i]].type == sigSample && vars[varx[i]].assigned) {
			return varx[i];
		}
	}
	return -1;
}

/* ---------------------------------------------------------------------- */
/* opcodes from Assign.c                                                  */

void
noop(UGen *this)
{
	/* this is a no op */
	this->type = sigScalar;
	this->outbuf = NULL;
}

typedef struct {
	int val;
} IfRec, *IfRecPtr;

void
iffn(UGen *this)
{
	IfRecPtr ifrec;

	this->type = sigScalar;
	this->outbuf = NULL;
	if (this->state == dfUninitted) {
		this->data = ifrec = (IfRecPtr)allocate(sizeof(IfRec));
		ifrec->val = this->in[0]->out[0];
	} else ifrec = (IfRecPtr)this->data;
	if (ifrec->val == 0) nextpc = this->oper;
}

void
elsefn(UGen *this)
{
	this->type = sigScalar;
	this->outbuf = NULL;
	nextpc = this->oper;
}

void endiffn(UGen *this) { this->type = sigScalar; }

/* port: the operators of a -= b, a /= b and a ^= b take their operands the
   other way around, since b is computed before a is pushed */
void rsubfn(UGen *this) { SWAP(SignalBufPtr, this->in[0], this->in[1]); subfn(this); }
void rdivfn(UGen *this) { SWAP(SignalBufPtr, this->in[0], this->in[1]); divfn(this); }
void rexponfn(UGen *this) { SWAP(SignalBufPtr, this->in[0], this->in[1]); exponfn(this); }
void endifelse(UGen *this) { this->type = sigScalar; }

void
assignfn(UGen *this)
{
	register double *s1;
	register double *outi;
	register long nsmps;
	Variable *v;

	this->outbuf = NULL;
	pushcopy(this->in[0]);
	this->type = this->in[0]->type;
	if (this->state != dfDone) {
		v = vars + this->oper;
		if (!v->buf) {
			v->buf = NewBuffer();
			v->buf->type = this->in[0]->type;
			v->buf->varid = this->oper;
		}

		if (v->buf != this->in[0]) {
			if (this->in[0]->varid >= 0) {
				s1 = this->in[0]->out;
				outi = v->buf->out;
				nsmps = BUFLEN;
				do {
					*outi++ = *s1++;
				} while (--nsmps);
				this->outbuf = v->buf;
			} else {
				/* swap bufs */
				SWAP(SignalBufPtr, this->in[0], v->buf);
				/* swap header fields back */
				SWAP(int, this->in[0]->type, v->buf->type);
				SWAP(int, this->in[0]->varid, v->buf->varid);
				this->outbuf = v->buf;
			}
		}
		if (v->pfield == 3 && numout == 0) {	/* set duration */
			SetGlobalLengths(v->buf->out[0]);
			ReVarsToBufs();
		}
	}
}

void
pushcon(UGen *this)
{
	register double *outi, value;
	register long nsmps;

	this->type = sigScalar;
	this->state = dfExecuted;

	value = constval[this->oper];
	outi = this->out;
	nsmps = BUFLEN>>4;
	do {
		*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
		*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
		*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
		*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
	} while (--nsmps);
}

void
pushvar(UGen *this)
{
	Variable *v;

	v = vars + this->oper;
	if (!v->buf) {		/* port: an assigned-only variable read before it was set */
		v->buf = NewBuffer();
		v->buf->type = v->type;
		v->buf->varid = this->oper;
		memset(v->buf->out, 0, sizeof(v->buf->out));
	}
	this->outbuf = v->buf;
	this->type = v->buf->type;
	pushcopy(v->buf);
}

void
pushvec(UGen *this)
{
	register double *outi, value;
	register double *s1, *buf;
	register long nsmps;
	Variable *v;

	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	v = vars + this->oper;
	s1 = this->in[0]->out;
	outi = this->out;
	if ((v->type == sigTable || v->type == sigEnvelope) && v->buf) {
		buf = v->buf->out;
		if (this->in[0]->type == sigScalar) {
			value = buf[511 & (int)(*s1)];
			nsmps = BUFLEN;
			do {
				*outi++ = value;
			} while (--nsmps);
		} else {
			nsmps = BUFLEN;
			do {
				*outi++ = buf[(BUFLEN-1) & (int)(*s1++)];
			} while (--nsmps);
		}
	} else if ((v->type == sigTable || v->type == sigEnvelope) && v->ac) {
		/* port: a table the program only assigns elements of has no
		   signal buffer; the original read through a NULL pointer here */
		nsmps = BUFLEN;
		do {
			*outi++ = v->ac[(BUFLEN-1) & (int)(*s1++)];
		} while (--nsmps);
	} else {
		memset(outi, 0, BUFLEN * sizeof(double));
	}
}

void
assignvec(UGen *this)
{
	register double *buf1, *s1, *s2;
	register float *buf2;
	register long nsmps;
	Variable *v;
	int index;

	if (this->state != dfDone) {
		if (this->state == dfUninitted) {
			if (this->in[1]->type == sigScalar) this->type = sigScalar;
			else if (this->in[1]->type == sigTable) this->type = sigTable;
		}
		v = vars + this->oper;
		if (v->type == sigTable || v->type == sigEnvelope) {
			s1 = this->in[1]->out;
			s2 = this->in[0]->out;
			this->outbuf = this->in[1];
			if (v->buf) buf1 = v->buf->out;
			else buf1 = NULL;
			buf2 = v->ac;
			if (this->in[1]->type == sigScalar) {
				index = 511 & (int)(*s2);
				if (buf1) buf1[index] = *s1;
				buf2[index] = *s1;
			} else {
				nsmps = BUFLEN;
				if (buf1) {
					do {
						index = 511 & (int)(*s2++);
						buf1[index] = *s1;
						buf2[index] = *s1++;
					} while (--nsmps);
				} else {
					do {
						index = 511 & (int)(*s2++);
						buf2[index] = *s1++;
					} while (--nsmps);
				}
			}
			if (v->type == sigTable) {
				if (buf1) buf1[BUFLEN] = buf1[0];	/* port: buf1 may be NULL */
				buf2[BUFLEN] = buf2[0];
			}
		} else this->outbuf = NULL;
	}
	pushcopy(this->in[1]);	this->in[1] = NULL;
}

void
overfn(UGen *this)
{
	register double *outi, *s1;
	register long nsmps;

	s1 = this->in[0]->out;
	this->type = this->in[0]->type;
	pushcopy(this->in[0]);	this->in[0] = NULL;
	pushcopy(this->in[1]);	this->in[1] = NULL;
	outi = (this->outbuf = pushsig())->out;
	nsmps = BUFLEN;
	do {
		*outi++ = *s1++;
	} while (--nsmps);
}

/* ---------------------------------------------------------------------- */
/* variables to and from signal buffers                                   */

typedef struct {
	long freq, phase, pphase;
	double level;
} Oscil1Rec, *Oscil1RecPtr;

/* plays an envelope variable's table over the whole duration, as oscil1 */
static void
EnvelopeToBuf(Variable *v, Oscil1RecPtr osc)
{
	register double *outi, value, slope;
	register float *s1;
	register long nsmps, phase, pphase, freq;
	double nxtlevel, frac, v1;
	int j;

	s1 = v->ac;
	outi = v->buf->out;
	phase = osc->phase;
	pphase = osc->pphase;
	freq  = osc->freq;
	value = nxtlevel = osc->level;

	nsmps = BUFLEN>>5;
	for (; nsmps && phase < onecyc; nsmps--) {
		value = nxtlevel;
		phase += freq;
		frac = PFRAC(phase);
		pphase = phase >> lobits;
		pphase = smin(pphase, BUFLEN-2);
		v1 = s1[pphase];
		pphase = smin(pphase+1, BUFLEN-2);
		nxtlevel = v1 + (s1[pphase] - v1) * frac;
		slope = (nxtlevel - value) * .03125;
		j = 4;
		do {
			*outi++ = value;	value += slope;		*outi++ = value;	value += slope;
			*outi++ = value;	value += slope;		*outi++ = value;	value += slope;
			*outi++ = value;	value += slope;		*outi++ = value;	value += slope;
			*outi++ = value;	value += slope;		*outi++ = value;	value += slope;
		} while (--j);
	}
	if (phase >= onecyc && nsmps) {
		for (nsmps <<= 5; nsmps; nsmps--) {	/* port: fill the rest of the block */
			*outi++ = s1[BUFLEN-1];
		}
	}
	osc->phase = phase;
	osc->pphase = pphase;
	osc->level = nxtlevel;
}

static void
VarsToBufs(void) {
	int i;
	register double *outi, value;
	register float *s1;
	register long nsmps;
	Variable *v;
	Oscil1RecPtr osc;

	for (i=0; i<numvars; ++i) {
		if (vars[i].referenced && vars[i].assigned != 2) {
			v = vars + i;
			if (!v->buf) {
				v->buf = NewBuffer();
				v->buf->type = v->type;
				v->buf->varid = i;
			}
			if (v->type == sigEnvelope) {
				/* in[0] is wavetable, in[1] is fm */

				if (v->data == NULL) {
					v->data = osc = (Oscil1RecPtr)allocate(sizeof(Oscil1Rec));
					osc->freq  = cps2lfinc / gSampleDur;
					osc->phase  = 0L;
					osc->pphase = 0L;
					osc->level = v->ac[0];
				} else {
					osc = (Oscil1RecPtr)v->data;
				}
				EnvelopeToBuf(v, osc);
			} else if (v->type == sigSample && !v->assigned) {
				/* port: a sample that the program only reads plays its
				   contents (e.g. an imported sound).  The original always
				   read zeros: "you must use soundin to read a sample". */
				long n = smin(BUFLEN, v->length - numout);
				outi = v->buf->out;
				for (nsmps=0; nsmps<n; ++nsmps) *outi++ = v->ac[numout + nsmps];
				for (; nsmps<BUFLEN; ++nsmps) *outi++ = 0.;
			} else if (numout == 0) {
				if (v->type == sigTable) {
					s1 = v->ac;
					outi = v->buf->out;
					nsmps = BUFLEN>>4;
					do {
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
					} while (--nsmps);
				} else if (v->type == sigScalar) {
					value = v->dc;
					outi = v->buf->out;
					nsmps = BUFLEN>>4;
					do {
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
					} while (--nsmps);
				} else if (v->type == sigSample) {
					/* you must use soundin to read a sample, else assumed to be 0.0 */
					outi = v->buf->out;
					nsmps = BUFLEN>>4;
					value = 0.;
					do {
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
						*outi++ = value;	*outi++ = value;	*outi++ = value;	*outi++ = value;
					} while (--nsmps);
				}
			}
		}
	}
}

static void
BufsToVars(void) {
	int i;
	register double *s1;
	register float *outi;
	register long nsmps;
	Variable *v;

	for (i=0; i<numvars; ++i) {
		if (vars[i].assigned) {
			v = vars + i;
			if (!v->buf) continue;		/* if true, something's wrong */
			if (v->type == sigSample) {
				s1 = v->buf->out;
				outi = v->ac + numout;
				if (numout+BUFLEN <= v->length) {
					if (numout+BUFLEN <= gSampleLength) {
						nsmps = BUFLEN>>4;
						do {
							*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
							*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
							*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
							*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						} while (--nsmps);
					} else {
						nsmps = BUFLEN + gSampleLength - gBufferedLength;
						while (nsmps-- > 0) *outi++ = *s1++;
						nsmps = gBufferedLength - gSampleLength;
						while (nsmps-- > 0) *outi++ = 0.;
					}
				}
			} else if (numout == 0) {
				s1 = v->buf->out;
				if (v->type == sigScalar) {
					v->dc = *s1;
				} else if (v->ac) {
					outi = v->ac;
					nsmps = BUFLEN>>4;
					do {
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
						*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;	*outi++ = *s1++;
					} while (--nsmps);
				}
			}
		}
	}
}

void
ReVarsToBufs(void) {
	int i;
	Variable *v;
	Oscil1RecPtr osc;

	for (i=0; i<numvars; ++i) {
		if (vars[i].referenced && vars[i].assigned != 2
			&& vars[i].type == sigEnvelope && vars[i].data && vars[i].buf) {
			/* in[0] is wavetable, in[1] is fm */

			v = vars + i;
			osc = (Oscil1RecPtr)v->data;
			osc->freq  = cps2lfinc / gSampleDur;
			osc->phase  = 0L;
			osc->pphase = 0L;
			osc->level = v->ac[0];
			EnvelopeToBuf(v, osc);
		}
	}
}
