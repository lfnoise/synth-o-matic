/*
 *  stubs.c
 *
 *  Unit generators that the original listed as "NOT YET DONE", from stubs.c.
 *  They produce silence.
 */

#include "som_internal.h"

static void
silence(UGen *this)
{
	this->type = sigScalar;
	memset(this->out, 0, BUFLEN * sizeof(double));	/* port: was left unwritten */
}

void	buzz(UGen *this) { silence(this); }
void	pluck(UGen *this) { silence(this); }
void	pitchshift(UGen *this) { silence(this); }
void	vocoder(UGen *this) { silence(this); }
void	envfollow(UGen *this) { silence(this); }
void	pchfollow(UGen *this) { silence(this); }
