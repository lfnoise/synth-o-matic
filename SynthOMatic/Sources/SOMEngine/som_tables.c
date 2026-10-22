/*
 *  som_tables.c
 *
 *  The Fill and Alter menus, from the original FillTable.c, and the FFT from
 *  fft.c (Numerical Recipes).  The dialogs and the harmonic editor window are
 *  in the Swift front end; these functions do the arithmetic on a table.
 */

#include "som_internal.h"

#define FFTSWAP(a, b)  tempr=(a); (a)=(b); (b)=tempr

void
four1(double *data, int nn, int isign)
{
	int n, mmax, m, j, istep, i;
	double wtemp, wr, wpr, wpi, wi, theta;
	double tempr, tempi;

	n = nn << 1;
	j = 1;
	for (i=1; i<n; i+=2) {
		if (j > i) {
			FFTSWAP(data[j], data[i]);
			FFTSWAP(data[j+1], data[i+1]);
		}
		m = n >> 1;
		while (m >= 2 && j  >m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	mmax = 2;
	while (n > mmax) {
		istep = mmax << 1;
		theta = 6.28318530717958647692 / (isign * mmax);
		wtemp = sin(0.5 * theta);
		wpr = -2.0 * wtemp * wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for (m=1; m<mmax; m+=2) {
			for (i=m; i<=n; i+=istep) {
				j = i+mmax;
				tempr = wr * data[j  ] - wi * data[j+1];
				tempi = wr * data[j+1] + wi * data[j  ];
				data[j  ] = data[i  ] - tempr;
				data[j+1] = data[i+1] - tempi;
				data[i  ] += tempr;
				data[i+1] += tempi;
			}
			wtemp = wr;
			wr = wr * wpr - wi    * wpi + wr;
			wi = wi * wpr + wtemp * wpi + wi;
		}
		mmax = istep;
	}
}

void
magphase(double *data, int n, int isign)
{
	int i;
	double d1, d2;

	if (isign == 1) {
		for (i=3; i<=n; i+=2) {
			d1 = data[i];
			d2 = data[i+1];
			data[i  ] = sqrt(d1*d1 + d2*d2);
			data[i+1] = atan2(d2, d1);
		}
	} else {
		for (i=3; i<=n; i+=2) {
			d1 = data[i];
			d2 = data[i+1];
			data[i  ] = d1 * cos(d2);
			data[i+1] = d1 * sin(d2);
		}
	}
}

/* ---------------------------------------------------------------------- */

static Variable *
FindTable(SOMContext *ctx, int inx) {
	SOM_ENTER(ctx);
	if (inx < 0 || inx >= numvars) return NULL;
	if (vars[inx].type != sigTable && vars[inx].type != sigEnvelope) return NULL;
	if (!vars[inx].ac) return NULL;
	return vars + inx;
}

void
som_analyze_harmonics(SOMContext *ctx, int inx, unsigned char ampl[32], unsigned char phas[32]) {
	int i, j;
	double maxmag, pscale, scale, px;
	float *buf;
	long k, phasx;
	double fftbuf[1028];
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;

	buf = v->ac;
	for (i=0,j=1; i<BUFLEN; ++i,j+=2) {
		fftbuf[j] = buf[i];
		fftbuf[j+1] = 0.;
	}
	four1(fftbuf, BUFLEN, 1);
	magphase(fftbuf, BUFLEN, 1);

	maxmag = 0.;
	for (i=0,j=3; i<32; ++i, j+=2) {
		if (fftbuf[j] > maxmag) maxmag = fftbuf[j];
	}
	maxmag = sqrt(maxmag);
	scale = (maxmag==0.) ? 0. : 99./maxmag;
	pscale = 100. / twopi;
	for (i=0,j=3; i<32; ++i, j+=2) {
		k = (long)floor(scale * sqrt(fftbuf[j]) + .5);
		ampl[i] = sclip(0, k, 99);
		if (ampl[i]) {
			px = pi2-fftbuf[j+1];
			if ((i+1)&1) px += pi;
			phasx = (long)floor(pscale * px + .5);
			while (phasx > 99) phasx -= 100;
			while (phasx <  0) phasx += 100;
			phas[i] = phasx;
		} else phas[i] = 0;
	}
}

static int
ChebyPowerFill(Variable *table) {
	char name[VARNAMELEN+4];
	int i, j, k;
	Variable *v;
	double sum, zeropt, x, y;
	float *buf, *buf2;

	snprintf(name, sizeof(name), "%sps", table->name);
	for (i=0; i<numvars; ++i) {
		if (strcmp(name, vars[i].name)==0) break;
	}
	if (i==numvars) return -1;
	v = vars + i;
	if ((v->type != sigTable && v->type != sigEnvelope) || !v->ac) return -1;

	buf = v->ac;
	buf2 = table->ac;
	for (i=0; i<BUFLEN; ++i) {
		buf[i] = 0.0;
	}

	sum = 0.0;
	zeropt = (buf2[255] + buf2[256])/2.;
	for (j=1,k=1; j<=256; j++,k+=2) {
		x = buf2[255+j] - zeropt;
		y = buf2[256-j] - zeropt;
		sum += x*x + y*y;
		if (sum==0.0) buf[k] = 10000.;
		else {
			buf[k] = 1./sqrt(sum/j/2.);
			if (buf[k] > 10000.0) buf[k] = 10000.;
		}
	}
	for(i=2; i<512; i+=2) {
		buf[i] = (buf[i-1]+buf[i+1])/2.;
	}
	buf[0] = buf[1];
	buf[512] = buf[511];
	return (int)(v - vars);
}

int
som_cheby_fill(SOMContext *ctx, int inx, const unsigned char ampl[32], const unsigned char phas[32]) {
	double amp, maxval, x, inc, offset;
	int i, j;
	float *buf;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return -1;
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		*buf++ = 0.0;
	}
	inc = 1/255.5;
	for (i=1; i<32; i++) {
		buf = v->ac;
		amp = ampl[i-1];
		if (phas[i-1] >= 50) amp = -amp;
		if (i & 2) amp = -amp;
		if (amp != 0.0) {
			x = -1.0;
			for (j=0; j<BUFLEN; ++j,x+=inc) {
				*buf++ += amp * cos(i * acos(sclip(-1., x, 1.)));
			}
		}
	}
	maxval = 0.0;
	buf = v->ac;
	offset = (buf[255] + buf[256])/2.;
	for (i=0; i<BUFLEN; ++i) {
		x = *buf - offset;
		x = (x < 0.) ? -x : x;
		if (x > maxval) maxval = x;
		buf ++;
	}
	amp = (maxval == 0.) ? 0. : 1.0 / maxval;	/* port: guard all zero */
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		*buf = (*buf - offset) * amp;
		buf++;
	}
	buf = v->ac;
	buf[512] = buf[511];
	return ChebyPowerFill(v);
}

void
som_sine_fill(SOMContext *ctx, int inx, const unsigned char ampl[32], const unsigned char phas[32]) {
	double amp, minval, maxval;
	int inc, i, j, k;
	float *buf;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		*buf++ = 0.0;
	}
	for (i=1; i<=32; i++) {
		buf = v->ac;
		amp = ampl[i-1];
		amp = amp * amp;
		inc = i<<1;
		k = phas[i-1] * 1024./100.;
		if (i&1) k += 512;
		for (j=0; j<BUFLEN; ++j,k+=inc) {
			while (k >= 1024) k -= 1024;
			*buf++ += amp * sine[k];
		}
	}

	minval = maxval = 0.0;
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		if (*buf < minval) minval = *buf;
		if (*buf > maxval) maxval = *buf;
		buf ++;
	}
	amp = smax(-minval,maxval);
	amp = (amp == 0.) ? 0. : 1.0 / amp;		/* port: guard all zero */
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		*buf++ *= amp;
	}
	buf = v->ac;
	buf[512] = buf[0];
}

/* ---------------------------------------------------------------------- */
/* the Alter menu                                                         */

static void
SquashTable(Variable *v, int how) {
	int i;
	float *buf;
	double x, minval, maxval, midpt, scale1, scale2;

	minval = v->lolim;
	maxval = v->hilim;
	if (maxval == minval) return;
	midpt = (maxval+minval)/2.;
	scale1 = 2./(maxval-minval);
	scale2 = 1./scale1;
	buf = v->ac;
	for (i=0; i<BUFLEN; ++i) {
		x = (buf[i] - midpt) * scale1;
		switch (how) {
			case SOMAlterSquashUp :
				x = ((2. * sin(pi4 * (x+1.)) - 1.) + x + x)/3.;
				break;
			case SOMAlterSquashDown :
				x = ((2. * sin(pi4 * (x-1.)) + 1.) + x + x)/3.;
				break;
			case SOMAlterSquashOut :
				x = (sin(pi2 * x) + x + x)/3.;
				break;
			case SOMAlterSquashIn :
				if (x > 0.) {
					x = ((sin(pi2 * (x-1.)) + 1.) + x + x)/3.;
				} else {
					x = ((sin(pi2 * (x+1.)) - 1.) + x + x)/3.;
				}
				break;
		}
		buf[i] = x * scale2 + midpt;
	}
	buf[512] = buf[0];
}

static void
SmoothTable(Variable *v) {
	int i, j, i1, i2, i3, i4, i5, i6, i7;
	float *buf, bufx[BUFLEN];
	double sqrt24, a1, a2, a3, a4, x;

	sqrt24 = .25 * sqrt(2.);
	a1 = .5 - sqrt24;
	a2 = .5;
	a3 = .5 + sqrt24;
	a4 = 1.;
	x = 1/ (2. * (a1 + a2 + a3) + a4);
	a1 *= x;	a2 *= x;	a3 *= x;	a4 *= x;
	buf = v->ac;
	for (j=0; j<4; ++j) {
		if (v->type == sigTable) {
			for (i=0; i<BUFLEN; ++i) {
				i1 = (i - 3) & (BUFLEN - 1);
				i2 = (i - 2) & (BUFLEN - 1);
				i3 = (i - 1) & (BUFLEN - 1);
				i4 = i;
				i5 = (i + 1) & (BUFLEN - 1);
				i6 = (i + 2) & (BUFLEN - 1);
				i7 = (i + 3) & (BUFLEN - 1);
				bufx[i] = a1 * (buf[i1] + buf[i7])
						+ a2 * (buf[i2] + buf[i6])
						+ a3 * (buf[i3] + buf[i5])
						+ a4 * buf[i4];

			}
		} else {
			for (i=0; i<BUFLEN; ++i) {
				i1 = sclip(0, i - 3, BUFLEN - 1);
				i2 = sclip(0, i - 2, BUFLEN - 1);
				i3 = sclip(0, i - 1, BUFLEN - 1);
				i4 = i;
				i5 = sclip(0, i + 1, BUFLEN - 1);
				i6 = sclip(0, i + 2, BUFLEN - 1);
				i7 = sclip(0, i + 3, BUFLEN - 1);
				bufx[i] = a1 * (buf[i1] + buf[i7])
						+ a2 * (buf[i2] + buf[i6])
						+ a3 * (buf[i3] + buf[i5])
						+ a4 * buf[i4];

			}
		}
		for (i=0; i<BUFLEN; ++i) buf[i] = bufx[i];
	}
	buf[512] = buf[0];
}

static void
SpectralAnalysis(Variable *v) {
	int i, j;
	float *buf, maxmag;
	double fftbuf[1028];

	buf = v->ac;
	for (i=0,j=1; i<BUFLEN; ++i,j+=2) {
		fftbuf[j] = buf[i];
		fftbuf[j+1] = 0.;
	}
	four1(fftbuf, BUFLEN, 1);
	magphase(fftbuf, BUFLEN, 1);
	for (i=0,j=1; i<BUFLEN; i+=2,j+=2) {
		buf[i  ] = buf[i+1] = fftbuf[j];
	}
	maxmag = 0.;
	for (i=0; i<BUFLEN; ++i) {
		if (buf[i] > maxmag) {
			maxmag = buf[i];
		}
	}
	for (i=0; i<BUFLEN; ++i) {
		if (buf[i] <= 0.) {
			buf[i] = -100.;
		} else {
			buf[i] = 20.*log10(buf[i]/maxmag);
			buf[i] = smax(-100., buf[i]);
		}
	}
	buf[512] = buf[0];
	AutoScale(v);
}

void
som_alter_table(SOMContext *ctx, int inx, SOMAlterOp op, const float *clip) {
	int i;
	float *buf, temp;
	double coef, minval, maxval, phase, inc;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	buf = v->ac;
	switch (op) {
		case SOMAlterReverse :
			for (i=1; i<BUFLEN>>1; ++i) {
				temp = buf[i];
				buf[i] = buf[511-i];
				buf[511-i] = temp;
			}
			break;
		case SOMAlterInvert :
			for (i=0; i<BUFLEN; ++i) {
				buf[i] = -buf[i];
			}
			buf[512] = buf[0];
			AutoScale(v);
			break;
		case SOMAlterAbs :
			for (i=0; i<BUFLEN; ++i) {
				buf[i] = sabs(buf[i]);
			}
			buf[512] = buf[0];
			break;
		case SOMAlterClipNegative :
			for (i=0; i<BUFLEN; ++i) {
				buf[i] = smax(0., buf[i]);
			}
			buf[512] = buf[0];
			break;
		case SOMAlterSquare :
			for (i=0; i<BUFLEN; ++i) {
				buf[i] = buf[i] * buf[i];
			}
			buf[512] = buf[0];
			break;
		case SOMAlterAdd :
			if (!clip) return;
			for (i=0; i<=BUFLEN; ++i) {
				buf[i] = buf[i] + clip[i];
			}
			break;
		case SOMAlterMultiply :
			if (!clip) return;
			for (i=0; i<=BUFLEN; ++i) {
				buf[i] = buf[i] * clip[i];
			}
			break;
		case SOMAlterSquashUp :
		case SOMAlterSquashDown :
		case SOMAlterSquashIn :
		case SOMAlterSquashOut :
			SquashTable(v, op);
			break;
		case SOMAlterHanning :
			phase = 0.;
			inc = twopi/512.;
			for (i=0; i<BUFLEN; ++i, phase+=inc) {
				buf[i] = (-.5 * cos(phase) + .5) * buf[i] ;
			}
			buf[512] = buf[0];
			break;
		case SOMAlterWelch :
			phase = 0.;
			inc = pi/512.;
			for (i=0; i<BUFLEN; ++i, phase+=inc) {
				buf[i] = sin(phase) * buf[i] ;
			}
			buf[512] = buf[0];
			break;
		case SOMAlterParzen :
			phase = 0.;
			inc = 1./256.;
			buf[0] = 0.;
			for (i=1; i<BUFLEN>>1; ++i, phase+=inc) {
				buf[i] *= phase ;
				buf[512-i] *= phase;
			}
			buf[512] = buf[0];
			break;
		case SOMAlterNormalize :
			FindExactLimits(v, &minval, &maxval);
			maxval = sabs(maxval);
			minval = sabs(minval);
			maxval = smax(minval, maxval);
			if (maxval != 0.) {
				coef = 1. / maxval;
			} else coef = 1.;
			for (i=0; i<BUFLEN; ++i) {
				buf[i] = buf[i] * coef;
			}
			buf[512] = buf[0];
			v->lolim = -1.;
			v->hilim = 1.;
			v->stepsiz = 1.;
			break;
		case SOMAlterSmooth :
			SmoothTable(v);
			break;
		case SOMAlterAnalyze :
			SpectralAnalysis(v);
			break;
	}
}

/* ---------------------------------------------------------------------- */
/* the Fill menu                                                          */

void
som_fill_fm(SOMContext *ctx, int inx, double cfreq, double mfreq, double fminx) {
	int i;
	float *buf;
	double cphase, mphase, inc, minc, cinc;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	buf = v->ac;
	cphase = mphase = 0.;
	inc = twopi/512.;
	cinc = cfreq * inc;
	minc = mfreq * inc;
	for (i=0; i<BUFLEN; ++i) {
		buf[i] = sin(cphase + fminx * sin(mphase));
		cphase += cinc;
		mphase += minc;
	}
	buf[512] = buf[0];
}

void
som_fill_gbuzz(SOMContext *ctx, int inx, double n, double k, double r) {
	int i;
	float *buf;
	double inc, twor, rsqp1, rtn, rtnp1,km1, kpn, kpnm1, rsumr, phase;
	double num, den, absr;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	buf = v->ac;
	phase = pi;
	inc = twopi/512.;
	twor = 2. * r;
	rsqp1 = r*r + 1;
	rtn = pow(r, n);
	rtnp1 = rtn * r;
	km1 = k - 1.;
	kpn = k + n;
	kpnm1 = kpn - 1;
	if ((absr = fabs(r)) > .9999 && absr < 1.0001)
		rsumr = 1. / n;
	else rsumr = (1 - absr) / (1 - fabs(rtn));
	for (i=0; i<BUFLEN; ++i) {
		num = cos(phase * k) - r * cos(phase * km1)
		    - rtn * cos(phase * kpn) + rtnp1 * cos(phase * kpnm1);
		den = rsqp1 - twor * cos(phase);
		if (den > .00001 || den < -.00001)
			buf[i] = num / den * rsumr;
		else buf[i] = 1.;
		phase += inc;
	}
	buf[512] = buf[0];
}

void
som_fill_grain_env(SOMContext *ctx, int inx, int atk, double dur) {
	int i, nsmps;
	float *buf;
	double phase, inc;
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	buf = v->ac;
	phase = 0.;
	nsmps = ((double)BUFLEN - (double)BUFLEN*dur/100.)/2.;
	nsmps = sclip(1, nsmps, BUFLEN>>1);

	switch (atk) {
		case 0 :	/* Hanning */
			inc = pi/nsmps;
			for (i=0; i<nsmps; ++i) {
				buf[i] = buf[BUFLEN-i] = .5 - .5 * cos(phase);
				phase += inc;
			}
			for (; i<=BUFLEN>>1; ++i) {
				buf[i] = buf[BUFLEN-i] = 1.;
			}
			break;
		case 1 : 	/* Welch */
			inc = pi2/nsmps;
			for (i=0; i<nsmps; ++i) {
				buf[i] = buf[BUFLEN-i] = sin(phase);
				phase += inc;
			}
			for (; i<=BUFLEN>>1; ++i) {
				buf[i] = buf[BUFLEN-i] = 1.;
			}
			break;
		case 2 :	/* Parzen */
			inc = 1./nsmps;
			for (i=0; i<nsmps; ++i) {
				buf[i] = buf[BUFLEN-i] = phase;
				phase += inc;
			}
			for (; i<=BUFLEN>>1; ++i) {
				buf[i] = buf[BUFLEN-i] = 1.;
			}
			break;
	}
}

void
som_comb_table(SOMContext *ctx, int inx, int even, double teeth) {
	int i, tooth;
	float *buf;
	float bufx[BUFLEN];
	Variable *v;

	if (!(v = FindTable(ctx, inx))) return;
	if (teeth <= 0.) teeth = 1.;
	tooth = BUFLEN/teeth;
	tooth = sclip(1, tooth, BUFLEN>>1);
	buf = v->ac;
	if (even) {
		for (i=0; i<BUFLEN; ++i) {
			bufx[i] = buf[i] + buf[(i+tooth)&(BUFLEN-1)];
		}
	} else {
		for (i=0; i<BUFLEN; ++i) {
			bufx[i] = buf[i] - buf[(i+tooth)&(BUFLEN-1)];
		}
	}
	for (i=0; i<BUFLEN; ++i) {
		buf[i] = bufx[i];
	}
	buf[512] = buf[0];
}
