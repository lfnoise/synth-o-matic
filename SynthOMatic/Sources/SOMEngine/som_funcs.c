/*
 *  som_funcs.c
 *
 *  The opcode and function tables, from the original rpnlang.h.
 */

#include "som_internal.h"

Opcode codes[opNumOps] = {
	{ opDrop,		"Drop",			noop,		1, 	1},
	{ opOver,		"Over",			overfn,		2, 	1},
	{ opAssign,		"Assign",		assignfn,	1, 	1},
	{ opAssignVec,	"AssignVec",	assignvec,	2, 	1},
	{ opPushCon,	"PushCon",		pushcon,	0, 	0},
	{ opPushVar,	"PushVar",		pushvar,	0, 	1},
	{ opPushVec,	"PushVec",		pushvec,	1, 	0},
	{ opFuncCall,	"FuncCall",		NULL,		0, 	0},
	{ opAdd,		"Add",			addfn,		2, 	0},
	{ opSubtract,	"Subtract",		subfn,		2, 	0},
	{ opMultiply,	"Multiply",		mulfn,		2, 	0},
	{ opDivide,		"Divide",		divfn,		2, 	0},
	{ opExpon,		"Expon",		exponfn,	2, 	0},
	{ opInvert,		"Invert",		invertfn,	1, 	0},
	{ opNot,		"Not",			notfn,		1, 	0},
	{ opGreater,	"Greater",		greaterfn,	2, 	0},
	{ opLess,		"Less",			lessfn,		2, 	0},
	{ opGreaterEq,	"GreaterEq",	greatereqfn,2, 	0},
	{ opLessEq,		"LessEq",		lesseqfn,	2, 	0},
	{ opEqual,		"Equal",		eqfn,		2, 	0},
	{ opNotEqual,	"NotEqual",		noteqfn,	2, 	0},
	{ opAnd,		"And",			andfn,		2, 	0},
	{ opOr,			"Or",			orfn,		2, 	0},
	{ opConditional,"Conditional",	condfn,		3, 	0},
	{ opIf,			"If",			iffn,		1,	1},
	{ opElse,		"Else",			elsefn,		0,	1},
	{ opEndIf,		"EndIf",		endiffn,	0,	1},
	{ opEndIfElse,	"EndIfElse",	endifelse,	0,	1},
	{ opRevSubtract,"RevSubtract",	rsubfn,		2, 	0},
	{ opRevDivide,	"RevDivide",	rdivfn,		2, 	0},
	{ opRevExpon,	"RevExpon",		rexponfn,	2, 	0},
	{ opEnd,		"End",			NULL,		0,	0}
};

Function funcs[] = {
	{ "abs",		absfn,		1,		0x0000},
	{ "diff",		difffn,		1,		0x0000},
	{ "integ",		integfn,	1,		0x0000},
	{ "sum",		sumfn,		1,		0x0000},
	{ "avg",		avgfn,		1,		0x0000},
	{ "trig",		trigfn,		1,		0x0000},
	{ "sgn",		sgnfn,		1,		0x0000},
	{ "rand",		randfn,		1,		0x0000},
	{ "rand2",		rand2fn,	2,		0x0000},

	{ "sah",		sah,		2,		0x0000},
	{ "gate",		gate,		2,		0x0000},
	{ "int",		intfn,		1,		0x0000},
	{ "frac",		fracfn,		1,		0x0000},
	{ "clip",		clipfn,		3,		0x0000},

	{ "min",		minfn,		2,		0x0000},
	{ "max",		maxfn,		2,		0x0000},
	{ "sin",		sinfn,		1,		0x0000},
	{ "cos",		cosfn,		1,		0x0000},
	{ "tan",		tanfn,		1,		0x0000},

	{ "log",		logfn,		1,		0x0000},
	{ "log10",		log10fn,	1,		0x0000},
	{ "sqrt",		sqrtfn,		1,		0x0000},
	{ "exp",		expfn,		1,		0x0000},
	{ "cpsmidi",	cpsmidi,	1,		0x0000},
	{ "midicps",	midicps,	1,		0x0000},
	{ "dbamp",		dbamp,		1,		0x0000},
	{ "ampdb",		ampdb,		1,		0x0000},
	{ "flip",		flip,		1,		0x0000},
	{ "flipsum",	flipsum,	1,		0x0000},

	{ "sinosc",		sinosc,		2,		0x0000},
	{ "oscil",		oscil,		3,		0x0000},
	{ "oscili",		oscili,		3,		0x0000},
	{ "poscil",		poscil,		3,		0x0000},
	{ "poscili",	poscili,	3,		0x0000},
	{ "pmoscil",	pmoscil,	3,		0x0000},
	{ "coscil",		coscil,		3,		0x0000},
	{ "coscili",	coscili,	3,		0x0000},
	{ "coscil2",	coscil2,	4,		0x0000},
	{ "coscili2",	coscili2,	4,		0x0000},
	{ "soscil",		soscil,		4,		0x0000},
	{ "xcoscil",	xcoscil,	5,		0x0000},
	{ "woscil",		woscil,		4,		0x0000},
	{ "wcoscil",	wcoscil,	5,		0x0000},
	{ "vosim",		vosim,		3,		0x0000},
	{ "wavelets",	wavelets,	4,		0x0000},
	{ "grains",		grains,		6,		0x0000},

	{ "lfo",		lfoa,		4,		0x0000},
	{ "sinlfo",		sinlfoa,	3,		0x0000},
	{ "oscil1",		oscil1a,	3,		0x0000},
/*
	{ "bpenv",		bpenv,		1,		0x0000},
	{ "bpenv2",		bpenv2,		3,		0x0000},
*/
	{ "saw",		saw,		2,		0x0000},
	{ "square",		square,		2,		0x0000},
	{ "square0",	square0,	2,		0x0000},
	{ "triang",		triang,		2,		0x0000},
	{ "blip",		blip,		2,		0x0000},
	{ "pulse",		pulse,		3,		0x0000},
	{ "pulse0",		pulse0,		3,		0x0000},
	{ "ramp",		ramp,		2,		0x0000},
	{ "linen",		linen,		4,		0x0000},
	/* port: documented in the quick reference and used by the flute patch,
	   but missing from the final table */
	{ "linseg",		linseg,		7,		0x0000},
	{ "linseg0",	linseg0,	9,		0x0000},
	{ "bpenv",		bpenv2,		3,		0x0000},
	{ "adsr",		adsr,		6,		0x0000},
	{ "ar",			ar,			3,		0x0000},
	{ "dopfreq",	dopfreq,	3,		0x0000},
	{ "dopamp",		dopamp,		3,		0x0000},

	{ "noise0",		noise0,		2,		0x0000},
	{ "noise1",		noise1,		2,		0x0000},
	{ "noise",		noise2,		2,		0x0000},
	{ "buzz",		buzz,		1,		0x0000},
	{ "pluck",		pluck,		1,		0x0000},
	{ "string",		ksstring,	3,		0x0000},
	{ "pipe",		kspipe,		3,		0x0000},

	{ "lpf",		lpf,		2,		0x0000},
	{ "hpf",		hpf,		2,		0x0000},
	{ "bpf",		bpf,		3,		0x0000},
	{ "brf",		brf,		3,		0x0000},

	{ "reson",		reson,		3,		0x0000},
	{ "areson",		areson,		3,		0x0000},
	{ "tone",		tone,		2,		0x0000},
	{ "atone",		atone,		2,		0x0000},
	{ "lag",		lag,		2,		0x0000},
	{ "lag2",		lag2,		3,		0x0000},

	{ "rms", 		rms, 		1,		0x0000},
	{ "gain", 		gain,		2,		0x0000},
	{ "balance",	balance,	2,		0x0000},
	{ "shaper",		shaper,		2,		0x0000},
	{ "tablei",		tablei,		2,		0x0000},

	{ "flange",		flange,		3,		0x0000},
	{ "flangei",	flangei,	3,		0x0000},
	{ "delay",		delay,		2,		0x0000},
	{ "delayi",		delayi,		2,		0x0000},
	{ "delay1",		delay1,		1,		0x0000},
	{ "comb",		comb,		3,		0x0000},
	{ "echo",		echo,		3,		0x0000},
	{ "echoi",		echoi,		3,		0x0000},
	{ "lpcomb",		lpcomb,		4,		0x0000},
	{ "allpass",	allpass,	3,		0x0000},
	{ "reverb",		reverb,		2,		0x0000},
	{ "reverb2",	reverb2,	2,		0x0000},
	{ "taps",		multitap,	10,		0x0000},
	{ "pan",		pan2,		4,		0x0003},
	{ "panx",		mixpan2,	4,		0x0003},
	{ "xfade",		xfade2,		3,		0x0000},

	{ "pitchshift",	pitchshift,	1,		0x0000},
	{ "vocoder",	vocoder,	1,		0x0000},

	{ "envfollow",	envfollow,	1,		0x0000},
	{ "pchfollow",	pchfollow,	1,		0x0000},
};

int fnNumFns = sizeof(funcs) / sizeof(funcs[0]);

int som_function_count(void) { return fnNumFns; }

const char *som_function_name(int i)
{
	return (i >= 0 && i < fnNumFns) ? funcs[i].name : NULL;
}

int som_function_arg_count(int i)
{
	return (i >= 0 && i < fnNumFns) ? funcs[i].numoperands : 0;
}
