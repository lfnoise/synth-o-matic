/*
 *  som_export.c
 *
 *  Writing sample variables to sound files, from the original SDFiles.c.
 *  The original wrote 16 bit AIFF and Sound Designer files, normalized to
 *  full scale; WAV in 16, 24 and 32 bit float is new.
 */

#include "som_internal.h"

void som_write_extended80(unsigned char out[10], double x);

typedef struct {
	FILE *f;
	int err;
} Out;

static void
put(Out *o, const void *p, size_t n) {
	if (!o->err && fwrite(p, 1, n, o->f) != n) o->err = 1;
}

static void
putbe(Out *o, unsigned long x, int n) {
	unsigned char b[8];
	int i;
	for (i=n-1; i>=0; --i) { b[i] = x & 0xFF; x >>= 8; }
	put(o, b, n);
}

static void
putle(Out *o, unsigned long x, int n) {
	unsigned char b[8];
	int i;
	for (i=0; i<n; ++i) { b[i] = x & 0xFF; x >>= 8; }
	put(o, b, n);
}

static void
putpstr(Out *o, const char *s, int fieldlen) {
	unsigned char b[256];
	int n = (int)strlen(s);
	memset(b, 0, sizeof(b));
	if (n > fieldlen-1) n = fieldlen-1;
	b[0] = n;
	memcpy(b+1, s, n);
	put(o, b, fieldlen);
}

static void
zeros(Out *o, int n) {
	static const unsigned char z[64];
	while (n > 0) { put(o, z, n > 64 ? 64 : n); n -= 64; }
}

/* the original's conversion: (long)(div * s + 32767.) - 32767 */
static long
to16(double div, float s) {
	long x = (long)(div * s + 32767.) - 32767L;
	return sclip(-32768L, x, 32767L);
}

static long
to24(double div, float s) {
	long x = (long)floor(div * s * 256. + .5);
	return sclip(-8388608L, x, 8388607L);
}

static void
sdheader(Out *o, long nbytes, long srate, const char *filename) {
	int i;
	putbe(o, 1336, 2);		/* headersize */
	putbe(o, 11, 2);		/* version */
	putbe(o, 0, 2);			/* preview */
	zeros(o, 40);
	putbe(o, 0, 4);			/* center */
	putbe(o, 0, 4);			/* left */
	putbe(o, 0, 2);
	putbe(o, (unsigned long)-256, 2);	/* vscale */
	putbe(o, 1, 4);			/* hscale */
	putbe(o, 327, 4);		/* vaxis */
	putbe(o, 4, 1);			/* vtype */
	putbe(o, 0, 1);
	putpstr(o, "%Scale", 32);
	putbe(o, 0, 2);
	putbe(o, 1, 4);			/* haxis */
	putbe(o, 0, 1);			/* htype */
	putbe(o, 0, 1);
	putpstr(o, "      ", 32);
	putbe(o, 0, 2);
	putbe(o, 0, 2);			/* VScrUpdate */
	zeros(o, 8);
	putbe(o, 0, 4);			/* firstsample */
	zeros(o, 12);
	putbe(o, 0, 2);			/* ctl rect */
	putbe(o, 0, 2);
	putbe(o, 15, 2);
	putbe(o, 15, 2);
	zeros(o, 8);
	putbe(o, nbytes, 4);	/* FileSize */
	zeros(o, 66);
	putpstr(o, filename, 64);
	zeros(o, 10);
	putbe(o, 0, 1);			/* NoBackup */
	putbe(o, 0, 1);			/* Mode */
	putbe(o, 0, 4);			/* edithi */
	putbe(o, 0, 4);			/* editlo */
	putbe(o, 0, 2);			/* editext */
	putbe(o, 0, 4);			/* CursorPos */
	putbe(o, 0, 4);
	for (i=0; i<10; ++i) {
		putbe(o, i < 9, 1);	/* Free */
		putbe(o, 0, 1);
		putbe(o, 0, 4);		/* Position */
		putpstr(o, "Untitled", 32);
		putbe(o, 0, 2);
	}
	putbe(o, (unsigned long)-1, 4);	/* MarkerOffset */
	putbe(o, 0, 4);			/* LoopStart */
	putbe(o, 0, 4);			/* LoopEnd */
	putbe(o, 0, 1);			/* ZeroLineOn */
	putbe(o, 0, 1);			/* CursorOn */
	putbe(o, 0, 1);			/* ScalesOn */
	putbe(o, 0, 1);
	putpstr(o, " ", 256);	/* Comment */
	putbe(o, srate, 4);		/* SampRate */
	putbe(o, 33, 4);		/* SampPeriod */
	putbe(o, 16, 2);		/* SampSize */
	zeros(o, 32);			/* CodeType */
	zeros(o, 262);
	putbe(o, (unsigned long)-1, 4);	/* Loop2Start */
	putbe(o, (unsigned long)-1, 4);	/* Loop2End */
	putbe(o, 0, 1);			/* Loop1Type */
	putbe(o, 1, 1);			/* Loop2Type */
	putbe(o, 0, 2);
}

static int
WriteSound(const char *path, float **chans, int numchans, long frames,
	SOMExportFormat format, int normalize)
{
	Out o;
	long i;
	int c, bytes;
	double maxs, div;
	unsigned long datalen;
	unsigned char rate80[10];
	const char *base;

	o.f = fopen(path, "wb");
	o.err = 0;
	if (!o.f) {
		dprintf("Can't create the file '%s'\r", path);
		return 1;
	}

	maxs = 0.;
	for (c=0; c<numchans; ++c) {
		for (i=0; i<frames; ++i) {
			if (fabs(chans[c][i]) > maxs) maxs = fabs(chans[c][i]);
		}
	}
	if (!normalize || maxs == 0.) maxs = 1.;
	div = 32767.0 / maxs;

	bytes = (format == SOMExportWAV24) ? 3 : (format == SOMExportWAVFloat) ? 4 : 2;
	datalen = (unsigned long)frames * numchans * bytes;

	switch (format) {
		case SOMExportAIFF16 :
			put(&o, "FORM", 4);
			putbe(&o, 4 + 26 + 16 + datalen, 4);
			put(&o, "AIFF", 4);
			put(&o, "COMM", 4);
			putbe(&o, 18, 4);
			putbe(&o, numchans, 2);
			putbe(&o, frames, 4);
			putbe(&o, 16, 2);
			som_write_extended80(rate80, gSampleRate);
			put(&o, rate80, 10);
			put(&o, "SSND", 4);
			putbe(&o, 8 + datalen, 4);
			putbe(&o, 0, 4);
			putbe(&o, 0, 4);
			for (i=0; i<frames; ++i)
				for (c=0; c<numchans; ++c)
					putbe(&o, (unsigned long)to16(div, chans[c][i]), 2);
			break;
		case SOMExportSoundDesigner :
			base = strrchr(path, '/');
			base = base ? base+1 : path;
			sdheader(&o, (long)datalen, (long)gSampleRate, base);
			for (i=0; i<frames; ++i)
				for (c=0; c<numchans; ++c)
					putbe(&o, (unsigned long)to16(div, chans[c][i]), 2);
			break;
		case SOMExportWAV16 :
		case SOMExportWAV24 :
		case SOMExportWAVFloat :
			put(&o, "RIFF", 4);
			putle(&o, 4 + 24 + 8 + datalen + (datalen & 1), 4);
			put(&o, "WAVE", 4);
			put(&o, "fmt ", 4);
			putle(&o, 16, 4);
			putle(&o, format == SOMExportWAVFloat ? 3 : 1, 2);
			putle(&o, numchans, 2);
			putle(&o, (unsigned long)gSampleRate, 4);
			putle(&o, (unsigned long)gSampleRate * numchans * bytes, 4);
			putle(&o, numchans * bytes, 2);
			putle(&o, bytes * 8, 2);
			put(&o, "data", 4);
			putle(&o, datalen, 4);
			for (i=0; i<frames; ++i) {
				for (c=0; c<numchans; ++c) {
					if (format == SOMExportWAV16) {
						putle(&o, (unsigned long)to16(div, chans[c][i]), 2);
					} else if (format == SOMExportWAV24) {
						putle(&o, (unsigned long)to24(div, chans[c][i]), 3);
					} else {
						union { float f; unsigned int u; } x;
						x.f = chans[c][i] / (normalize ? maxs : 1.);
						putle(&o, x.u, 4);
					}
				}
			}
			if (datalen & 1) putle(&o, 0, 1);
			break;
	}
	if (fclose(o.f) != 0) o.err = 1;
	if (o.err) dprintf("Error writing the file '%s'\r", path);
	return o.err;
}

static Variable *
SampleVar(SOMContext *ctx, int var) {
	SOM_ENTER(ctx);
	if (var < 0 || var >= numvars) return NULL;
	if (vars[var].type != sigSample || !vars[var].ac) return NULL;
	return vars + var;
}

int
som_export_sample(SOMContext *ctx, int var, const char *path, SOMExportFormat format, int normalize) {
	Variable *v;
	float *chans[1];

	if (!(v = SampleVar(ctx, var))) return 1;
	chans[0] = v->ac;
	return WriteSound(path, chans, 1, smin(gSampleLength, v->length), format, normalize);
}

int
som_export_stereo(SOMContext *ctx, int left, int right, const char *path,
	SOMExportFormat format, int normalize)
{
	Variable *l, *r;
	float *chans[2];

	if (!(l = SampleVar(ctx, left)) || !(r = SampleVar(ctx, right))) return 1;
	chans[0] = l->ac;
	chans[1] = r->ac;
	return WriteSound(path, chans, 2, smin(gSampleLength, smin(l->length, r->length)), format, normalize);
}
