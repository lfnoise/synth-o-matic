/*
 *  som_internal.h
 *
 *  Internal definitions of the Synth-O-Matic engine, derived from the
 *  original synth.h, rpnlang.h and funcdefs.h.
 *
 *  The original kept all of its state in globals.  To let several documents
 *  exist at once, that state now lives in a SOMContext, and the old global
 *  names are macros that refer to the context of the current thread.  This
 *  keeps the unit generator code essentially as it was written.
 */

#ifndef SOM_INTERNAL_H
#define SOM_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include "SOMEngine.h"

#define Case break;case
#define or :case

#define lobits 16
#define tablesize 512
#define tablebits   9
#define tablebits2 10
#define lomask  0x01FF
#define lomask2 0x03FF			/* for 1024 point table */
#define fracmask 0x0000FFFF
#define longmask 0x01FFFFFF
#define onecyc   0x02000000
#define onesamp  65536.0
#define onesamp1 1.52587890625e-5		/*  (1. / 65536.)  */

#define smin(a, b) (((a)<(b))?(a):(b))
#define smax(a, b) (((a)>(b))?(a):(b))
#define sclip(a, b, c) (((b)<(a))?(a):(((b)>(c))?(c):(b)))
#define sabs(a) (((a)<0)?-(a):(a))
#define SWAP(type, a, b) {type temp; temp = (a); (a) = (b); (b) = temp;}

#define BUFLEN 512
#define BUFLOG 9
#define MAXLEVEL 32
#define MAXCONSTS 64
#define PFRAC(p) (((p)&fracmask)*onesamp1)
#define VARNAMELEN 32
#define MAXJUMP 64
#define MAXPF 50
#define MAXGL 10
#define MAXOUT 4

/* constants for random number generator */
#define IM  714025
#define IA 1366
#define IC 150889

enum { /* result codes for data flow machine */
	dfUninitted,
	dfExecuted,
	dfInputStale,
	dfOutputFull,
	dfDone
};

enum { /* signal types */
	sigScalar,
	sigTable,
	sigEnvelope,
	sigSample
};

typedef unsigned char byte;
typedef unsigned short word;

typedef struct {
	int type;
	int bufnum;	/* -1 if permanent */
	int varid;
	double out[BUFLEN+1];
} SignalBuf, *SignalBufPtr;

typedef struct {
	char name[VARNAMELEN];
	int type;
	float lolim, hilim;				/* upper & lower limits when plotting */
	float stepsiz;					/* step size for slider and randomization */
	char select;
	char assigned;
	char referenced;
	char randomset;
	char pfield;					/* score card pfield number */
	char input;
	char output;
	long length;
	float *ac;
	double dc;
	SignalBufPtr buf;
	void *data;			/* for envelope gen rec */
} Variable;

typedef struct {
	char assigned[MAXJUMP];
	char referenced[MAXJUMP];
} RefCount, *RefCountPtr;

typedef struct ugen {
	int code;
	int oper;
	int srcline;			/* code line number */
	void (*func)(struct ugen *);	/* ugen function */
	char state;
	int type;
	double *out;			/* output signal buffer */
	SignalBufPtr in[16];
	SignalBufPtr outbuf;
	void *data;				/* ptr to ugen specific data */
	long time;				/* for testing performance */
} UGen;

typedef struct {
	int index;
	char *name;
	void (*execute)(UGen *);
	int numoperands;
	char selfserve;
} Opcode;

typedef struct {
	char *name;
	void (*execute)(UGen *);
	int numoperands;
	int asgnmask;
} Function;

enum {
	opDrop,
	opOver,
	opAssign,
	opAssignVec,
	opPushCon,
	opPushVar,
	opPushVec,
	opFuncCall,
	opAdd,
	opSubtract,
	opMultiply,
	opDivide,
	opExpon,
	opInvert,
	opNot,
	opGreater,
	opLess,
	opGreaterEq,
	opLessEq,
	opEqual,
	opNotEqual,
	opAnd,
	opOr,
	opConditional,
	opIf,
	opElse,
	opEndIf,
	opEndIfElse,
	opRevSubtract,	/* port: in[1] - in[0], for -= */
	opRevDivide,	/* port: in[1] / in[0], for /= */
	opRevExpon,		/* port: in[1] ^ in[0], for ^= */
	opEnd,

	opNumOps
};

extern Opcode codes[opNumOps];
extern Function funcs[];
extern int fnNumFns;

/* ---------------------------------------------------------------------- */
/* per document state                                                     */

struct SOMContext {
	/* globals */
	double x_sampleRate, x_sampleDur, x_tempo;
	long x_sampleLength, x_bufferedLength, x_delayLength;
	char x_infilename[32], x_outfilename[32];
	word x_fileVersion;

	/* sample rate dependent constants */
	double x_cps2rad, x_rad2inc, x_cps2inc, x_cps2inc2, x_rad2inc2;
	double x_rad2lfinc, x_cps2lfinc, x_rad2lfinc2, x_cps2lfinc2;
	double x_pisr, x_pi2sr, x_nyqlim;
	unsigned long x_randnum;

	/* variables */
	Variable *x_vars;
	int *x_varx;		/* sorted index of variables */
	int x_numvars, x_maxvars;
	Variable *x_pfields[MAXPF];
	Variable *x_globals[MAXGL];
	Variable *x_outputs[MAXOUT];
	Variable *x_inputs[MAXOUT];

	/* program */
	char *x_text;
	UGen *x_U;
	int x_compileptr, x_progsize, x_maxprogsize, x_numdone;
	float x_constval[MAXCONSTS];
	int x_numconsts;
	int x_errorline;

	/* compiler */
	int *x_operandstack;
	int x_opstackptr, x_maxopstack;
	int x_jumpstack[MAXJUMP];
	int x_jumpptr;
	RefCountPtr x_ref;
	int x_funcid[MAXLEVEL], x_funclevel;
	int x_numargs[MAXLEVEL];
	int x_varsid[MAXLEVEL], x_varlevel;
	int x_constid[MAXLEVEL], x_constlevel;

	/* lexer */
	char x_yytext[256];
	char x_prevtext[256];	/* the token before, for error messages */
	int x_yylen, x_lineno, x_textlen, x_textpos;

	/* interpreter */
	long x_numout, x_nextpc;
	SignalBufPtr *x_sigstack;
	int x_sigstackptr, x_maxsigstack;
	SignalBufPtr *x_buflist;
	int x_numbufs, *x_freebuflist, x_numfree;

	jmp_buf x_allocerr;
	int x_allocerr_armed;

	SOMMessageFn msgfn;
	void *msguser;
};

extern _Thread_local SOMContext *som_cur;

/* enter the context of a document; every public entry point does this */
#define SOM_ENTER(ctx) (som_cur = (ctx))

#define gSampleRate		(som_cur->x_sampleRate)
#define gSampleDur		(som_cur->x_sampleDur)
#define gTempo			(som_cur->x_tempo)
#define gSampleLength	(som_cur->x_sampleLength)
#define gBufferedLength	(som_cur->x_bufferedLength)
#define gDelayLength	(som_cur->x_delayLength)
#define gInfilename		(som_cur->x_infilename)
#define gOutfilename	(som_cur->x_outfilename)
#define gFileVersion	(som_cur->x_fileVersion)
#define gUseDisk		0

#define cps2rad			(som_cur->x_cps2rad)
#define rad2inc			(som_cur->x_rad2inc)
#define cps2inc			(som_cur->x_cps2inc)
#define rad2inc2		(som_cur->x_rad2inc2)
#define cps2inc2		(som_cur->x_cps2inc2)
#define rad2lfinc		(som_cur->x_rad2lfinc)
#define cps2lfinc		(som_cur->x_cps2lfinc)
#define rad2lfinc2		(som_cur->x_rad2lfinc2)
#define cps2lfinc2		(som_cur->x_cps2lfinc2)
#define pisr			(som_cur->x_pisr)
#define pi2sr			(som_cur->x_pi2sr)
#define nyqlim			(som_cur->x_nyqlim)
#define randnum			(som_cur->x_randnum)

#define vars			(som_cur->x_vars)
#define varx			(som_cur->x_varx)
#define numvars			(som_cur->x_numvars)
#define maxvars			(som_cur->x_maxvars)
#define pfields			(som_cur->x_pfields)
#define globals			(som_cur->x_globals)
#define outputs			(som_cur->x_outputs)
#define inputs			(som_cur->x_inputs)

#define U				(som_cur->x_U)
#define compileptr		(som_cur->x_compileptr)
#define progsize		(som_cur->x_progsize)
#define maxprogsize		(som_cur->x_maxprogsize)
#define constval		(som_cur->x_constval)
#define numconsts		(som_cur->x_numconsts)

#define operandstack	(som_cur->x_operandstack)
#define opstackptr		(som_cur->x_opstackptr)
#define maxopstack		(som_cur->x_maxopstack)
#define jumpstack		(som_cur->x_jumpstack)
#define jumpptr			(som_cur->x_jumpptr)
#define ref				(som_cur->x_ref)
#define funcid			(som_cur->x_funcid)
#define funclevel		(som_cur->x_funclevel)
#define numargs			(som_cur->x_numargs)
#define varsid			(som_cur->x_varsid)
#define varlevel		(som_cur->x_varlevel)
#define constid			(som_cur->x_constid)
#define constlevel		(som_cur->x_constlevel)

#define yytext			(som_cur->x_yytext)
#define yylen			(som_cur->x_yylen)
#define lineno			(som_cur->x_lineno)

#define numout			(som_cur->x_numout)
#define nextpc			(som_cur->x_nextpc)
#define allocerr		(som_cur->x_allocerr)

/* tables shared by every document, built once */
extern double *sine;
extern double *pmsine;
extern float *env0;
extern double sqrt2, twopi, pi, pi2, pi4;
extern double randfac, randfac1;
extern double log001, log01, log1;
void som_init_tables(void);

/* som_context.c */
/* the original's message window printf; renamed to stay clear of POSIX dprintf */
#define dprintf som_printf
void som_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void som_reset_for_read(SOMContext *ctx);
void *allocate(size_t);
void *reallocate(void *, size_t);
void InitSignals(void);
void FreeSignals(void);
Variable *DefineVar(const char *);
void VarSetType(Variable *v, int type);
void VarSetLength(Variable *v, long len);
void FixSignalWindow(void);
void DefaultPField(Variable *v);
void FindLimits(Variable *v, double *minval, double *maxval, double *stepsiz);
void FindExactLimits(Variable *v, double *minval, double *maxval);
void AutoScale(Variable *v);
void SetGlobalLengths(double dur);

/* som_compile.c */
int  compile2(int op, int id);
int  som_compile_program(void);
void FixPFields(void);

/* som_lexer.c */
void initlexer(void);
int  yylex(void *lvalp);
void yyerror(const char *str);
int  yyparse(void);

/* som_interp.c */
SignalBufPtr NewBuffer(void);
SignalBufPtr popsig(void);
SignalBufPtr pushsig(void);
void pushcopy(SignalBufPtr s);
void ReVarsToBufs(void);
int  runit(SOMProgressFn progress, void *user);

/* vosim.c */
double xrand(void);
double yrand(void);

/* som_tables.c */
void four1(double *data, int nn, int isign);
void magphase(double *data, int n, int isign);

/* unit generators */
#define echo som_echo		/* clear of curses' echo(), which CoreFoundation can bring in */
void	noop(UGen *);
void	overfn(UGen *);
void	pushcon(UGen *);
void	pushvar(UGen *);
void	pushvec(UGen *);
void	assignvec(UGen *);

void	addfn(UGen *);
void	subfn(UGen *);
void	mulfn(UGen *);
void	divfn(UGen *);
void	exponfn(UGen *);
void	invertfn(UGen *);
void	notfn(UGen *);
void	greaterfn(UGen *);
void	lessfn(UGen *);
void	greatereqfn(UGen *);
void	lesseqfn(UGen *);
void	eqfn(UGen *);
void	noteqfn(UGen *);
void	andfn(UGen *);
void	orfn(UGen *);
void	condfn(UGen *);
void	assignfn(UGen *);
void	rsubfn(UGen *);
void	rdivfn(UGen *);
void	rexponfn(UGen *);

void	absfn(UGen *);
void	integfn(UGen *);
void	difffn(UGen *);
void	sumfn(UGen *);
void	avgfn(UGen *);
void	trigfn(UGen *);
void	sgnfn(UGen *);
void	randfn(UGen *);
void	rand2fn(UGen *);
void	sah(UGen *);
void	gate(UGen *);
void	intfn(UGen *);
void	fracfn(UGen *);
void	clipfn(UGen *);
void	minfn(UGen *);
void	maxfn(UGen *);
void	sinfn(UGen *);
void	cosfn(UGen *);
void	tanfn(UGen *);
void	logfn(UGen *);
void	log10fn(UGen *);
void	sqrtfn(UGen *);
void	expfn(UGen *);
void	midicps(UGen *);
void	cpsmidi(UGen *);
void	dbamp(UGen *);
void	ampdb(UGen *);
void	flip(UGen *);
void	flipsum(UGen *);

void	sinosc(UGen *);
void	oscil(UGen *);
void	oscili(UGen *);
void	poscil(UGen *);
void	poscili(UGen *);
void	pmoscil(UGen *);
void	xcoscil(UGen *);
void	coscil(UGen *);
void	coscili(UGen *);
void	coscil2(UGen *);
void	coscili2(UGen *);
void	soscil(UGen *);
void	woscil(UGen *);
void	wcoscil(UGen *);
void	lfoa(UGen *);
void	sinlfoa(UGen *);
void	oscil1a(UGen *);
void	bpenv(UGen *);
void	bpenv2(UGen *);
void	noise1(UGen *);
void	noise0(UGen *);
void	noise2(UGen *);
void	buzz(UGen *);
void	pluck(UGen *);
void	vosim(UGen *);
void	wavelets(UGen *);
void	grains(UGen *);
void	ksstring(UGen *);
void	kspipe(UGen *);
void	saw(UGen *);
void	square(UGen *);
void	square0(UGen *);
void	triang(UGen *);
void	blip(UGen *);
void	pulse(UGen *);
void	pulse0(UGen *);
void	ramp(UGen *);
void	linen(UGen *);
void	linseg(UGen *);
void	linseg0(UGen *);
void	adsr(UGen *);
void	ar(UGen *);
void	dopfreq(UGen *);
void	dopamp(UGen *);

void	lpf(UGen *);
void	hpf(UGen *);
void	bpf(UGen *);
void	brf(UGen *);
void	reson(UGen *);
void	areson(UGen *);
void	tone(UGen *);
void	atone(UGen *);
void	lag(UGen *);
void	lag2(UGen *);
void	rms(UGen *);
void	gain(UGen *);
void	balance(UGen *);
void	shaper(UGen *);
void	tablei(UGen *);

void	flange(UGen *);
void	flangei(UGen *);
void	delay(UGen *);
void	delayi(UGen *);
void	delay1(UGen *);
void	comb(UGen *);
void	echo(UGen *);
void	echoi(UGen *);
void	lpcomb(UGen *);
void	allpass(UGen *);
void	reverb(UGen *);
void	reverb2(UGen *);
void	multitap(UGen *);
void	pan2(UGen *);
void	mixpan2(UGen *);
void	xfade2(UGen *);

void	pitchshift(UGen *);
void	vocoder(UGen *);

void	envfollow(UGen *);
void	pchfollow(UGen *);

void	iffn(UGen *);
void	elsefn(UGen *);
void	endiffn(UGen *);
void	endifelse(UGen *);

#endif
