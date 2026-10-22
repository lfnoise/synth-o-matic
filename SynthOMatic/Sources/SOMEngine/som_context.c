/*
 *  som_context.c
 *
 *  Document state and variable management.  Derived from the original
 *  Signal.c (variables), ManyWind.c (tables, File > New), SetGlobals.c and
 *  interpreter.c (allocation).  The QuickDraw user interface code of those
 *  files became the Swift front end.
 */

#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "som_internal.h"

_Thread_local SOMContext *som_cur = NULL;

double *sine;
double *pmsine;
float *env0;
double sqrt2;
double twopi, pi, pi2, pi4;
double randfac, randfac1;
double log001, log01, log1;

static pthread_once_t tablesOnce = PTHREAD_ONCE_INIT;

static void
inittables(void) {
	register int i;
	register float *f;
	register double pinc, x;

	sine = (double*)calloc(1025, sizeof(double));
	pmsine = (double*)calloc(1025, sizeof(double));
	pi = 3.14159265358979323846;
	pi2 = pi/2.;
	pi4 = pi/4.;
	twopi = 2.0 * pi;
	log001 = log(0.001);
	log01  = log(0.01);
	log1   = log(0.1);
	pinc = twopi / 1024.0 * 1.0;
	for (i=0; i<=256; ++i) {
		sine[i] = sin(i * pinc);
		sine[512+i] = -sine[i];
		sine[512-i] = sine[i];
		sine[1024-i] = -sine[i];
	}
	sine[1024] = sine[0];
	randfac = 2.0/IM;
	randfac1 = 1.0/IM;
	sqrt2 = sqrt(2.0);
	x = (1L<<(lobits+tablebits2)) / twopi;		/* rad2inc2 */
	for (i=0; i<=256; ++i) {
		pmsine[i] = x * sine[i];
		pmsine[512+i] = -pmsine[i];
		pmsine[512-i] = pmsine[i];
		pmsine[1024-i] = -pmsine[i];
	}

	env0 = f = (float*)calloc(BUFLEN+1, sizeof(float));
	x = 1.0;
	for (i=0; i<BUFLEN+1; ++i) {
		*f++ = x;
		x *= 0.989228013194; /* halves every 64 samples (0.989228013194 ^ 64 = .5) */
	}
}

void
som_init_tables(void) {
	pthread_once(&tablesOnce, inittables);
}

/* ---------------------------------------------------------------------- */
/* messages and memory                                                    */

void
som_printf(const char *fmt, ...) {
	char str[1024], *s;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	for (s=str; *s; ++s) if (*s == '\r') *s = '\n';	/* the original used CR */
	if (som_cur && som_cur->msgfn) (*som_cur->msgfn)(som_cur->msguser, str);
	else fputs(str, stderr);
}

void *
allocate(size_t size) {
	void *ptr;
	ptr = calloc(1, size ? size : 1);
	if (ptr == NULL) {
		dprintf("alloc failed %ld\r", (long)size);
		if (som_cur && som_cur->x_allocerr_armed) longjmp(allocerr, 1);
		abort();
	}
	return ptr;
}

void *
reallocate(void *ptr, size_t size) {
	ptr = realloc(ptr, size ? size : 1);
	if (ptr == NULL) {
		dprintf("realloc failed %ld\r", (long)size);
		if (som_cur && som_cur->x_allocerr_armed) longjmp(allocerr, 1);
		abort();
	}
	return ptr;
}

/* ---------------------------------------------------------------------- */
/* lifetime                                                               */

SOMContext *
som_create(void) {
	SOMContext *ctx;

	som_init_tables();
	ctx = (SOMContext*)calloc(1, sizeof(SOMContext));
	SOM_ENTER(ctx);
	gSampleRate = 39063.;		/* 10Mhz/256, an EPS rate */
	gDelayLength = 40L*BUFLEN;
	gSampleLength = 160L*BUFLEN;
	gBufferedLength = 160L*BUFLEN;
	gSampleDur = 2.097125156798;
	gTempo = 120.0;
	gFileVersion = 70;
	randnum = (unsigned long)time(NULL) % IM;
	strcpy(gInfilename, "sndin");
	strcpy(gOutfilename, "sndout");
	ctx->x_text = strdup("");
	InitSignals();
	return ctx;
}

void
som_destroy(SOMContext *ctx) {
	if (!ctx) return;
	SOM_ENTER(ctx);
	FreeSignals();
	free(vars);
	free(varx);
	free(U);
	free(ctx->x_text);
	free(ctx);
	som_cur = NULL;
}

void
som_set_message_callback(SOMContext *ctx, SOMMessageFn fn, void *user) {
	ctx->msgfn = fn;
	ctx->msguser = user;
}

double som_version(void) { return 0.070; }	/* the original's gVersion 70 */

void
som_set_random_seed(SOMContext *ctx, unsigned long seed) {
	ctx->x_randnum = seed % IM;
}

/* ---------------------------------------------------------------------- */
/* variables                                                              */

void
InitSignals(void) {
	maxvars = 40;
	numvars = 0;
	if (vars) free(vars);
	if (varx) free(varx);
	vars = (Variable*)allocate((long)maxvars * sizeof(Variable));
	varx = (int*)allocate((long)maxvars * sizeof(int));
}

void
FreeSignals(void) {
	int i;
	for (i=0; i<numvars; ++i) {
		if (vars[i].ac) free(vars[i].ac);
		if (vars[i].data) free(vars[i].data);
		vars[i].ac = NULL;
		vars[i].data = NULL;
	}
	numvars = 0;
}

Variable *
DefineVar(const char *name) {
	Variable *v;
	v = vars + numvars;
	memset(v, 0, sizeof(Variable));
	strncpy(v->name, name, VARNAMELEN-1L);
	v->type = sigScalar;
	v->select = 0;
	v->randomset = 0;
	v->referenced = 0;
	v->assigned = 0;
	v->length = 0L;
	v->ac = NULL;
	v->dc = 0.0;
	v->hilim =  1.0;
	v->lolim = -1.0;
	v->stepsiz = .02;
	v->pfield = 0;
	v->input = 0;
	v->output = 0;
	varx[numvars] = numvars;
	if (++numvars >= maxvars) {
		maxvars += maxvars;
		vars = (Variable*)reallocate(vars, (long)maxvars * sizeof(Variable));
		varx = (int*)reallocate(varx, (long)maxvars * sizeof(int));
		if (ref) {
			int i, j;
			ref = (RefCountPtr)reallocate(ref, (long)maxvars * sizeof(RefCount));
			for (i=maxvars>>1; i<maxvars; ++i) {
				for (j=0; j<MAXJUMP; ++j) {
					ref[i].assigned[j] = 0;
					ref[i].referenced[j] = 0;
				}
			}
		}
		v = vars + numvars - 1;
	}
	return v;
}

void
VarSetType(Variable *v, int type) {
	int i, j;
	v->type = type;
	if (v->ac) free(v->ac);
	v->ac = NULL;
	if (type == sigSample) {
		v->ac = (float*)allocate(gBufferedLength * sizeof(float));
		v->length = gBufferedLength;
		v->hilim = 1.;
		v->lolim = -1.;
		v->stepsiz = 1.;
	} else if (type == sigTable) {
		v->ac = (float*)allocate((BUFLEN+1) * sizeof(float));
		v->length = BUFLEN+1;
		v->hilim = 1.;
		v->lolim = -1.;
		v->stepsiz = 1.;
		for (i=0,j=0; i<BUFLEN; i++,j+=2) {
			v->ac[i] = sine[j];
		}
		v->ac[512] = v->ac[0];
	} else if (type == sigEnvelope) {
		v->ac = (float*)allocate((BUFLEN+1) * sizeof(float));
		v->length = BUFLEN+1;
		v->hilim = 1.;
		v->lolim = 0.;
		v->stepsiz = .1;
		for (i=0; i<BUFLEN+1; i++) {
			v->ac[i] = env0[i];
		}
	} else {
		v->ac = NULL;
		v->length = 0L;
		v->hilim = 1.;
		v->lolim = -1.;
		v->stepsiz = .02;
	}
	v->dc = 0.0;
}

void
VarSetLength(Variable *v, long len) {
	long oldlen;

	if (v->type != sigSample) return;
	oldlen = v->length;
	v->ac = (float*)reallocate(v->ac, (len ? len : 1) * sizeof(float));
	v->length = len;
	if (oldlen < len) {
		memset(v->ac + oldlen, 0, (len - oldlen) * sizeof(float));
	}
}

void
SetGlobalLengths(double dur) {
	int i;
	gSampleDur = dur;
	gSampleLength = dur * gSampleRate;
	gBufferedLength = BUFLEN * floor((gSampleLength + BUFLEN - 1.)/BUFLEN);
	for (i=0; i<numvars; ++i) {
		VarSetLength(vars+i, gBufferedLength);
	}
}

static int
comparesigs(const void *a, const void *b) {
	int typei, typej;
	const int *i = (const int *)a, *j = (const int *)b;

	typei = vars[*i].type;
	typej = vars[*j].type;

	if (typei == typej) {
		if (typei == sigScalar) {
			if (vars[*i].assigned != 2 &&  vars[*j].assigned == 2)
				return -1;
			if (vars[*i].assigned == 2 &&  vars[*j].assigned != 2)
				return  1;
		}
		return strcmp(vars[*i].name, vars[*j].name);
	} else {
		if (typei == sigTable) return -1;
		else if (typej == sigTable) return 1;
		else return typei - typej;
	}
}

void
FixSignalWindow(void) {
	int i;
	/* sort signals */
	for (i=0; i<numvars; ++i) {
		varx[i] = i;
	}
	qsort(varx, numvars, sizeof(int), comparesigs);
}

void
DefaultPField(Variable *v) {
	if (v->pfield > 0) return;
	if      (strcmp(v->name, "instr")==0) v->pfield = 1;
	else if (strcmp(v->name, "time" )==0) v->pfield = 2;
	else if (strcmp(v->name, "dur"  )==0) v->pfield = 3;
	else if (strcmp(v->name, "slur" )==0) v->pfield = 4;
	else if (strcmp(v->name, "note" )==0) v->pfield = 5;
	else if (strcmp(v->name, "volume")==0) v->pfield = 6;
	else if (strcmp(v->name, "veloc")==0) v->pfield = 7;
	else if ((v->name[0] == 'p' || v->name[0] == 'P')
		&& v->name[1] >= '0' && v->name[1] <= '9' ) {
		if (v->name[2] == 0) v->pfield = v->name[1] - '0';
		else if (v->name[2] >= '0' && v->name[2] <= '9' && v->name[3] == 0)	/* port: was + name[2] */
			v->pfield = (v->name[1] - '0')*10 + v->name[2] - '0';
		if (v->pfield >= MAXPF) v->pfield = 0;
	}
	else v->pfield = 0;
}

void
FindExactLimits(Variable *v, double *minval, double *maxval) {
	register double minv, maxv;
	register float *smp;
	register long i;
	smp = v->ac;
	if (!smp || v->length == 0) {
		*minval = *maxval = v->dc;
		return;
	}
	minv = maxv = *smp;
	for (i=0; i<v->length; ++i) {
		if (*smp<minv) minv = *smp;
		else if (*smp>maxv) maxv = *smp;
		smp++;
	}
	*minval = minv;
	*maxval = maxv;
}

void
FindLimits(Variable *v, double *minval, double *maxval, double *stepsiz) {
	double minv, maxv, inc;
	FindExactLimits(v, &minv, &maxv);
	if (minv == maxv) {
		if (maxv == 0.) {		/* port: a flat zero table gave NaN limits */
			*minval = -1.; *maxval = 1.; *stepsiz = 1.;
			return;
		}
		inc = pow(10., floor(log10(sabs(maxv))));
	} else {
		inc = pow(10., floor(log10((maxv - minv)/2.)));
	}
	maxv = ceil(maxv/inc) * inc;
	minv = floor(minv/inc) * inc;
	if (minv == maxv) {		/* port: keep a nonzero range */
		minv -= inc;
		maxv += inc;
	}
	*minval = minv;
	*maxval = maxv;
	*stepsiz = inc;
}

void
AutoScale(Variable *v) {
	double minval, maxval, stepsiz;
	FindLimits(v, &minval, &maxval, &stepsiz);
	v->hilim = maxval;
	v->lolim = minval;
	v->stepsiz = stepsiz;
}

static void
AutoScale2(Variable *v) {
	double minv, maxv, inc;
	FindExactLimits(v, &minv, &maxv);
	if (minv == maxv) {
		if (maxv == 0.) {
			v->hilim = 1.; v->lolim = -1.; v->stepsiz = 1.;
			return;
		}
		inc = pow(10., floor(log10(sabs(maxv))));
		minv -= inc;
		maxv += inc;
	} else {
		inc = pow(10., floor(log10((maxv - minv)/2.)));
	}
	v->hilim = maxv;
	v->lolim = minv;
	v->stepsiz = inc;
}

/* ---------------------------------------------------------------------- */
/* File > New                                                             */

static void
BlankSlate(void) {
	progsize = 0;
	if (U) free(U);
	U = NULL;
	free(som_cur->x_text);
	som_cur->x_text = strdup("");
	som_cur->x_errorline = 0;
	FreeSignals();
	InitSignals();
}

void
som_new_document(SOMContext *ctx) {
	Variable *v;

	SOM_ENTER(ctx);
	BlankSlate();
	free(ctx->x_text);
	ctx->x_text = strdup("freq = midicps(note);\n");

	v = DefineVar("note");
	VarSetType(v, sigScalar);
	v->dc = 60.;
	v->hilim = 108.;
	v->lolim = 20.;
	v->stepsiz = 1.;
	DefaultPField(v);

	v = DefineVar("freq");
	VarSetType(v, sigScalar);
	v->dc = 261.625565300599;
	v->hilim = 4200.;
	v->lolim = 0.;
	v->stepsiz = 50.;
	v->assigned = 2;
	FixSignalWindow();
}

void
som_reset_for_read(SOMContext *ctx) {
	SOM_ENTER(ctx);
	BlankSlate();
}

const char *
som_get_program(SOMContext *ctx) {
	return ctx->x_text;
}

void
som_set_program(SOMContext *ctx, const char *utf8) {
	char *s = strdup(utf8 ? utf8 : "");
	free(ctx->x_text);
	ctx->x_text = s;
}

/* ---------------------------------------------------------------------- */
/* globals                                                                */

double som_sample_rate(SOMContext *ctx) { return ctx->x_sampleRate; }
double som_duration(SOMContext *ctx) { return ctx->x_sampleDur; }
double som_max_delay(SOMContext *ctx) { return ctx->x_delayLength / ctx->x_sampleRate; }
long som_sample_length(SOMContext *ctx) { return ctx->x_sampleLength; }
long som_buffered_length(SOMContext *ctx) { return ctx->x_bufferedLength; }
const char *som_in_filename(SOMContext *ctx) { return ctx->x_infilename; }
const char *som_out_filename(SOMContext *ctx) { return ctx->x_outfilename; }

void
som_set_globals(SOMContext *ctx, double sampleRate, double duration, double maxDelay) {
	SOM_ENTER(ctx);
	if (sampleRate > 0.) gSampleRate = sampleRate;
	if (duration > 0.) SetGlobalLengths(duration);
	else SetGlobalLengths(gSampleDur);
	if (maxDelay >= 0.)
		gDelayLength = BUFLEN * floor((maxDelay * gSampleRate + BUFLEN - 1.)/BUFLEN);
	gDelayLength = smax(gDelayLength, BUFLEN);
}

void
som_set_io_filenames(SOMContext *ctx, const char *in, const char *out) {
	if (in) { strncpy(ctx->x_infilename, in, 31); ctx->x_infilename[31] = 0; }
	if (out) { strncpy(ctx->x_outfilename, out, 31); ctx->x_outfilename[31] = 0; }
}

/* ---------------------------------------------------------------------- */
/* public variable access                                                 */

#define VALIDVAR(ctx, i) ((i) >= 0 && (i) < (ctx)->x_numvars)

int som_var_count(SOMContext *ctx) { return ctx->x_numvars; }

int
som_var_at_row(SOMContext *ctx, int row) {
	if (row < 0 || row >= ctx->x_numvars) return -1;
	return ctx->x_varx[row];
}

int
som_row_of_var(SOMContext *ctx, int var) {
	int i;
	for (i=0; i<ctx->x_numvars; ++i) if (ctx->x_varx[i] == var) return i;
	return -1;
}

int
som_find_var(SOMContext *ctx, const char *name) {
	int i;
	for (i=0; i<ctx->x_numvars; ++i) {
		if (strncmp(ctx->x_vars[i].name, name, VARNAMELEN-1)==0) return i;
	}
	return -1;
}

void
som_get_var_info(SOMContext *ctx, int var, SOMVarInfo *info) {
	Variable *v;
	memset(info, 0, sizeof(*info));
	if (!VALIDVAR(ctx, var)) return;
	v = ctx->x_vars + var;
	memcpy(info->name, v->name, VARNAMELEN);
	info->type = v->type;
	info->value = v->dc;
	info->lolim = v->lolim;
	info->hilim = v->hilim;
	info->stepsiz = v->stepsiz;
	info->pfield = v->pfield;
	info->assigned = v->assigned;
	info->referenced = v->referenced;
	info->randomset = v->randomset;
	info->length = v->ac ? v->length : 0;
}

float *
som_var_data(SOMContext *ctx, int var) {
	if (!VALIDVAR(ctx, var)) return NULL;
	return ctx->x_vars[var].ac;
}

int
som_define_var(SOMContext *ctx, const char *name, int type) {
	Variable *v;
	int i;
	SOM_ENTER(ctx);
	if ((i = som_find_var(ctx, name)) >= 0) return i;
	v = DefineVar(name);
	VarSetType(v, type);
	DefaultPField(v);
	FixSignalWindow();
	return (int)(v - vars);
}

void
som_set_var_value(SOMContext *ctx, int var, double value) {
	if (VALIDVAR(ctx, var)) ctx->x_vars[var].dc = value;
}

void
som_set_var_limits(SOMContext *ctx, int var, double lolim, double hilim, double stepsiz) {
	Variable *v;
	if (!VALIDVAR(ctx, var)) return;
	v = ctx->x_vars + var;
	v->lolim = lolim;
	v->hilim = hilim;
	v->stepsiz = stepsiz;
}

void
som_set_var_pfield(SOMContext *ctx, int var, int pfield) {
	if (VALIDVAR(ctx, var)) ctx->x_vars[var].pfield = sclip(0, pfield, MAXPF-1);
}

void
som_set_var_type(SOMContext *ctx, int var, int type) {
	SOM_ENTER(ctx);
	if (!VALIDVAR(ctx, var) || type < sigScalar || type > sigSample) return;
	VarSetType(vars+var, type);
	FixSignalWindow();
}

/* the OK button of the original DoSetVarDlg */
void
som_set_scalar_params(SOMContext *ctx, int var, double value, double lolim,
	double hilim, double numsteps, int pfield)
{
	Variable *v;
	double minv, maxv, inc;

	if (!VALIDVAR(ctx, var)) return;
	v = ctx->x_vars + var;
	v->dc = value;
	v->lolim = lolim;
	v->hilim = hilim;
	if (v->lolim > v->hilim) {
		value = v->lolim;
		v->lolim = v->hilim;
		v->hilim = value;
	}
	v->pfield = sclip(0, pfield, MAXPF-1);
	if (v->dc > v->hilim || v->dc < v->lolim) {
		if (v->dc > 0.) {
			minv = 0.;
			maxv = 2. * v->dc;
		} else if (v->dc < 0.) {
			minv = 2. * v->dc;
			maxv = -2. * v->dc;
		} else {
			minv = -1.;
			maxv = 1.;
		}
		inc = pow(10., floor(log10((maxv - minv)/2.)));
		v->hilim = ceil(maxv/inc) * inc;
		v->lolim = floor(minv/inc) * inc;
	}
	if (numsteps != 0.) {
		v->stepsiz = (v->hilim - v->lolim) / numsteps;
	} else {
		v->stepsiz = 0.;
	}
}

/* the OK button of the original DoSetBoundsDlg */
void
som_set_graph_bounds(SOMContext *ctx, int var, double hilim, double lolim) {
	Variable *v;
	if (!VALIDVAR(ctx, var)) return;
	v = ctx->x_vars + var;
	if (lolim > hilim) { double t = lolim; lolim = hilim; hilim = t; }
	v->hilim = hilim;
	v->lolim = lolim;
	if (v->lolim == v->hilim) {
		if (v->hilim == 0.) v->stepsiz = 1.;
		else v->stepsiz = pow(10., floor(log10(sabs(v->hilim))));
	} else {
		v->stepsiz = pow(10., floor(log10((v->hilim - v->lolim)/2.)));
	}
	v->hilim = ceil (v->hilim/v->stepsiz) * v->stepsiz;
	v->lolim = floor(v->lolim/v->stepsiz) * v->stepsiz;
	if (v->hilim == v->lolim) {		/* port: keep a nonzero range */
		v->hilim += v->stepsiz;
		v->lolim -= v->stepsiz;
	}
}

void
som_find_limits(SOMContext *ctx, int var, double *minval, double *maxval, double *stepsiz) {
	SOM_ENTER(ctx);
	if (!VALIDVAR(ctx, var)) return;
	FindLimits(vars + var, minval, maxval, stepsiz);
}

void
som_auto_scale(SOMContext *ctx, int var) {
	SOM_ENTER(ctx);
	if (VALIDVAR(ctx, var)) AutoScale(vars + var);
}

void
som_auto_scale_exact(SOMContext *ctx, int var) {
	SOM_ENTER(ctx);
	if (VALIDVAR(ctx, var)) AutoScale2(vars + var);
}

void
som_restore_var(SOMContext *ctx, int var, const SOMVarInfo *info, const float *data, long length) {
	Variable *v;

	SOM_ENTER(ctx);
	if (!VALIDVAR(ctx, var)) return;
	v = vars + var;
	if (v->type != info->type) {
		VarSetType(v, info->type);
		FixSignalWindow();
	}
	v->dc = info->value;
	v->lolim = info->lolim;
	v->hilim = info->hilim;
	v->stepsiz = info->stepsiz;
	v->pfield = info->pfield;
	v->randomset = info->randomset;
	if (data && v->ac) {
		memcpy(v->ac, data, smin(length, v->length) * sizeof(float));
	}
}

void
som_set_sample_data(SOMContext *ctx, int var, const float *data, long length) {
	Variable *v;

	SOM_ENTER(ctx);
	if (!VALIDVAR(ctx, var)) return;
	v = vars + var;
	if (v->type != sigSample) {
		VarSetType(v, sigSample);
		FixSignalWindow();
	}
	memset(v->ac, 0, v->length * sizeof(float));
	memcpy(v->ac, data, smin(length, v->length) * sizeof(float));
}

/* ---------------------------------------------------------------------- */
/* Synth menu                                                             */

void
som_make_random_set(SOMContext *ctx, const int *list, int count) {
	int i;
	Variable *v;

	SOM_ENTER(ctx);
	for (i=0; i<numvars; ++i) vars[i].randomset = 0;
	for (i=0; i<count; ++i) {
		if (!VALIDVAR(ctx, list[i])) continue;
		v = vars + list[i];
		if (v->type == sigScalar && v->assigned != 2) v->randomset = 1;
	}
}

void
som_randomize(SOMContext *ctx) {
	int i, numsteps;
	double r;
	Variable *v;

	SOM_ENTER(ctx);
	for (i=0; i<numvars; ++i) {
		if (vars[varx[i]].randomset) {
			v = vars + varx[i];
			if (v->stepsiz != 0.) {
				numsteps = floor((v->hilim - v->lolim) / v->stepsiz + .5);
				r = xrand();
				v->dc = v->lolim + v->stepsiz * floor(r * (numsteps+1));
			} else {
				r = xrand();
				v->dc = v->lolim + r * (v->hilim - v->lolim);
			}
		}
	}
}

int
som_remove_unused(SOMContext *ctx, const int *list, int count, int all) {
	int i, j, removed = 0;

	SOM_ENTER(ctx);
	for (i=0; i<numvars; ++i) vars[i].select = 0;
	for (j=0; j<count; ++j) {
		if (VALIDVAR(ctx, list[j])) vars[list[j]].select = 1;
	}
	for (i=0; i<numvars; ) {
		if ((vars[i].select || all) && (!vars[i].assigned && !vars[i].referenced)) {
			if (vars[i].ac) free(vars[i].ac);
			if (vars[i].data) free(vars[i].data);
			removed++;
			if (i < --numvars) {
				vars[i] = vars[numvars];
			} else break;
		} else ++i;
	}
	for (i=0; i<numvars; ++i) vars[i].select = 0;
	/* the compiled program refers to variables by index */
	if (removed) progsize = 0;
	FixSignalWindow();
	FixPFields();
	return removed;
}

void
som_memory_report(SOMContext *ctx) {
	int i;
	SOM_ENTER(ctx);
	dprintf("core size = %d\r", maxprogsize);
	dprintf("maxvars = %d\r", maxvars);
	for (i=0; i<numvars; ++i) {
		dprintf("var '%s' length = %ld\r", vars[i].name, vars[i].length);
	}
}
