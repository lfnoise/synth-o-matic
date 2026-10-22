/*
 *  som_file.c
 *
 *  Reading and writing patch files, from the original WriteSOMFile2.c.
 *
 *  The original wrote its structs straight to disk from a 68000 compiled by
 *  THINK C with the 68881 option, so a file is big endian with 2 byte ints,
 *  4 byte longs and floats, and 12 byte (96 bit extended) doubles:
 *
 *    'SOM1' size
 *       version (v50+), sampleLength, sampleRate, [infile[32] outfile[32] (v70+)]
 *    'Prog' size  program text, MacRoman with CR line ends
 *    'Vars' size  numvars, [varsize (v50+)], then per variable a FileVar
 *       name[32] type lolim hilim [stepsiz pfield pad]  followed by
 *       513 floats for a table or envelope, or a 12 byte double for a scalar.
 *
 *  Files written before version 50 have no version word and 42 byte
 *  variable records.  This writes version 70.
 */

#include <CoreFoundation/CoreFoundation.h>
#include "som_internal.h"

#define FILEVERSION 70
#define FILEVARSIZE 48		/* sizeof(FileVar) on the 68000 */
#define OLDVARSIZE 42

typedef struct {
	const unsigned char *p, *end;
	int bad;
} Reader;

static unsigned long
rd(Reader *r, int n) {
	unsigned long x = 0;
	if (r->end - r->p < n) { r->bad = 1; r->p = r->end; return 0; }
	while (n--) x = (x << 8) | *r->p++;
	return x;
}

static void
rdbytes(Reader *r, void *dst, long n) {
	if (r->end - r->p < n) { r->bad = 1; memset(dst, 0, n); r->p = r->end; return; }
	memcpy(dst, r->p, n);
	r->p += n;
}

static float
rdfloat(Reader *r) {
	union { unsigned int i; float f; } u;
	u.i = (unsigned int)rd(r, 4);
	return u.f;
}

/* 68881 extended: sign+exponent (16), padding (16), mantissa (64) */
static double
rdextended(Reader *r, int padded) {
	unsigned int se;
	unsigned long long m;
	int e;
	double x;

	se = (unsigned int)rd(r, 2);
	if (padded) rd(r, 2);
	m = ((unsigned long long)rd(r, 4) << 32);
	m |= rd(r, 4);
	e = se & 0x7FFF;
	if (e == 0 && m == 0) return (se & 0x8000) ? -0. : 0.;
	if (e == 0x7FFF) return (m << 1) ? NAN : ((se & 0x8000) ? -INFINITY : INFINITY);
	x = ldexp((double)m, e - 16383 - 63);
	return (se & 0x8000) ? -x : x;
}

/* ---------------------------------------------------------------------- */

typedef struct {
	unsigned char *buf;
	size_t len, cap;
} Writer;

static void
wrbytes(Writer *w, const void *src, size_t n) {
	if (w->len + n > w->cap) {
		w->cap = (w->len + n) * 2 + 1024;
		w->buf = reallocate(w->buf, w->cap);
	}
	memcpy(w->buf + w->len, src, n);
	w->len += n;
}

static void
wr(Writer *w, unsigned long x, int n) {
	unsigned char b[8];
	int i;
	for (i=n-1; i>=0; --i) { b[i] = x & 0xFF; x >>= 8; }
	wrbytes(w, b, n);
}

static void
wrfloat(Writer *w, float f) {
	union { unsigned int i; float f; } u;
	u.f = f;
	wr(w, u.i, 4);
}

static void
wrextended(Writer *w, double x, int padded) {
	unsigned int se = 0;
	unsigned long long m = 0;
	int e;

	if (x < 0. || (x == 0. && signbit(x))) { se = 0x8000; x = -x; }
	if (isnan(x)) { se |= 0x7FFF; m = 0xC000000000000000ULL; }
	else if (isinf(x)) { se |= 0x7FFF; }
	else if (x != 0.) {
		x = frexp(x, &e);			/* x in [.5, 1) */
		se |= (unsigned int)(e - 1 + 16383);
		m = (unsigned long long)ldexp(x, 64);
	}
	wr(w, se, 2);
	if (padded) wr(w, 0, 2);
	wr(w, (unsigned long)(m >> 32), 4);
	wr(w, (unsigned long)(m & 0xFFFFFFFFUL), 4);
}

/* exported for the AIFF writer */
void
som_write_extended80(unsigned char out[10], double x) {
	Writer w = { NULL, 0, 0 };
	wrextended(&w, x, 0);
	memcpy(out, w.buf, 10);
	free(w.buf);
}

/* ---------------------------------------------------------------------- */
/* MacRoman <-> UTF-8                                                     */

static char *
MacRomanToUTF8(const unsigned char *src, long len) {
	CFStringRef s;
	char *out;
	CFIndex max, i;

	s = CFStringCreateWithBytes(NULL, src, len, kCFStringEncodingMacRoman, false);
	if (!s) {
		out = allocate(len+1);
		memcpy(out, src, len);
	} else {
		max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
		out = allocate(max);
		CFStringGetCString(s, out, max, kCFStringEncodingUTF8);
		CFRelease(s);
	}
	for (i=0; out[i]; ++i) {
		if (out[i] == '\r') {
			if (out[i+1] == '\n') memmove(out+i, out+i+1, strlen(out+i));
			else out[i] = '\n';
		}
	}
	return out;
}

static unsigned char *
UTF8ToMacRoman(const char *src, long *len) {
	CFStringRef s;
	CFIndex used = 0, n;
	unsigned char *out;
	long i;

	s = CFStringCreateWithCString(NULL, src, kCFStringEncodingUTF8);
	if (!s) {
		*len = strlen(src);
		out = allocate(*len + 1);
		memcpy(out, src, *len);
	} else {
		n = CFStringGetLength(s);
		CFStringGetBytes(s, CFRangeMake(0, n), kCFStringEncodingMacRoman, '?', false, NULL, 0, &used);
		out = allocate(used + 1);
		CFStringGetBytes(s, CFRangeMake(0, n), kCFStringEncodingMacRoman, '?', false, out, used, &used);
		CFRelease(s);
		*len = used;
	}
	for (i=0; i<*len; ++i) if (out[i] == '\n') out[i] = '\r';
	return out;
}

/* ---------------------------------------------------------------------- */
/* reading                                                                */

#define FOURCC(a,b,c,d) (((unsigned long)(a)<<24)|((unsigned long)(b)<<16)|((unsigned long)(c)<<8)|(unsigned long)(d))

int
som_is_patch_data(const unsigned char *data, size_t length) {
	return length >= 8 && memcmp(data, "SOM1", 4) == 0;
}

static int
ReadVar(Reader *r, int readsize) {
	Variable *v2;
	char name[VARNAMELEN];
	int type, pfield = 0;
	float lolim, hilim, stepsiz = 0.;
	const unsigned char *start = r->p;
	int i;

	rdbytes(r, name, VARNAMELEN);
	name[VARNAMELEN-1] = 0;
	type = (short)rd(r, 2);
	lolim = rdfloat(r);
	hilim = rdfloat(r);
	if (readsize > 42) stepsiz = rdfloat(r);
	if (readsize > 46) pfield = (signed char)rd(r, 1);
	r->p = start + readsize;		/* skip what we don't know about */
	if (r->p > r->end) { r->bad = 1; return 0; }
	if (type < sigScalar || type > sigSample) {
		dprintf("bad variable type %d for '%s'\r", type, name);
		r->bad = 1;
		return 0;
	}

	v2 = DefineVar(name);
	VarSetType(v2, type);
	v2->lolim = lolim;
	v2->hilim = hilim;
	if (readsize > 42) v2->stepsiz = stepsiz;
	else {
		v2->stepsiz = (hilim - lolim)/10.;
	}
	if (pfield <= 0 || pfield >= MAXPF) DefaultPField(v2);
	else v2->pfield = pfield;
	switch(v2->type) {
		Case sigTable or sigEnvelope : {
			for (i=0; i<BUFLEN+1; ++i) v2->ac[i] = rdfloat(r);
		}
		Case sigScalar : v2->dc = rdextended(r, 1);
		Case sigSample : ;
	}
	return !r->bad;
}

static int
ReadVarBlock(Reader *r, int old) {
	int i;
	int numtoread, readsize;

	numtoread = (short)rd(r, 2);
	if (old) readsize = OLDVARSIZE;
	else readsize = (short)rd(r, 2);
	if (numtoread < 0 || readsize < OLDVARSIZE || readsize > 256) {
		dprintf("bad variable block\r");
		return 0;
	}
	for (i=0; i<numtoread; ++i) {
		if (!ReadVar(r, readsize)) return 0;
	}
	return 1;
}

static int
ReadProgram(Reader *r, long size) {
	if (size < 0 || r->end - r->p < size) return 0;
	free(som_cur->x_text);
	som_cur->x_text = MacRomanToUTF8(r->p, size);
	r->p += size;
	return 1;
}

static int
plausible_rate(double sr) {
	return sr >= 100. && sr <= 1000000.;
}

int
som_read_patch(SOMContext *ctx, const unsigned char *data, size_t length) {
	Reader r;
	unsigned long type, size;
	long sampleLength;
	double sampleRate;
	word version;
	int old = 0, ok;

	SOM_ENTER(ctx);
	som_reset_for_read(ctx);
	ctx->x_allocerr_armed = 1;
	if (setjmp(allocerr)) {
		ctx->x_allocerr_armed = 0;
		dprintf("Not Enough Memory\r");
		som_reset_for_read(ctx);
		return 1;
	}

	r.p = data; r.end = data + length; r.bad = 0;
	type = rd(&r, 4);
	size = rd(&r, 4);
	if (type != FOURCC('S','O','M','1')) {
		dprintf("invalid file type\r");
		ctx->x_allocerr_armed = 0;
		return 1;
	}

	/* version 50 and later start with a version word */
	version = (word)rd(&r, 2);
	sampleLength = (long)rd(&r, 4);
	sampleRate = rdextended(&r, 1);
	if (version >= 70) {
		rdbytes(&r, gInfilename, 32);
		rdbytes(&r, gOutfilename, 32);
		gInfilename[31] = gOutfilename[31] = 0;
	}
	type = rd(&r, 4);
	if (type != FOURCC('P','r','o','g') || !plausible_rate(sampleRate)) {
		/* try again : an old file */
		r.p = data + 8;
		r.bad = 0;
		version = 10;
		old = 1;
		strcpy(gInfilename, "sndin");
		strcpy(gOutfilename, "sndout");
		sampleLength = (long)rd(&r, 4);
		sampleRate = rdextended(&r, 1);
		type = rd(&r, 4);
		if (type != FOURCC('P','r','o','g')) {
			dprintf("missing program block\r");
			ctx->x_allocerr_armed = 0;
			return 1;
		}
	}
	if (!plausible_rate(sampleRate)) {
		dprintf("implausible sample rate %g, using 22050\r", sampleRate);
		sampleRate = 22050.;
	}
	gFileVersion = version;
	gSampleRate = sampleRate;
	if (sampleLength <= 0) sampleLength = 160L*BUFLEN;
	gSampleLength = sampleLength;
	gSampleDur = gSampleLength/gSampleRate;
	gBufferedLength = BUFLEN * floor((gSampleLength + BUFLEN - 1.)/BUFLEN);

	size = rd(&r, 4);
	if (!ReadProgram(&r, size)) {
		dprintf("damaged program block\r");
		ctx->x_allocerr_armed = 0;
		return 1;
	}
	type = rd(&r, 4);
	size = rd(&r, 4);
	if (type != FOURCC('V','a','r','s')) {
		dprintf("missing variable block\r");
		ok = 1;
	} else {
		ok = ReadVarBlock(&r, old);
		if (!ok) dprintf("the variable block is damaged; some variables were not read\r");
	}
	ctx->x_allocerr_armed = 0;
	FixSignalWindow();
	FixPFields();
	return 0;
}

/* ---------------------------------------------------------------------- */
/* writing                                                                */

static void
WriteVar(Writer *w, Variable *v) {
	char name[VARNAMELEN];
	int i;

	memset(name, 0, VARNAMELEN);
	strncpy(name, v->name, VARNAMELEN-1);
	wrbytes(w, name, VARNAMELEN);
	wr(w, v->type, 2);
	wrfloat(w, v->lolim);
	wrfloat(w, v->hilim);
	wrfloat(w, v->stepsiz);
	wr(w, (unsigned char)v->pfield, 1);
	wr(w, 0, 1);
	switch(v->type) {
		Case sigTable or sigEnvelope : {
			for (i=0; i<BUFLEN+1; ++i) wrfloat(w, v->ac ? v->ac[i] : 0.f);
		}
		Case sigScalar : wrextended(w, v->dc, 1);
		Case sigSample : ;
	}
}

unsigned char *
som_write_patch(SOMContext *ctx, size_t *length) {
	Writer w = { NULL, 0, 0 };
	unsigned char *text;
	long textlen;
	size_t sizepos;
	char fname[32];
	int i;

	SOM_ENTER(ctx);
	wrbytes(&w, "SOM1", 4);
	wr(&w, 0, 4);		/* patched below */
	wr(&w, FILEVERSION, 2);
	wr(&w, (unsigned long)gSampleLength, 4);
	wrextended(&w, gSampleRate, 1);
	memset(fname, 0, 32); strncpy(fname, gInfilename, 31); wrbytes(&w, fname, 32);
	memset(fname, 0, 32); strncpy(fname, gOutfilename, 31); wrbytes(&w, fname, 32);

	text = UTF8ToMacRoman(ctx->x_text, &textlen);
	wrbytes(&w, "Prog", 4);
	wr(&w, textlen, 4);
	wrbytes(&w, text, textlen);
	free(text);

	wrbytes(&w, "Vars", 4);
	sizepos = w.len;
	wr(&w, 0, 4);
	wr(&w, numvars, 2);
	wr(&w, FILEVARSIZE, 2);
	for (i=0; i<numvars; ++i) {
		WriteVar(&w, vars+i);
	}
	/* block sizes */
	{
		unsigned long n = w.len - sizepos - 4;
		w.buf[sizepos] = n >> 24; w.buf[sizepos+1] = n >> 16;
		w.buf[sizepos+2] = n >> 8; w.buf[sizepos+3] = n;
		n = w.len - 8;
		w.buf[4] = n >> 24; w.buf[5] = n >> 16; w.buf[6] = n >> 8; w.buf[7] = n;
	}
	*length = w.len;
	return w.buf;
}
