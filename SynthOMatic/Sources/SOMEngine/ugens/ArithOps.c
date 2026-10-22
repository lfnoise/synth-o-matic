/*
 *  ArithOps.c
 *
 *  Ported from the original Synth-O-Matic source by scripts/port_ugens.py.
 *  Apart from the commented 'port:' fixes the code is as it was written.
 */

#include <math.h>
#include "som_internal.h"

/* binary operations */

void
mulfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = *s1 * *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ * *s2++;
		} while (--nsmps);
	}
}

void
addfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = *s1 + *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ + *s2++;
		} while (--nsmps);
	}
}

void
subfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = *s1 - *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ - *s2++;
		} while (--nsmps);
	}
}

void
divfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		if (*s2 == 0.) value = 1e10;
		else value = *s1 / *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			if (*s2 == 0.) { *outi++ = 1e10; s1++; s2++; }
			else *outi++ = *s1++ / *s2++;
		} while (--nsmps);
	}
}

void
exponfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		if (*s1 < 0.) {
			value = -pow(-*s1, *s2);	/* it's quirky, but it work-y !? :-) */
		} else {
			value = pow(*s1, *s2);
		}
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			if (*s1 < 0.) {
				*outi++ = -pow(-*s1++,*s2++);	/* it's quirky, but it work-y !? :-) */
			} else {
				*outi++ = pow(*s1++,*s2++);
			}
		} while (--nsmps);
	}
}


void
minfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = smin(*s1, *s2);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = smin(*s1,*s2); s1++; s2++;
		} while (--nsmps);
	}
}


void
maxfn(UGen *this)
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = smax(*s1, *s2);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = smax(*s1,*s2); s1++; s2++;
		} while (--nsmps);
	}
}

typedef struct {	
	double previn, curout;
} SAHRec, *SAHRecPtr;

void
sah(UGen *this)
{
	register double value, *outi, *s1, *s2, previn, curout;
	register long nsmps;
	SAHRecPtr srec;
	
	if (this->state == dfUninitted) {
		this->data = srec = (SAHRecPtr)allocate(sizeof(SAHRec));
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
		srec->previn = -1.0;
		srec->curout = 0.0;
	} else {
		srec = (SAHRecPtr)this->data;
		
	}
	previn = srec->previn;
	curout = srec->curout;
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		if (previn <= 0. && *s2 > 0.) curout = *s1;
		*outi++ = curout;
		previn = *s2++;
		s1++;
	} while (--nsmps);
	srec->curout = curout;
	srec->previn = previn;
}


void
gate(UGen *this)
{
	register double value, *outi, *s1, *s2, curout;
	register long nsmps;
	SAHRecPtr srec;
	
	if (this->state == dfUninitted) {
		this->data = srec = (SAHRecPtr)allocate(sizeof(SAHRec));
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable) 
			this->type = sigTable;
		srec->curout = 0.0;
	} else {
		srec = (SAHRecPtr)this->data;
		
	}
	curout = srec->curout;
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		if (*s2++ > 0.) curout = *s1;
		*outi++ = curout; 
		s1++;
	} while (--nsmps);
	srec->curout = curout;
}

void
counter(UGen *this)	/* impulse counter */
{
	register double value, *outi, *s1, *s2, curout, previn;
	register long nsmps;
	SAHRecPtr srec;
	
	if (this->state == dfUninitted) {
		this->data = srec = (SAHRecPtr)allocate(sizeof(SAHRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		srec->curout = 0.0;
	} else {
		srec = (SAHRecPtr)this->data;
		
	}
	previn = srec->previn;
	curout = srec->curout;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		if (previn <= 0. && *s1 > 0.) curout += 1.;
		*outi++ = curout;
		previn = *s1++;
	} while (--nsmps);
	srec->curout = curout;
	srec->previn = previn;
}


void
countern(UGen *this)	/* modulo n counter */
{
	register double value, *outi, *s1, *s2, curout, previn;
	register long nsmps;
	SAHRecPtr srec;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		this->data = srec = (SAHRecPtr)allocate(sizeof(SAHRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		srec->curout = 0.0;
	} else {
		srec = (SAHRecPtr)this->data;
		
	}
	previn = srec->previn;
	curout = srec->curout;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		if (previn <= 0. && *s1 > 0.) {
			curout += 1.;
			if (*s2 > 0. && curout >= *s2) curout = 0.;
		}
		*outi++ = curout;
		previn = *s1++;
		s2++;
	} while (--nsmps);
	srec->curout = curout;
	srec->previn = previn;
}

void
divn(UGen *this)
{
	register double value, *outi, *s1, *s2, curout, previn;
	register long nsmps;
	SAHRecPtr srec;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	if (this->state == dfUninitted) {
		this->data = srec = (SAHRecPtr)allocate(sizeof(SAHRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		srec->curout =s2[0];
	} else {
		srec = (SAHRecPtr)this->data;
		
	}
	previn = srec->previn;
	curout = srec->curout;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		if (previn <= 0. && *s1 > 0.) {
			if (--curout == 0.) {
				*outi++ = 1.;
				curout = *s2;
			} else *outi++ = 0.;
		} else *outi++ = 0.;
		previn = *s1++;
		s2++;
	} while (--nsmps);
	srec->curout = curout;
	srec->previn = previn;
}

/* unary operations */

void
invertfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = -*s1;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = -*s1++;
		} while (--nsmps);
	}
}


void
absfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = sabs(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = sabs(*s1); s1++;
		} while (--nsmps);
	}
}


void
trigfn(UGen *this)		/* trigger function */
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = (*s1<=0.0) ? 0.0 : 1.0 ;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = (*s1<=0.0) ? 0.0 : 1.0 ; s1++;
		} while (--nsmps);
	}
}


void
sgnfn(UGen *this)		/* sign function */
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = (*s1<0.) ? -1. : ((*s1>0.) ? 1. : 0.);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = (*s1<0.) ? -1. : ((*s1>0.) ? 1. : 0.); s1++;
		} while (--nsmps);
	}
}


void
randfn(UGen *this)		/* random number function */
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	this->type = sigScalar;
	randnum = (IA * randnum + IC) % IM;
	value = *s1 * (randfac1 * randnum);
	outi = this->out;
	nsmps = BUFLEN;
	do *outi++ = value; while (--nsmps);
}

void
rand2fn(UGen *this)		/* random number function */
{
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	this->type = sigScalar;
	randnum = (IA * randnum + IC) % IM;
	value = *s1 + (*s2 - *s1) * (randfac1 * randnum);
	outi = this->out;
	nsmps = BUFLEN;
	do *outi++ = value; while (--nsmps);
}

typedef struct {
	double previn;
} DiffRec, *DiffRecPtr;

void
difffn(UGen *this)		/* differentiator */
{
	register double value, *outi, *s1, previn;
	register long nsmps;
	DiffRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (DiffRecPtr)allocate(sizeof(DiffRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->previn = 0.0;
	} else {
		drec = (DiffRecPtr)this->data;
	}
	previn = drec->previn;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		*outi++ = *s1 - previn;
		previn = *s1++;
	} while (--nsmps);
	drec->previn = previn;
}


void
sumfn(UGen *this)		/* 2 point sum */
{
	register double value, *outi, *s1, previn;
	register long nsmps;
	DiffRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (DiffRecPtr)allocate(sizeof(DiffRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->previn = 0.0;
	} else {
		drec = (DiffRecPtr)this->data;
	}
	previn = drec->previn;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		*outi++ = *s1 + previn;
		previn = *s1++;
	} while (--nsmps);
	drec->previn = previn;
}


void
avgfn(UGen *this)		/* 2 point average */
{
	register double value, *outi, *s1, previn;
	register long nsmps;
	DiffRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (DiffRecPtr)allocate(sizeof(DiffRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->previn = 0.0;
	} else {
		drec = (DiffRecPtr)this->data;
	}
	previn = drec->previn;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		*outi++ = .5 * (*s1 + previn);
		previn = *s1++;
	} while (--nsmps);
	drec->previn = previn;
}


void
delay1(UGen *this)		/* unit delay */
{
	register double value, *outi, *s1, previn;
	register long nsmps;
	DiffRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (DiffRecPtr)allocate(sizeof(DiffRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->previn = 0.0;
	} else {
		drec = (DiffRecPtr)this->data;
	}
	previn = drec->previn;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		*outi++ = previn;
		previn = *s1++;
	} while (--nsmps);
	drec->previn = previn;
}

typedef struct {
	double sum;
} IntegRec, *IntegRecPtr;

void
integfn(UGen *this)		/* integrator */
{
	register double value, *outi, *s1, sum;
	register long nsmps;
	IntegRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (IntegRecPtr)allocate(sizeof(IntegRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->sum = 0.0;
	} else {
		drec = (IntegRecPtr)this->data;
	}
	sum = drec->sum;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN;
	do {
		*outi++ = sum += *s1++;
	} while (--nsmps);
	drec->sum = sum;
}



void
intfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = floor(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = floor(*s1++);
		} while (--nsmps);
	}
}


void
fracfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = *s1 - floor(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1 - floor(*s1); s1++;
		} while (--nsmps);
	}
}


void
sinfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = sin(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = sin(*s1++);
		} while (--nsmps);
	}
}


void cosfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = cos(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = cos(*s1++);
		} while (--nsmps);
	}
}

void tanfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = tan(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = tan(*s1++);
		} while (--nsmps);
	}
}

void log10fn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = log10(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = log10(*s1++);
		} while (--nsmps);
	}
}

void logfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = log(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = log(*s1++);
		} while (--nsmps);
	}
}


void sqrtfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = sqrt(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = sqrt(*s1++);
		} while (--nsmps);
	}
}


void expfn(UGen *this)
{
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = exp(*s1);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = exp(*s1++);
		} while (--nsmps);
	}
}

void
midicps(UGen *this)
{			/* convert midi note numbers to cps */
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type == sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		/* midi note 57 is A 220Hz */
		value = 220. * pow(2., (*s1 - 57.)/12.);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = 220. * pow(2., (*s1++ - 57.)/12.);
		} while (--nsmps);
	}
}

void
cpsmidi(UGen *this)
{			/* convert cps to midi note numbers */
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type == sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		/* midi note 57 is A 220Hz */
		value = log(*s1 / 220.)/log(2.) * 12. + 57.;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = log(*s1++ / 220.)/log(2.) * 12. + 57.;
		} while (--nsmps);
	}
}

void
dbamp(UGen *this)
{			/* convert decibels to linear amplitude */
	register double value, *outi, *s1, *s2;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type == sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = pow(10., *s1/20.);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = pow(10., *s1++/20.);
		} while (--nsmps);
	}
}

void
ampdb(UGen *this)
{			/* convert linear amp to decibels */
	register double value, *outi, *s1;
	register long nsmps;
	
	s1 = this->in[0]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar) 
			this->type = sigScalar;
		else if (this->in[0]->type == sigTable) 
			this->type = sigTable;
	}
	if (this->type == sigScalar) {
		value = log(*s1)/log(10.) * 20.;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = log(*s1++)/log(10.) * 20.;
		} while (--nsmps);
	}
}


void flip(UGen *this)	/* spectral inverter */
{
	register double *outi, *s1;
	register long nsmps;
	
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigTable) this->type = sigTable;
	}
	
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN>>1;
	do {
		*outi++ = *s1++;
		*outi++ = -*s1++;
	} while (--nsmps);
}


void
flipsum(UGen *this)		/* spectral inverter w/ 2 point sum */
{
	register double *outi, *s1, previn;
	register long nsmps;
	DiffRecPtr drec;
	
	if (this->state == dfUninitted) {
		this->data = drec = (DiffRecPtr)allocate(sizeof(DiffRec));
		if (this->in[0]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type == sigTable) this->type = sigTable;
		drec->previn = 0.0;
	} else {
		drec = (DiffRecPtr)this->data;
	}
	previn = drec->previn;
	s1 = this->in[0]->out;
	outi = this->out;
	nsmps = BUFLEN>>1;
	do {
		*outi++ = previn + *s1;
		previn = *s1++;
		*outi++ = previn - *s1;
		previn = - *s1++;
	} while (--nsmps);
	drec->previn = previn;
}



/* ternary operators */


void
clipfn(UGen *this)
{
	register double value, *outi, *s1, *s2, *s3;
	register double a, b, c;
	register long nsmps;
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	if (this->state == dfUninitted) {
		if (this->in[0]->type == sigScalar && this->in[1]->type == sigScalar 
			&& this->in[2]->type == sigScalar) this->type = sigScalar;
		else if (this->in[0]->type <= sigTable && this->in[1]->type <= sigTable
		 && this->in[2]->type <= sigTable) this->type = sigTable;
	}
		
	if (this->type == sigScalar) {
		value = sclip(*s1, *s2, *s3);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			a = *s1++; b = *s2++; c = *s3++;
			*outi++ = sclip(a,b,c);
		} while (--nsmps);
	}
}

/* pan and cross fade */

void
pan2(UGen *this)
{
	register double *outi, *s1, *s2, *s3, *s4;
	register double leftpan, rightpan;
	register long nsmps, nsmp2;
	long panpos;
	
	/* in[0] = input, in[1] = panpos, in[2] = left out, in[3] = right out */
	/* panpos : -1 = left, 0 = center, 1 = right */
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	s4 = this->in[3]->out;
	outi = this->out;
	nsmps = BUFLEN>>5;
	do {
		panpos = 256. * (.5 + *s2 * .5);
		panpos = sclip(0, panpos, 256);
		rightpan = sine[panpos];
		leftpan  = sine[256 - panpos];
		nsmp2 = 32;
		do {
			*s3++ = leftpan  * *s1;
			*s4++ = rightpan * *s1;
			*outi++ = *s1++;
		} while (--nsmp2);
		s2 += 32;
	} while (--nsmps);
	pushcopy(this->in[2]);	this->in[2] = NULL;
	pushcopy(this->in[3]);	this->in[3] = NULL;
}


void
mixpan2(UGen *this)
{
	register double *outi, *s1, *s2, *s3, *s4;
	register double leftpan, rightpan;
	register long nsmps, nsmp2;
	long panpos;
	
	/* in[0] = input, in[1] = panpos, in[2] = left out, in[3] = right out */
	/* panpos : -1 = left, 0 = center, 1 = right */
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	s4 = this->in[3]->out;
	outi = this->out;
	nsmps = BUFLEN>>5;
	do {
		panpos = 256. * (.5 + *s2 * .5);
		panpos = sclip(0, panpos, 256);
		rightpan = sine[panpos];
		leftpan  = sine[256 - panpos];
		nsmp2 = 32;
		do {
			*s3++ += leftpan  * *s1;
			*s4++ += rightpan * *s1;
			*outi++ = *s1++;
		} while (--nsmp2);
		s2 += 32;
	} while (--nsmps);
	pushcopy(this->in[2]);	this->in[2] = NULL;
	pushcopy(this->in[3]);	this->in[3] = NULL;
}


void
xfade2(UGen *this)		/* cross fade between two inputs */
{
	register double *outi, *s1, *s2, *s3;
	register double leftpan, rightpan;
	register long nsmps, nsmp2;
	long panpos;
	
	/* in[0] = left in, in[1] = right in, in[2] = pan pos */
	/* panpos : -1 = left, 0 = center, 1 = right */
	
	s1 = this->in[0]->out;
	s2 = this->in[1]->out;
	s3 = this->in[2]->out;
	outi = this->out;
	nsmps = BUFLEN>>5;
	do {
		panpos = 256. * (.5 + *s3 * .5);
		panpos = sclip(0, panpos, 256);
		rightpan = sine[panpos];
		leftpan  = sine[256 - panpos];
		nsmp2 = 32;
		do {
			*outi++ = leftpan  * *s1++ + rightpan * *s2++;
		} while (--nsmp2);
		s3 += 32;
	} while (--nsmps);
}

