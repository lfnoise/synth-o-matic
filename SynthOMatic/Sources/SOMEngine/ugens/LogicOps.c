/*
 *  LogicOps.c
 *
 *  Ported from the original Synth-O-Matic source by scripts/port_ugens.py.
 *  Apart from the commented 'port:' fixes the code is as it was written.
 */

#include <math.h>
#include "som_internal.h"


void
andfn(UGen *this)
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
		value = *s1 && *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ && *s2++;
		} while (--nsmps);
	}
}

void
orfn(UGen *this)
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
		value = *s1 || *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ || *s2++;
		} while (--nsmps);
	}
}


void
eqfn(UGen *this)
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
		value = *s1 == *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ == *s2++;
		} while (--nsmps);
	}
}

void
noteqfn(UGen *this)
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
		value = *s1 != *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ != *s2++;
		} while (--nsmps);
	}
}

void
greaterfn(UGen *this)
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
		value = *s1 > *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ > *s2++;
		} while (--nsmps);
	}
}

void
greatereqfn(UGen *this)
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
		value = *s1 >= *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ >= *s2++;
		} while (--nsmps);
	}
}

void
lessfn(UGen *this)
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
		value = *s1 < *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ < *s2++;
		} while (--nsmps);
	}
}

void
lesseqfn(UGen *this)
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
		value = *s1 <= *s2;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = *s1++ <= *s2++;
		} while (--nsmps);
	}
}

void
notfn(UGen *this)
{
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
		value = !*s1;
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = !*s1++;
		} while (--nsmps);
	}
}


void
condfn(UGen *this)
{
	register double value, *outi, *s1, *s2, *s3;
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
		value = (*s1 ? *s2 : *s3);
		outi = this->out;
		nsmps = BUFLEN;
		do *outi++ = value; while (--nsmps);
	} else {
		outi = this->out;
		nsmps = BUFLEN;
		do {
			*outi++ = (*s1 ? *s2 : *s3);
			s1++; s2++; s3++;
		} while (--nsmps);
	}
}

