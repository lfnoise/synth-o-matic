/*
 *  som_compile.c
 *
 *  The code generator called by the parser, from the original interpreter.c.
 *  The program is a sequence of UGens in reverse polish order.
 */

#include "som_internal.h"

/*
	each variable must know the last ugen that assigned to it
	so that subsequent references refer to the source of the prev assign
*/

#define PROGINC 80
#define IPROGSIZ 80

static void BlockAnalysis(void);
static void BlockSwap(void);

static void
initcompiler(void) {
	int i, j;
	compileptr = 0;
	progsize = 0;
	maxprogsize = IPROGSIZ;
	maxopstack = 100;
	opstackptr = 0;
	jumpptr = 0;
	if (U) free(U);
	operandstack = allocate((long)maxopstack * sizeof(int));
	U = (UGen*)allocate((long)maxprogsize*sizeof(UGen));
	ref = (RefCountPtr)allocate((long)maxvars*sizeof(RefCount));
	for (i=0; i<numvars; ++i) {
		vars[i].assigned = 0;
		vars[i].referenced = 0;
		for (j=0; j<MAXJUMP; ++j) {
			ref[i].assigned[j] = 0;
			ref[i].referenced[j] = 0;
		}
	}
}

static void
finicompiler(void) {
	int i;

	if (compileptr) {
		for (i=0; i<numvars; ++i) {
			if (ref[i].assigned[0] == 2
				|| (vars[i].assigned && !vars[i].referenced)) vars[i].assigned = 2;
		}
	}
	free(ref);
	ref = NULL;
	free(operandstack);
	operandstack = NULL;
	progsize = compileptr;
}

int
som_compile_program(void) {
	int retc, i;

	som_cur->x_errorline = 0;
	som_cur->x_allocerr_armed = 1;
	if (setjmp(allocerr)) {
		som_cur->x_allocerr_armed = 0;
		dprintf("Not Enough Memory\r");
		if (ref) { free(ref);	ref = NULL;	}
		if (operandstack) { free(operandstack);	operandstack = NULL;	}
		progsize = compileptr = 0;
		for (i=0; i<numvars; ++i) {
			if (vars[i].ac) free(vars[i].ac);
			vars[i].ac = NULL;
		}
		InitSignals();
		return 1;
	}
	initlexer();
	initcompiler();
	retc = yyparse();
	finicompiler();
	/* port: a failed parse could leave half a program behind */
	if (retc != 0) progsize = compileptr = 0;
	som_cur->x_allocerr_armed = 0;
	FixSignalWindow();
	FixPFields();
	return retc;
}

static void
CoreSpace(void) {
	maxprogsize += PROGINC;
	U = (UGen *)reallocate(U, (long)maxprogsize*sizeof(UGen));
	memset(U + maxprogsize - PROGINC, 0, PROGINC*sizeof(UGen));
}

static void
oppush(int x) {
	if (opstackptr >= maxopstack) {
		maxopstack += 100;
		operandstack = reallocate(operandstack, (long)maxopstack * sizeof(int));
	}
	operandstack[opstackptr++] = x;
}

int
compile2(int op, int id) {
	int i, k, in[32], mask;
	UGen *u;

	if (op == opFuncCall) {		/**** FUNCCALL ****/
		if (numargs[funclevel] != funcs[id].numoperands) {
			dprintf("Wrong Number of operands in call to '%s' on line %d (needs %d)\r",
				funcs[id].name, lineno+1, funcs[id].numoperands);
			som_cur->x_errorline = lineno+1;
			return 0;
		}
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = funcs[id].execute;
		if (++compileptr >= maxprogsize) CoreSpace();
		mask = funcs[id].asgnmask;
		for (i=0; i<numargs[funclevel]; ++i) {
			in[i] = operandstack[opstackptr-i-1];
			if (mask & 1) {
				int code = U[in[i]].code;
				if (!(code == opPushVar || code == opPushVec)) {
					dprintf("Nonvariable in assignment position in call to '%s' on line %d\r",
						funcs[id].name, lineno+1);
					som_cur->x_errorline = lineno+1;
					return 0;
				} else {
					k = U[in[i]].oper;
					vars[k].assigned = 1;	/* port: was vars[i] */
					if (!ref[k].assigned[jumpptr]) {
						ref[k].assigned[jumpptr] = ref[k].referenced[jumpptr] ? 1 : 2;
					}
				}
			}
			mask >>= 1;
		}
		opstackptr -= numargs[funclevel];
		oppush(compileptr);
		mask = funcs[id].asgnmask;
		if (mask) {
			for (i=0; i<numargs[funclevel]; ++i) {
				if (mask & 1) oppush(compileptr);
				mask >>= 1;
			}
		}
		mask = funcs[id].asgnmask;
		if (mask) {
			for (i=0; i<numargs[funclevel]; ++i) {
				if (mask & 1) {
					compile2(opAssign, U[in[i]].oper);
					compile2(opDrop, -1);
				}
				mask >>= 1;
			}
		}
	} else if (op == opPushVar) {		/**** PUSHVAR ****/
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = codes[op].execute;
		vars[id].referenced = 1;
		ref[id].referenced[jumpptr] = 1;
		oppush(compileptr);
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opPushVec) {		/**** PUSHVEC ****/
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = codes[op].execute;
		vars[id].referenced = 1;
		ref[id].referenced[jumpptr] = 1;
		--opstackptr;
		oppush(compileptr);
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opPushCon) {		/**** PUSHCON ****/
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = codes[op].execute;
		oppush(compileptr);
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opAssign) {		/**** ASSIGN ****/
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = codes[op].execute;
		--opstackptr;
		oppush(compileptr);
		vars[id].assigned = 1;
		if (!ref[id].assigned[jumpptr])
			ref[id].assigned[jumpptr] = ref[id].referenced[jumpptr] ? 1 : 2;
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opAssignVec) {		/**** ASSIGNVEC ****/
		u = U + compileptr;
		u->code = op;
		u->oper = id;
		u->srcline = lineno;
		u->func = codes[op].execute;
		opstackptr-=2;
		oppush(compileptr);
		vars[id].assigned = 1;
		if (!ref[id].assigned[jumpptr])
			ref[id].assigned[jumpptr] = ref[id].referenced[jumpptr] ? 1 : 2;
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opIf) {				/**** IF ****/
		u = U + compileptr;
		u->code = op;
		u->oper = -1;
		u->srcline = lineno;
		u->func = codes[op].execute;
		--opstackptr;
		if (jumpptr+1 >= MAXJUMP) {
			dprintf("If statements nested too deep!  ( > %d levels)\r", MAXJUMP);
			som_cur->x_errorline = lineno+1;
			return 0;
		}
		for (i=0;i<numvars;++i) {
			ref[i].assigned[jumpptr+1] = ref[i].assigned[jumpptr];
			ref[i].referenced[jumpptr+1] = ref[i].referenced[jumpptr];
		}
		jumpstack[jumpptr++] = compileptr;
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opElse) {				/**** ELSE ****/
		u = U + compileptr;
		u->code = op;
		u->oper = -1;
		u->srcline = lineno;
		u->func = codes[op].execute;
		for (i=codes[op].numoperands-1; i>=0; --i) {
			--opstackptr;
		}
		U[jumpstack[--jumpptr]].oper = compileptr+1;
		BlockSwap();
		BlockAnalysis();
		jumpstack[jumpptr++] = compileptr;
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opEndIf || op == opEndIfElse) {	/**** ENDIF ****/
		U[jumpstack[--jumpptr]].oper = compileptr;
		BlockAnalysis();
	} else if (op == opDrop) {						/**** DROP ****/
		u = U + compileptr;
		u->code = op;
		u->oper = -1;
		u->srcline = lineno;
		u->func = codes[op].execute;
		--opstackptr;
		if (++compileptr >= maxprogsize) CoreSpace();
	} else if (op == opOver) {						/**** OVER ****/
		u = U + compileptr;
		u->code = op;
		u->oper = -1;
		u->srcline = lineno;
		u->func = codes[op].execute;
		oppush(compileptr);
		if (++compileptr >= maxprogsize) CoreSpace();
	} else {						/**** OTHER ****/
		u = U + compileptr;
		u->code = op;
		u->oper = -1;
		u->srcline = lineno;
		u->func = codes[op].execute;
		for (i=codes[op].numoperands-1; i>=0; --i) {
			--opstackptr;
		}
		oppush(compileptr);
		if (++compileptr >= maxprogsize) CoreSpace();
	}
	return 1;
}

static void
BlockAnalysis(void) {
	int i, x1, x2, a1, a2, r1, r2;

	for (i=0; i<numvars; ++i) {
		a1 = ref[i].assigned[jumpptr];
		r1 = ref[i].referenced[jumpptr];
		x1 = a1+a1 + r1;
		a2 = ref[i].assigned[jumpptr+1];
		r2 = ref[i].referenced[jumpptr+1];
		x2 = a2+a2 + r2;
		if (x1 == x2) { }
		else if (x1 == 3 || x2 == 3) { a1 = 1;	r1 = 1; }
		else if (x1 == 1 || x2 == 1) { a1 = 0;	r1 = 1; }
		else if (x1 == 0 || x2 == 0) { a1 = 0;	r1 = 0; }
		else { a1 = 1;	r1 = 0; }
		ref[i].assigned[jumpptr] = a1;
		ref[i].referenced[jumpptr] = r1;
	}
}

static void
BlockSwap(void) {
	int i;

	for (i=0;i<numvars;++i) {
		SWAP(int, ref[i].assigned[jumpptr],   ref[i].assigned[jumpptr+1]);
		SWAP(int, ref[i].referenced[jumpptr], ref[i].referenced[jumpptr+1]);
	}
}

void
FixPFields(void) {
	int i;

	for (i=0; i<MAXPF; ++i) pfields[i] = NULL;
	for (i=0; i<MAXGL; ++i) globals[i] = NULL;
	for (i=0; i<MAXOUT; ++i) outputs[i] = inputs[i] = NULL;
	for (i=0; i<numvars; ++i) {
		vars[i].input = vars[i].output = 0;
		if (vars[i].pfield > 0 && vars[i].pfield < MAXPF) {
			pfields[(int)vars[i].pfield] = vars + i;
		}
		if (strcmp(vars[i].name, "sr"   )==0) globals[0] = vars+i;
		if (strcmp(vars[i].name, "nyq"  )==0) globals[1] = vars+i;
		if (strcmp(vars[i].name, "tempo")==0) globals[2] = vars+i;
		if (strcmp(vars[i].name, "out"  )==0) {
			outputs[0] = vars+i;
			vars[i].output = 1;
		}
		if (strcmp(vars[i].name, "out1" )==0) {
			outputs[0] = vars+i;
			vars[i].output = 1;
		}
		if (strcmp(vars[i].name, "out2" )==0) {
			outputs[1] = vars+i;
			vars[i].output = 2;
		}
		if (strcmp(vars[i].name, "out3" )==0) {
			outputs[2] = vars+i;
			vars[i].output = 3;
		}
		if (strcmp(vars[i].name, "out4" )==0) {
			outputs[3] = vars+i;
			vars[i].output = 4;
		}
		if (strcmp(vars[i].name, "inp"  )==0) {
			inputs[0] = vars+i;
			vars[i].input = 1;
		}
		if (strcmp(vars[i].name, "inp1" )==0) {
			inputs[0] = vars+i;
			vars[i].input = 1;
		}
		if (strcmp(vars[i].name, "inp2" )==0) {
			inputs[1] = vars+i;
			vars[i].input = 2;
		}
		if (strcmp(vars[i].name, "inp3" )==0) {
			inputs[2] = vars+i;
			vars[i].input = 3;
		}
		if (strcmp(vars[i].name, "inp4" )==0) {
			inputs[3] = vars+i;
			vars[i].input = 4;
		}
	}
}

/* ---------------------------------------------------------------------- */

int
som_compile(SOMContext *ctx) {
	int retc;
	SOM_ENTER(ctx);
	retc = som_compile_program();
	if (retc == 0 && progsize == 0 && ctx->x_text[0]) {
		/* an empty program is fine, a program that compiled to nothing is not */
	}
	return retc;
}

int som_is_compiled(SOMContext *ctx) { return ctx->x_progsize > 0; }
int som_error_line(SOMContext *ctx) { return ctx->x_errorline; }
