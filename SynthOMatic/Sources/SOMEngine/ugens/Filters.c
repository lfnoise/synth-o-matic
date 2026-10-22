/*
 *  Filters.c
 *
 *  Ported from the original Synth-O-Matic source by scripts/port_ugens.py.
 *  Apart from the commented 'port:' fixes the code is as it was written.
 */

#include "som_internal.h"
#include <math.h>


typedef struct {
	double y1, y2;
} FilterRec, *FilterRecPtr;

void lpf(UGen *this)		/* Butterworth Low Pass Filter */
{
	double a0, a1, a2, b1, b2, C, sqrt2C, C2, cfreq;
	register double y0, y1, y2;
	register double *outi, *s1, *s2;
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq */
	
	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
		y2 = frec->y2;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		cfreq = *s2;
		cfreq = sclip(.001, cfreq, nyqlim);
		C = 1.0 / tan(pisr * cfreq);
		C2 = C * C;
		sqrt2C = C * sqrt2;
		a0 = 1.0 / (1.0 + sqrt2C + C2);
		/* a1 = 2.0 * a0;  optimized out below */
		/* a2 = a0;        optimized out below */
		b1 = 2.0 * (1.0 - C2) * a0 ;
		b2 = (1.0 - sqrt2C + C2) * a0;
		
		s2 += 32;
		nsmps = 32;
		do {
			y0 = *s1++ - b1 * y1 - b2 * y2;
			*outi++ = a0 * (y0 + y1 + y1 + y2);
			y2 = y1;
			y1 = y0;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
	frec->y2 = y2;
}

void hpf(UGen *this)		/* Butterworth High Pass Filter */
{
	double a0, a1, a2, b1, b2, C, sqrt2C, C2, cfreq;
	register double y0, y1, y2;
	register double *outi, *s1, *s2;
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
		y2 = frec->y2;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		cfreq = *s2;
		cfreq = sclip(.001, cfreq, nyqlim);
		C = tan(pisr * cfreq);
		C2 = C * C;
		sqrt2C = C * sqrt2;
		a0 = 1.0 / (1.0 + sqrt2C + C2);
		/* a1 = -2.0 * a0;  optimized out below */
		/* a2 = a0;         optimized out below */
		b1 = -2.0 * (1.0 - C2) * a0 ;
		b2 = (1.0 - sqrt2C + C2) * a0;
		
		s2 += 32;
		nsmps = 32;
		do {
			y0 = *s1++ - b1 * y1 - b2 * y2;
			*outi++ = a0 * (y0 - y1 - y1 + y2);
			y2 = y1;
			y1 = y0;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
	frec->y2 = y2;
}

void bpf(UGen *this)		/* Butterworth Band Pass Filter */
{
	double a0, a1, a2, b1, b2, C, D, cfreq, bwfreq;
	register double y0, y1, y2;
	register double *outi, *s1, *s2, *s3;
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq, in[2] = bandwidth */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
		y2 = frec->y2;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		cfreq = *s2;
		cfreq = sclip(.001, cfreq, nyqlim);
		bwfreq = *s3;
		bwfreq = sclip(.001, bwfreq, nyqlim);
		C = 1. / tan(pisr * bwfreq);
		D = 2. * cos(pi2sr * cfreq);
		a0 = 1. / (1. + C);
		/* a1 = 0.;  optimized out below */
		/* a2 = -a0;  optimized out below */
		b1 = - C * D * a0 ;
		b2 = (C - 1.) * a0;
		
		s2 += 32;
		s3 += 32;
		nsmps = 32;
		do {
			y0 = *s1++ - b1 * y1 - b2 * y2;
			*outi++ = a0 * (y0 - y2);
			y2 = y1;
			y1 = y0;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
	frec->y2 = y2;
}

void brf(UGen *this)		/* Butterworth Band Reject Filter */
{
	double a0, a1, a2, b1, b2, C, D, ay, cfreq, bwfreq;
	register double y0, y1, y2;
	register double *outi, *s1, *s2, *s3;
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq, in[2] = bandwidth */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
		y2 = frec->y2;
	}
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		cfreq = *s2;
		cfreq = sclip(.001, cfreq, nyqlim);
		bwfreq = *s3;
		bwfreq = sclip(.001, bwfreq, nyqlim);
		C = tan(pisr * bwfreq);
		D = 2. * cos(pi2sr * cfreq);
		a0 = 1. / (1. + C);
		a1 = -D * a0;
		/* a2 = a0;  optimized out below */
		/* b1 = a1;  optimized out below */
		b2 = (1. - C) * a0;
		
		s2 += 32;
		s3 += 32;
		nsmps = 32;
		do {
			ay = a1 * y1;
			y0 = *s1++ - ay - b2 * y2;
			*outi++ = a0 * (y0 + y2) + ay;
			y2 = y1;
			y1 = y0;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
	frec->y2 = y2;
}

void reson(UGen *this)		/* Versatile Resonant Filter */
{
	double a0, b1, b2, cosf, c3t4, omc3, c2sqr, c3p1;
	register double y1, y2;
	register double *outi, *s3, *s1, *s2;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq, in[2] = bandwidth */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y2 = frec->y2;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		cosf = cos(pi2sr * *s3);
		b2 = exp(- pi2sr * *s2);
		c3p1 = b2 + 1.;
		c3t4 = b2 * 4.;
		omc3 = 1 - b2;
		b1 = c3t4 * cosf / c3p1;		
		c2sqr = b1 * b1;
		a0 = omc3 * sqrt(1. - c2sqr / c3t4); /* scale for narrow band */

		s2 += 32;
		s3 += 32;
		nsmps = 32;
		do {
			*outi = a0 * *s1++ + b1 * y1 - b2 * y2;
			y2 = y1;
			y1 = *outi++ ;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y2 = y2;
	frec->y1 = y1;
}


void tone(UGen *this)		/* First Order Low Pass Filter */
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		C = 2. - cos(pi2sr * *s2);
		b1 = C - sqrt(C * C - 1.);
		a0 = 1. - b1;

		s2 += 32;
		nsmps = 32;
		do {
			*outi++ = y1 = *s1 + b1 * (y1 - *s1); s1++;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
}

void atone(UGen *this)		/* Complement of tone */
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		C = 2. - cos(pi2sr * *s2);
		b1 = C - sqrt(C * C - 1.);
		a0 = 1. - b1;

		s2 += 32;
		nsmps = 32;
		do {
			*outi++ = y1 = b1 * (*s1 + y1);
			y1 -= *s1++;		/* y1 contains y1-x1 */
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
}

void areson(UGen *this)		/* Complement of reson */
{
	double a0, b1, b2, cosf, c3t4, omc3, c2sqr, c3p1;
	register double y1, y2;
	register double *outi, *s3, *s1, *s2;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = cutoff freq, in[2] = bandwidth */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
		y2 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y2 = frec->y2;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		cosf = cos(pi2sr * *s3);
		b2 = exp(- pi2sr * *s2);
		c3p1 = b2 + 1.;
		c3t4 = b2 * 4.;
		omc3 = 1 - b2;
		b1 = c3t4 * cosf / c3p1;		
		c2sqr = b1 * b1;
		a0 = 1. - omc3 * sqrt(1. - c2sqr / c3t4); /* scale for narrow band */

		s2 += 32;
		s3 += 32;
		nsmps = 32;
		do {
			*outi = a0 * *s1 + b1 * y1 - b2 * y2;
			y2 = y1;
			y1 = *outi++ - *s1++;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y2 = y2;
	frec->y1 = y1;
}

typedef struct {
	double a0, b1, inrms, refrms, adjust;
} BalanceRec;

void rms(UGen *this)
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	double inrms, outrms;
	register long nsmps;
	long nsmp2;
	BalanceRec *brec;
	
	/* in[0] = input */

	if (this->state == dfUninitted) {
		this->data = brec = (BalanceRec*)allocate(sizeof(BalanceRec));
		C = 2. - cos(pi2sr * 10.);
		b1 = brec->b1 = C - sqrt(C * C - 1.);
		a0 = brec->a0 = 1. - b1;
		inrms = 0.;
	} else {
		brec = (BalanceRec*)this->data;
		a0 = brec->a0;
		b1 = brec->b1;
		inrms  = brec->inrms;
	}
	s1 = this->in[0]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		nsmps = 32;
		do {
			inrms  = a0 * *s1 * *s1 + b1 * inrms;		s1++;
		} while (--nsmps);
		outrms = sqrt(inrms);
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
		*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;	*outi++ = outrms;
	} while (--nsmp2);
	brec->inrms = inrms;
}

void gain(UGen *this)
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	double inrms, oldadjust, newadjust, slope;
	register long nsmps;
	long nsmp2;
	BalanceRec *brec;
	
	/* in[0] = input, in[1] = desired rms gain */

	if (this->state == dfUninitted) {
		this->data = brec = (BalanceRec*)allocate(sizeof(BalanceRec));
		y1 = 0.0;
		C = 2. - cos(pi2sr * 10.);
		b1 = brec->b1 = C - sqrt(C * C - 1.);
		a0 = brec->a0 = 1. - b1;
		inrms = 0.;
		newadjust = 0.;
	} else {
		brec = (BalanceRec*)this->data;
		a0 = brec->a0;
		b1 = brec->b1;
		newadjust = brec->adjust;
		inrms  = brec->inrms;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		nsmps = 32;
		do {
			inrms  = a0 * *s1 * *s1 + b1 * inrms;		s1++;
		} while (--nsmps);
		s1 -= 32;
		oldadjust = newadjust;
		if (inrms == 0.) newadjust = *s2;
		else newadjust = *s2 / sqrt(inrms);
		s2 += 32;
		slope = (newadjust - oldadjust) / 32.;
		nsmps = 32;
		do {
			*outi++ = oldadjust * *s1++;
			oldadjust += slope;
		} while (--nsmps);
	} while (--nsmp2);
	brec->adjust = newadjust;
	brec->inrms = inrms;
}

void balance(UGen *this)
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	double inrms, refrms, oldadjust, newadjust, slope;
	register long nsmps;
	long nsmp2;
	BalanceRec *brec;
	
	/* in[0] = input, in[1] = reference signal */

	if (this->state == dfUninitted) {
		this->data = brec = (BalanceRec*)allocate(sizeof(BalanceRec));
		y1 = 0.0;
		C = 2. - cos(pi2sr * 10.);
		b1 = brec->b1 = C - sqrt(C * C - 1.);
		a0 = brec->a0 = 1. - b1;
		inrms = 0.;
		refrms = 0.;
		newadjust = 0.;
	} else {
		brec = (BalanceRec*)this->data;
		a0 = brec->a0;
		b1 = brec->b1;
		newadjust = brec->adjust;
		inrms  = brec->inrms;
		refrms = brec->refrms;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		nsmps = 32;
		do {
			inrms  = a0 * *s1 * *s1 + b1 * inrms;		s1++;
			refrms = a0 * *s2 * *s2 + b1 * refrms;		s2++;
		} while (--nsmps);
		s1 -= 32;
		oldadjust = newadjust;
		if (inrms == 0.) newadjust = sqrt(refrms);
		else newadjust = sqrt(refrms/inrms);
		slope = (newadjust - oldadjust) / 32.;
		nsmps = 32;
		do {
			*outi++ = oldadjust * *s1++;
			oldadjust += slope;
		} while (--nsmps);
	} while (--nsmp2);
	brec->adjust = newadjust;
	brec->inrms = inrms;
	brec->refrms = refrms;
}


void lag(UGen *this)		/* decay function */
{
	double C;
	register double a0, b1, y1;
	register double *outi, *s1, *s2;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = decay time */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		b1 = exp(log001 / (*s2 * gSampleRate));

		s2 += 32;
		nsmps = 32;
		do {
			*outi++ = y1 = *s1 + b1 * (y1 - *s1); s1++;
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
}

void lag2(UGen *this)		/* Attack and Decay */
{
	double C;
	register double a0, b1, b2, y1;
	register double *outi, *s1, *s2, *s3;
	
	register long nsmps;
	long nsmp2;
	FilterRecPtr frec;
	
	/* in[0] = input, in[1] = attack time, in[2] = decay time */

	if (this->state == dfUninitted) {
		this->data = frec = (FilterRec*)allocate(sizeof(FilterRec));
		y1 = 0.0;
	} else {
		frec = (FilterRecPtr)this->data;
		y1 = frec->y1;
	}
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	
	nsmp2 = BUFLEN>>5;
	do {
		
		b1 = exp(log001 / (*s2 * gSampleRate));
		b2 = exp(log001 / (*s3 * gSampleRate));

		s2 += 32;
		nsmps = 32;
		do {
			if (*s1 > y1) {
				*outi++ = y1 = *s1 + b1 * (y1 - *s1); s1++;
			} else {
				*outi++ = y1 = *s1 + b2 * (y1 - *s1); s1++;
			}
		} while (--nsmps);
	} while (--nsmp2);
	frec->y1 = y1;
}

