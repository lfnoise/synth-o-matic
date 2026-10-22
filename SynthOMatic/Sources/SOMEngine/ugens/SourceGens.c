/*
 *  SourceGens.c
 *
 *  Ported from the original Synth-O-Matic source by scripts/port_ugens.py.
 *  Apart from the commented 'port:' fixes the code is as it was written.
 */

#include "som_internal.h"
#include <math.h>


typedef struct {
	long countdown, seglen;
	double curve, slope, midpt, nextmidpt, value, nextvalue;
} NoiseRec, *NoiseRecPtr;

void noise0(UGen *this)		/* Step Noise */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, nextvalue, denom;
	register long nsmps;
	NoiseRecPtr nrec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = nrec = (NoiseRec*)allocate(sizeof(NoiseRec));
		value = 0.0;
		countdown = 0 ;
	} else {
		nrec = (NoiseRec*)this->data;
		countdown = nrec->countdown;
		seglen = nrec->seglen;
		value = nrec->value;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			randnum = (IA * randnum + IC) % IM;
			value = *s2 * (1.0 - randfac * randnum);
		}
		nsmps = numdone = smin(smpleft, countdown);
		do {
			*outi++ = value;
		} while (--nsmps);
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
		s2 += numdone;
	} while (smpleft);
	nrec->seglen = seglen;
	nrec->countdown = countdown;
	nrec->value = value;
}

void noise1(UGen *this)		/* Linear Interpolated Noise */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, nextvalue, denom;
	register long nsmps;
	NoiseRecPtr nrec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = nrec = (NoiseRec*)allocate(sizeof(NoiseRec));
		value = nextvalue = slope = 0.0;
		countdown = 0 ;
	} else {
		nrec = (NoiseRec*)this->data;
		countdown = nrec->countdown;
		seglen = nrec->seglen;
		slope = nrec->slope;
		value = nrec->value;
		nextvalue = nrec->nextvalue;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			value = nextvalue;
			randnum = (IA * randnum + IC) % IM;
			nextvalue = *s2 * (1.0 - randfac * randnum);
			slope = (nextvalue - value) / seglen ;
		}
		nsmps = numdone = smin(smpleft, countdown);
		do {
			*outi++ = value;
			value += slope;
		} while (--nsmps);
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
		s2 += numdone;
	} while (smpleft);
	nrec->seglen = seglen;
	nrec->countdown = countdown;
	nrec->slope = slope;
	nrec->nextvalue = nextvalue;
	nrec->value = value;
}


void noise2(UGen *this)		/* Quadratic Spline Noise */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, midpt, slope;
	double *s1, *s2, nextvalue, curve, value, nextmidpt, denom;
	register long nsmps;
	NoiseRecPtr nrec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = nrec = (NoiseRec*)allocate(sizeof(NoiseRec));
		value = midpt = 0.0;
		denom = smax(*s1, .000001);
		seglen = gSampleRate / denom;
		randnum = (IA * randnum + IC) % IM;
		nextvalue = *s2 * (1.0 - randfac * randnum);
		nextmidpt = (nextvalue) / 2.;
		slope = nextmidpt / seglen;
		countdown = 0 ;
	} else {
		nrec = (NoiseRec*)this->data;
		countdown = nrec->countdown;
		seglen = nrec->seglen;
		curve = nrec->curve;
		slope = nrec->slope;
		value = nrec->value;
		midpt = nrec->midpt;
		nextmidpt = nrec->nextmidpt;
		nextvalue = nrec->nextvalue;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			value = nextvalue;
			randnum = (IA * randnum + IC) % IM;
			nextvalue = *s2 * (1.0 - randfac * randnum);
			midpt = nextmidpt;
			nextmidpt = (nextvalue + value) / 2.;
			curve = 2. * (nextmidpt - midpt - seglen * slope) / (seglen*seglen + seglen);
		}
		nsmps = numdone = smin(smpleft, countdown);
		do {
			*outi++ = midpt;
			slope += curve;
			midpt += slope;
		} while (--nsmps);
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
		s2 += numdone;
	} while (smpleft);
	nrec->seglen = seglen;
	nrec->countdown = countdown;
	nrec->curve = curve;
	nrec->slope = slope;
	nrec->nextvalue = nextvalue;
	nrec->value = value;
	nrec->midpt = midpt;
	nrec->nextmidpt = nextmidpt;
}

typedef struct {
	long countdown, seglen;
	double slope, value, nextvalue;
	int evenodd;
} TriRec, *TriRecPtr;

void triang(UGen *this)		/* triangle lfo */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, nextvalue, denom;
	register long nsmps;
	TriRecPtr trec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = trec = (TriRec*)allocate(sizeof(TriRec));
		trec->evenodd = 1;
		value = nextvalue = *s2;
		countdown = 0 ;
	} else {
		trec = (TriRec*)this->data;
		countdown = trec->countdown;
		seglen = trec->seglen;
		slope = trec->slope;
		value = trec->value;
		nextvalue = trec->nextvalue;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = 2. * smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			value = nextvalue;
			trec->evenodd = 1 - trec->evenodd;
			nextvalue = (trec->evenodd) ? *s2 : -*s2 ;
			slope = (nextvalue - value) / seglen ;
		}
		nsmps = numdone = smin(smpleft, countdown);
		do {
			*outi++ = value;
			value += slope;
		} while (--nsmps);
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
		s2 += numdone;
	} while (smpleft);
	trec->seglen = seglen;
	trec->countdown = countdown;
	trec->slope = slope;
	trec->nextvalue = nextvalue;
	trec->value = value;
}


void square(UGen *this)		/* square wave lfo */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, denom;
	register long nsmps;
	TriRecPtr trec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = trec = (TriRec*)allocate(sizeof(TriRec));
		trec->evenodd = 0;
		value = 0.0;
		countdown = 0 ;
	} else {
		trec = (TriRec*)this->data;
		countdown = trec->countdown;
		seglen = trec->seglen;
		value = trec->value;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = 2. * smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			trec->evenodd = 1 - trec->evenodd;
		}
		nsmps = numdone = smin(smpleft, countdown);
		if (trec->evenodd) {
			do {
				*outi++ = *s2++;
			} while (--nsmps);
		} else {
			do {
				*outi++ = -*s2++;
			} while (--nsmps);
		}
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
	} while (smpleft);
	trec->seglen = seglen;
	trec->countdown = countdown;
	trec->value = value;
}


void square0(UGen *this)		/* square wave lfo */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, denom;
	register long nsmps;
	TriRecPtr trec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = trec = (TriRec*)allocate(sizeof(TriRec));
		trec->evenodd = 0;
		value = 0.0;
		countdown = 0 ;
	} else {
		trec = (TriRec*)this->data;
		countdown = trec->countdown;
		seglen = trec->seglen;
		value = trec->value;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = 2. * smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			trec->evenodd = 1 - trec->evenodd;
		}
		nsmps = numdone = smin(smpleft, countdown);
		if (trec->evenodd) {
			do {
				*outi++ = *s2++;
			} while (--nsmps);
		} else {
			do {
				*outi++ = 0.;
			} while (--nsmps);
			s2 += numdone;
		}
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
	} while (smpleft);
	trec->seglen = seglen;
	trec->countdown = countdown;
	trec->value = value;
}


void saw(UGen *this)		/* sawtooth lfo */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, nextvalue, denom;
	register long nsmps;
	TriRecPtr trec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = trec = (TriRec*)allocate(sizeof(TriRec));
		trec->evenodd = 0;
		value = nextvalue = 0.0;
		countdown = 0 ;
	} else {
		trec = (TriRec*)this->data;
		countdown = trec->countdown;
		seglen = trec->seglen;
		slope = trec->slope;
		value = trec->value;
		nextvalue = trec->nextvalue;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			denom = smax(*s1, .000001);
			seglen = gSampleRate / denom;
			seglen = smax(2, seglen);
			countdown = seglen;
			value = -*s2;
			nextvalue = *s2;
			slope = (nextvalue - value) / seglen ;
		}
		nsmps = numdone = smin(smpleft, countdown);
		do {
			*outi++ = value;
			value += slope;
		} while (--nsmps);
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
		s2 += numdone;
	} while (smpleft);
	trec->seglen = seglen;
	trec->countdown = countdown;
	trec->slope = slope;
	trec->nextvalue = nextvalue;
	trec->value = value;
}



void blip(UGen *this)	/* impulse wave lfo */
{
	long smpleft, numdone, seglen, countdown;
	register double *outi, value, slope;
	double *s1, *s2, denom;
	register long nsmps;
	TriRecPtr trec;
	
	/* in[0] frequency , in[1] amplitude */

	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	if (this->state == dfUninitted) {
		this->data = trec = (TriRec*)allocate(sizeof(TriRec));
		trec->evenodd = 0;
		value = 0.0;
		countdown = 0 ;
	} else {
		trec = (TriRec*)this->data;
		countdown = trec->countdown;
		seglen = trec->seglen;
		value = trec->value;
	}
	smpleft = BUFLEN;
	do {
		if (countdown <= 0) {
			trec->evenodd = 1 - trec->evenodd;
			if (trec->evenodd) {
				denom = smax(*s1, .000001);
				seglen = 1;
				countdown = seglen;
			} else {
				denom = smax(*s1, .000001);
				seglen = gSampleRate / denom - 1;
				seglen = smax(1, seglen);
				countdown = seglen;
			}
		}
		nsmps = numdone = smin(smpleft, countdown);
		if (trec->evenodd) {
			do {
				*outi++ = *s2++;
			} while (--nsmps);
		} else {
			do {
				*outi++ = 0.; 
			} while (--nsmps);
			s2 += numdone;
		}
		smpleft -= numdone;
		countdown -= numdone;
		s1 += numdone;
	} while (smpleft);
	trec->seglen = seglen;
	trec->countdown = countdown;
	trec->value = value;
}

typedef struct {
	long freq, phase, pulsewidth;
} PulseRec, *PulseRecPtr;

void pulse(UGen *this)		/* pulse wave generator for controls */
{
	register double *outi, *s1, *s2, *s3;
	register long nsmps, phase, pulsewidth, freq;
	PulseRecPtr osc;
	
	/* in[0] is fm, in[1] is pulse width, in[2] is amp */
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	if (this->state == dfUninitted) {
		this->data = osc = (PulseRecPtr)allocate(sizeof(PulseRec));
		if (this->in[0]->type == sigScalar) osc->freq  = cps2inc * *s1;
		else osc->freq  = 0L;
		if (this->in[1]->type == sigScalar) {
			pulsewidth = *s2 * onecyc;
			osc->pulsewidth = sclip(1, pulsewidth, onecyc-1);
		} else {
			osc->pulsewidth = 0L;
		}
		osc->phase  = 0L;
	} else {
		osc = (PulseRecPtr)this->data;
	}
	
	outi = this->out;
	phase = osc->phase;
	pulsewidth = osc->pulsewidth;
	freq  = osc->freq;
	nsmps = BUFLEN;
	if (this->in[1]->type == sigScalar) {		/* no fm */
		if (this->in[2]->type == sigScalar) {		/* no pwm */
			do {
				if (phase < pulsewidth) *outi++ = *s3++;
				else *outi++ = -*s3++;
				phase += freq;
				phase &= longmask;
			} while (--nsmps);
		} else {
			do {	/* pwm */
				pulsewidth = *s2++ * onecyc;
				pulsewidth = sclip(1, pulsewidth, onecyc-1);
				if (phase < pulsewidth) *outi++ = *s3++;
				else *outi++ = -*s3++;
				phase += freq;
				phase &= longmask;
			} while (--nsmps);
		}
	} else {
		if (this->in[2]->type == sigScalar) {		/* no pwm */
			do {
				if (phase < pulsewidth) *outi++ = *s3++;
				else *outi++ = -*s3++;
				phase += (long)(cps2inc * *s1++);
				phase &= longmask;
			} while (--nsmps);
		} else {
			do {	/* pwm */
				pulsewidth = *s2++ * onecyc;
				pulsewidth = sclip(1, pulsewidth, onecyc-1);
				if (phase < pulsewidth) *outi++ = *s3++;
				else *outi++ = -*s3++;
				phase += (long)(cps2inc * *s1++);
				phase &= longmask;
			} while (--nsmps);
		}
	}
	osc->phase = phase;
}


void pulse0(UGen *this)		/* pulse wave generator for controls */
{
	register double *outi, *s1, *s2, *s3;
	register long nsmps, phase, pulsewidth, freq;
	PulseRecPtr osc;
	
	/* in[0] is fm, in[1] is pulse width, in[2] is amp */
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	if (this->state == dfUninitted) {
		this->data = osc = (PulseRecPtr)allocate(sizeof(PulseRec));
		if (this->in[0]->type == sigScalar) osc->freq  = cps2inc * *s1;
		else osc->freq  = 0L;
		if (this->in[1]->type == sigScalar) {
			pulsewidth = *s2 * onecyc;
			osc->pulsewidth = sclip(1, pulsewidth, onecyc-1);
		} else {
			osc->pulsewidth = 0L;
		}
		osc->phase  = 0L;
	} else {
		osc = (PulseRecPtr)this->data;
	}
	
	outi = this->out;
	phase = osc->phase;
	pulsewidth = osc->pulsewidth;
	freq  = osc->freq;
	nsmps = BUFLEN;
	if (this->in[1]->type == sigScalar) {		/* no fm */
		if (this->in[2]->type == sigScalar) {		/* no pwm */
			do {
				if (phase < pulsewidth) *outi++ = *s3++;
				else { *outi++ = 0.; s3++; }
				phase += freq;
				phase &= longmask;
			} while (--nsmps);
		} else {
			do {	/* pwm */
				pulsewidth = *s2++ * onecyc;
				pulsewidth = sclip(1, pulsewidth, onecyc-1);
				if (phase < pulsewidth) *outi++ = *s3++;
				else { *outi++ = 0.; s3++; }
				phase += freq;
				phase &= longmask;
			} while (--nsmps);
		}
	} else {
		if (this->in[2]->type == sigScalar) {		/* no pwm */
			do {
				if (phase < pulsewidth) *outi++ = *s3++;
				else { *outi++ = 0.; s3++; }
				phase += (long)(cps2inc * *s1++);
				phase &= longmask;
			} while (--nsmps);
		} else {
			do {	/* pwm */
				pulsewidth = *s2++ * onecyc;
				pulsewidth = sclip(1, pulsewidth, onecyc-1);
				if (phase < pulsewidth) *outi++ = *s3++;
				else { *outi++ = 0.; s3++; }
				phase += (long)(cps2inc * *s1++);
				phase &= longmask;
			} while (--nsmps);
		}
	}
	osc->phase = phase;
}

typedef struct {
	double slope, level;
} RampRec, *RampRecPtr;

void ramp(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmps;
	RampRecPtr rrec;
	
	/* in[0] start level , in[1] end level */

	if (this->state == dfUninitted) {
		this->data = rrec = (RampRec*)allocate(sizeof(RampRec));
		rrec->level = this->in[0]->out[0];
		rrec->slope = (this->in[1]->out[0] - this->in[0]->out[0]) / (gSampleLength - 1);
	} else {
		rrec = (RampRec*)this->data;
	}
	outi = this->out;
	slope = rrec->slope;
	level = rrec->level;
	nsmps = BUFLEN;
	do {
		*outi++ = level;
		level += slope;
	} while (--nsmps);
	rrec->level = level;
}

typedef struct {
	long t1, t2, t3;
	double slope1, slope2, level;
	int stage;
} LinenRec, *LinenRecPtr;

void linen(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	LinenRecPtr rrec;
	double *s1, *s2, *s3, *s4;
	long nsmps, numdone;
	/* in[0] max level , in[1] rise time, in[2] sus time, in[3] fall time */

	outi = this->out;
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;
		s2 = this->in[1]->out;
		s3 = this->in[2]->out;
		s4 = this->in[3]->out;
		this->data = rrec = (LinenRec*)allocate(sizeof(LinenRec));
		rrec->level = 0.;
		rrec->slope1 = *s1 / (gSampleRate * *s2);
		rrec->slope2 = - *s1 / (gSampleRate * *s4);
		rrec->stage = 0;
		rrec->t1 = gSampleRate * *s2;
		rrec->t2 = gSampleRate * *s3;
		rrec->t3 = gSampleRate * *s4;
	} else {
		rrec = (LinenRec*)this->data;
	}
	nsmps = BUFLEN;
	if (rrec->stage == 0) {
		slope = rrec->slope1;
		level = rrec->level;
		nsmp2 = numdone = smin(nsmps, rrec->t1);
		while (nsmp2--) {
			*outi++ = level;
			level += slope;
		}
		rrec->level = level;
		nsmps -= numdone;
		rrec->t1 -= numdone;
		if (rrec->t1 <= 0) rrec->stage ++;
	}
	if (rrec->stage == 1 && nsmps) {
		level = rrec->level;
		nsmp2 = numdone = smin(nsmps, rrec->t2);
		while (nsmp2--) {
			*outi++ = level;
		}
		rrec->level = level;
		nsmps -= numdone;
		rrec->t2 -= numdone;
		if (rrec->t2 <= 0) rrec->stage ++;
	}
	if (rrec->stage == 2 && nsmps) {
		slope = rrec->slope2;
		level = rrec->level;
		nsmp2 = numdone = smin(nsmps, rrec->t3);
		while (nsmp2--) {
			*outi++ = level;
			level += slope;
		}
		rrec->level = level;
		nsmps -= numdone;
		rrec->t3 -= numdone;
		if (rrec->t3 <= 0) rrec->stage ++;
	}
	if (rrec->stage >= 3 && nsmps) {
		do {
			*outi++ = 0.;
		} while (--nsmps);
	}

}

typedef struct {
	long t[4];
	double slope[4], level;
	int stage;
} LinSegRec, *LinSegRecPtr;

void linseg(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	LinSegRecPtr rrec;
	double *s1, *s2, *s3, *s4, *s5, *s6, *s7;
	long nsmps, numdone;
	int i;
	
	outi = this->out;
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;	/* level 1 */
		s2 = this->in[1]->out;	/* level 2 */
		s3 = this->in[2]->out;	/* level 3 */
		s4 = this->in[3]->out;	/* time 1 */
		s5 = this->in[4]->out;	/* time 2 */
		s6 = this->in[5]->out;	/* time 3 */
		s7 = this->in[6]->out;	/* time 4 */
		this->data = rrec = (LinSegRec*)allocate(sizeof(LinSegRec));
		rrec->level = 0.;
		rrec->t[0] = gSampleRate * *s4;
		rrec->t[1] = gSampleRate * *s5;
		rrec->t[2] = gSampleRate * *s6;
		rrec->t[3] = gSampleRate * *s7;
		rrec->slope[0] = *s1 / rrec->t[0];
		rrec->slope[1] = (*s2 - *s1) / rrec->t[1];
		rrec->slope[2] = (*s3 - *s2) / rrec->t[2];
		rrec->slope[3] = - *s3 / rrec->t[3];
		rrec->t[0] = smax(0, rrec->t[0]);
		rrec->t[1] = smax(0, rrec->t[1]);
		rrec->t[2] = smax(0, rrec->t[2]);
		rrec->t[3] = smax(0, rrec->t[3]);
		rrec->stage = 0;
	} else {
		rrec = (LinSegRec*)this->data;
	}
	nsmps = BUFLEN;
	for (i=rrec->stage; i<4 && nsmps; ++i) {
		if (rrec->stage == i && nsmps) {
			slope = rrec->slope[i];
			level = rrec->level;
			nsmp2 = numdone = smin(nsmps, rrec->t[i]);
			while (nsmp2--) {
				*outi++ = level;
				level += slope;
			}
			rrec->level = level;
			nsmps -= numdone;
			rrec->t[i] -= numdone;
			if (rrec->t[i] <= 0) rrec->stage ++;
		}
	}
	if (rrec->stage >= 4 && nsmps) {
		do {
			*outi++ = 0.;
		} while (--nsmps);
	}

}


void linseg0(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	LinSegRecPtr rrec;
	double *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9;
	long nsmps, numdone;
	int i;
	
	outi = this->out;
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;	/* level 0 */
		s2 = this->in[1]->out;	/* level 1 */
		s3 = this->in[2]->out;	/* level 2 */
		s4 = this->in[3]->out;	/* level 3 */
		s5 = this->in[4]->out;	/* level 4 */
		s6 = this->in[5]->out;	/* time 1 */
		s7 = this->in[6]->out;	/* time 2 */
		s8 = this->in[7]->out;	/* time 3 */
		s9 = this->in[8]->out;	/* time 4 */
		this->data = rrec = (LinSegRec*)allocate(sizeof(LinSegRec));
		rrec->level = *s1;
		rrec->t[0] = gSampleRate * *s6;
		rrec->t[1] = gSampleRate * *s7;
		rrec->t[2] = gSampleRate * *s8;
		rrec->t[3] = gSampleRate * *s9;
		rrec->slope[0] = (*s2 - *s1) / rrec->t[0];
		rrec->slope[1] = (*s3 - *s2) / rrec->t[1];
		rrec->slope[2] = (*s4 - *s3) / rrec->t[2];
		rrec->slope[3] = (*s5 - *s4) / rrec->t[3];
		rrec->t[0] = smax(0, rrec->t[0]);
		rrec->t[1] = smax(0, rrec->t[1]);
		rrec->t[2] = smax(0, rrec->t[2]);
		rrec->t[3] = smax(0, rrec->t[3]);
		rrec->stage = 0;
	} else {
		rrec = (LinSegRec*)this->data;
	}
	nsmps = BUFLEN;
	for (i=rrec->stage; i<4 && nsmps; ++i) {
		if (rrec->stage == i && nsmps) {
			slope = rrec->slope[i];
			level = rrec->level;
			nsmp2 = numdone = smin(nsmps, rrec->t[i]);
			while (nsmp2--) {
				*outi++ = level;
				level += slope;
			}
			rrec->level = level;
			nsmps -= numdone;
			rrec->t[i] -= numdone;
			if (rrec->t[i] <= 0) rrec->stage ++;
		}
	}
	if (rrec->stage >= 4 && nsmps) {
		do {
			*outi++ = 0.;
		} while (--nsmps);
	}

}

typedef struct {
	long t[4];
	double decay[4], level, y1;
	int stage;
} ADSRRec, *ADSRRecPtr;

void adsr(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	ADSRRecPtr rrec;
	double *s1, *s2, *s3, *s4, *s5, *s6;
	double endlvl, y1, b1;
	long nsmps, numdone;
	int i;
	
	outi = this->out;
	s1 = this->in[0]->out;	/* level 1 */
	s2 = this->in[1]->out;	/* level 2 */
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;	/* level 1 */
		s2 = this->in[1]->out;	/* level 2 */
		s3 = this->in[2]->out;	/* time 1 */
		s4 = this->in[3]->out;	/* time 2 */
		s5 = this->in[4]->out;	/* time 3 */
		s6 = this->in[5]->out;	/* time 4 */
		this->data = rrec = (ADSRRec*)allocate(sizeof(ADSRRec));
		rrec->level = 0.;
		rrec->y1 = 0.;
		rrec->t[0] = (long)(gSampleRate * *s3) & -16L;
		rrec->t[1] = (long)(gSampleRate * *s4) & -16L;
		rrec->t[2] = (long)(gSampleRate * *s5) & -16L;
		rrec->t[3] = 0x7fffffff;
		rrec->decay[0] = exp(log1  / (rrec->t[0] >> 4));
		rrec->decay[1] = exp(log01 / (rrec->t[1] >> 4));
		rrec->decay[2] = exp(log01 / (rrec->t[2] >> 4));
		rrec->decay[3] = exp(log01 / (gSampleRate/16. * *s6));
		rrec->t[0] = smax(0, rrec->t[0]);
		rrec->t[1] = smax(0, rrec->t[1]);
		rrec->t[2] = smax(0, rrec->t[2]);
		rrec->stage = 0;
	} else {
		rrec = (ADSRRec*)this->data;
	}
	y1 = rrec->y1;
	level = rrec->level;
	nsmps = BUFLEN;
	for (i=rrec->stage; nsmps; ++i) {
		if (rrec->stage == i && nsmps) {
			b1 = rrec->decay[i];
			if (rrec->stage == 0) endlvl = *s1/.9;
			else if (rrec->stage == 3) endlvl = 0.;
			else endlvl = *s2;
			nsmp2 = numdone = smin(nsmps, rrec->t[i]);
			while (nsmp2) {
				nsmp2 -= 16;
				y1 = endlvl + b1 * (y1 - endlvl); 
				slope = (y1 - level) / 16.;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
			}
			nsmps -= numdone;
			rrec->t[i] -= numdone;
			if (rrec->t[i] <= 0 && rrec->stage < 3) rrec->stage ++;
		}
	}
	rrec->level = level;
	rrec->y1 = y1;
}

typedef struct {
	long t[2];
	double decay[2], level, y1;
	int stage;
} ARRec, *ARRecPtr;

void ar(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	ARRecPtr rrec;
	double *s1, *s2, *s3;
	double endlvl, y1, b1;
	long nsmps, numdone;
	int i;
	
	outi = this->out;
	s1 = this->in[0]->out;	/* level 1 */
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;	/* level 1 */
		s2 = this->in[1]->out;	/* time 1 */
		s3 = this->in[2]->out;	/* time 2 */
		this->data = rrec = (ARRec*)allocate(sizeof(ARRec));
		rrec->level = 0.;
		rrec->y1 = 0.;
		rrec->t[0] = (long)(gSampleRate * *s2) & -16L;
		rrec->t[1] = 0x7fffffff;
		rrec->decay[0] = exp(log1  / (rrec->t[0] >> 4));
		rrec->decay[1] = exp(log01 / (gSampleRate/16. * *s3));
		rrec->t[0] = smax(0, rrec->t[0]);
		rrec->t[1] = smax(0, rrec->t[1]);
		rrec->stage = 0;
	} else {
		rrec = (ARRec*)this->data;
	}
	y1 = rrec->y1;
	level = rrec->level;
	nsmps = BUFLEN;
	for (i=rrec->stage; nsmps; ++i) {
		if (rrec->stage == i && nsmps) {
			b1 = rrec->decay[i];
			if (rrec->stage == 0) endlvl = *s1/.9;
			else endlvl = 0.;
			nsmp2 = numdone = smin(nsmps, rrec->t[i]);
			while (nsmp2) {
				nsmp2 -= 16;
				y1 = endlvl + b1 * (y1 - endlvl); 
				slope = (y1 - level) / 16.;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
			}
			nsmps -= numdone;
			rrec->t[i] -= numdone;
			if (rrec->t[i] <= 0 && rrec->stage < 1) rrec->stage ++;
		}
	}
	rrec->level = level;
	rrec->y1 = y1;
}


typedef struct {
	long t[3];
	double decay[2], level, y1;
	int stage;
} DARRec, *DARRecPtr;

void dar(UGen *this)		/* ramp generator */
{
	register double *outi, level, slope;
	register long nsmp2;
	DARRecPtr rrec;
	double *s1, *s2, *s3, *s4;
	double endlvl, y1, b1;
	long nsmps, numdone;
	int i;
	
	outi = this->out;
	s1 = this->in[0]->out;	/* level 1 */
	if (this->state == dfUninitted) {
		s1 = this->in[0]->out;	/* level 1 */
		s2 = this->in[1]->out;	/* time 1 */
		s3 = this->in[2]->out;	/* time 2 */
		s4 = this->in[3]->out;	/* time 3 */
		this->data = rrec = (DARRec*)allocate(sizeof(DARRec));
		rrec->level = 0.;
		rrec->y1 = 0.;
		rrec->t[0] = (long)(gSampleRate * *s2) & -16L;
		rrec->t[1] = (long)(gSampleRate * *s3) & -16L;
		rrec->t[2] = 0x7fffffff;
		rrec->decay[0] = exp(log1  / (rrec->t[0] >> 4));
		rrec->decay[1] = exp(log01 / (gSampleRate/16. * *s4));
		rrec->t[0] = smax(0, rrec->t[0]);
		rrec->t[1] = smax(0, rrec->t[1]);
		rrec->t[2] = smax(0, rrec->t[1]);
		rrec->stage = 0;
	} else {
		rrec = (DARRec*)this->data;
	}
	y1 = rrec->y1;
	level = rrec->level;
	nsmps = BUFLEN;
	for (i=rrec->stage; nsmps; ++i) {
		if (rrec->stage == i && nsmps) {
			b1 = rrec->decay[i];
			if (rrec->stage == 0) endlvl = 0.;
			else if (rrec->stage == 1) endlvl = *s1/.9;
			else endlvl = 0.;
			nsmp2 = numdone = smin(nsmps, rrec->t[i]);
			while (nsmp2) {
				nsmp2 -= 16;
				y1 = endlvl + b1 * (y1 - endlvl); 
				slope = (y1 - level) / 16.;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
				*outi++ = level; level += slope;
			}
			nsmps -= numdone;
			rrec->t[i] -= numdone;
			if (rrec->t[i] <= 0 && rrec->stage < 1) rrec->stage ++;
		}
	}
	rrec->level = level;
	rrec->y1 = y1;
}

typedef struct {
	float level;
} DopRec, *DopRecPtr;

void dopfreq(UGen *this)		/* doppler frequency control function - pron.:"dope freak" */
{
	register double *outi, level, slope, nxtlevel;
	register long nsmp2;
	double *s1, *s2, *s3, *s4, v2, t;
	long nsmps, numdone;
	DopRecPtr drec;
	
	/* in[0] speed (ft/sec), in[1] nearest approach distance, 
		in[2] time of nearest approach 
	*/
	if (this->state == dfUninitted) {
		this->data = drec = (DopRec*)allocate(sizeof(DopRec));
		drec->level = 0.;
		outi = this->out;
		s1 = this->in[0]->out;
		s2 = this->in[1]->out;
		s3 = this->in[2]->out;
		numdone = numout;
		v2 = *s1 * *s1;
		t = numdone / gSampleRate - *s3; 
		nxtlevel = 1128.3 / (1128.3 + (v2*t/sqrt(v2*t*t + *s2* *s2)));
	} else {
		outi = this->out;
		s1 = this->in[0]->out;
		s2 = this->in[1]->out;
		s3 = this->in[2]->out;
		drec = (DopRec*)this->data;
		nxtlevel = drec->level;
	}

	nsmps = BUFLEN>>4;
	numdone = numout;
	do {
		numdone += 16;
		v2 = *s1 * *s1;
		t = numdone / gSampleRate - *s3; 
		level = nxtlevel;
		nxtlevel = 1128.3 / (1128.3 + (v2*t/sqrt(v2*t*t + *s2* *s2)));
		slope = (nxtlevel - level) / 16.;
		nsmp2 = 16;
		while (nsmp2--) {
			level += slope;
			*outi++ = level;
		}
	} while (--nsmps);
	drec->level = level;
}

void dopamp(UGen *this)		/* doppler amplitude control function  */
{
	register double *outi, level, slope, nxtlevel;
	register long nsmp2;
	double *s1, *s2, *s3, *s4, v2, t;
	long nsmps, numdone;
	DopRecPtr drec;
	
	/* in[0] speed (ft/sec), in[1] nearest approach distance, 
		in[2] time of nearest approach 
	*/

	
	if (this->state == dfUninitted) {
		this->data = drec = (DopRec*)allocate(sizeof(DopRec));
		drec->level = 0.;
		outi = this->out;
		s1 = this->in[0]->out;
		s2 = this->in[1]->out;
		s3 = this->in[2]->out;
		numdone = numout;
		v2 = *s1 * *s1;
		t = numdone / gSampleRate - *s3; 
		nxtlevel = 400./(v2*t*t + *s2* *s2);
	} else {
		outi = this->out;
		s1 = this->in[0]->out;
		s2 = this->in[1]->out;
		s3 = this->in[2]->out;
		drec = (DopRec*)this->data;
		nxtlevel = drec->level;
	}

	nsmps = BUFLEN>>4;
	numdone = numout;
	do {
		numdone += 16;
		v2 = *s1 * *s1;
		t = numdone / gSampleRate - *s3; 
		level = nxtlevel;
		nxtlevel = 400./(v2*t*t + *s2* *s2);
		slope = (nxtlevel - level) / 16.;
		nsmp2 = 16;
		while (nsmp2--) {
			level += slope;
			*outi++ = level;
		}
	} while (--nsmps);
	drec->level = level;
}
