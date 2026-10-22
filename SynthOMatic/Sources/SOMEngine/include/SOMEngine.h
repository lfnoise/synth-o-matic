/*
 *  SOMEngine.h
 *
 *  Public interface to the Synth-O-Matic synthesis engine.
 *
 *  The engine is a port of the original 1990-1992 Synth-O-Matic sources
 *  (THINK C, 68k Macintosh).  The DSP code, compiler and interpreter are kept
 *  as close to the original as possible; this header is the boundary used by
 *  the macOS front end and the command line renderer.
 *
 *  Every document owns one SOMContext.  A context may be used from any thread,
 *  but only from one thread at a time.
 */

#ifndef SOMENGINE_H
#define SOMENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SOMContext SOMContext;

/* signal (variable) types, same numbering as the original */
typedef enum {
	SOMTypeScalar   = 0,	/* "Variable" / "Parameter" */
	SOMTypeTable    = 1,	/* "Wavetable" : 512 points + guard point */
	SOMTypeEnvelope = 2,	/* "Envelope"  : 512 points stretched over the duration */
	SOMTypeSample   = 3		/* "Sample"    : one value per output sample */
} SOMVarType;

enum { SOM_TABLE_SIZE = 512, SOM_BLOCK_SIZE = 512, SOM_VAR_NAME_LEN = 32 };

typedef struct {
	char name[SOM_VAR_NAME_LEN];
	int type;			/* SOMVarType */
	double value;		/* current value of a scalar */
	double lolim;		/* lower plot/slider limit */
	double hilim;		/* upper plot/slider limit */
	double stepsiz;		/* slider / randomize step size, 0 = continuous */
	int pfield;			/* score card p-field number, 0 = none */
	int assigned;		/* 0 = never assigned, 1 = assigned, 2 = assigned only (an output) */
	int referenced;		/* 1 if the program reads this variable */
	int randomset;		/* member of the random set */
	long length;		/* number of points in the data array */
} SOMVarInfo;

/* ---- lifetime ---- */

SOMContext *som_create(void);
void som_destroy(SOMContext *ctx);

/* Messages that the original printed to its "Message" window. */
typedef void (*SOMMessageFn)(void *user, const char *text);
void som_set_message_callback(SOMContext *ctx, SOMMessageFn fn, void *user);

/* Reset to the state of File > New (one line program, note & freq). */
void som_new_document(SOMContext *ctx);

/* ---- program text (UTF-8, '\n' line endings) ---- */

const char *som_get_program(SOMContext *ctx);
void som_set_program(SOMContext *ctx, const char *utf8);

/* ---- patch files (the classic 'SOM1' format, all versions) ---- */

/* returns 0 on success; on failure a message has been printed */
int som_read_patch(SOMContext *ctx, const unsigned char *data, size_t length);
/* returns a malloc'ed buffer the caller must free() */
unsigned char *som_write_patch(SOMContext *ctx, size_t *length);
/* quick test whether a file looks like a Synth-O-Matic patch */
int som_is_patch_data(const unsigned char *data, size_t length);

/* ---- compile & calculate ---- */

/* returns 0 on success.  New variables named in the program are created. */
int som_compile(SOMContext *ctx);
int som_is_compiled(SOMContext *ctx);
/* 1-based line number of the last compile error, 0 if none */
int som_error_line(SOMContext *ctx);

/* Called once per calculated block.  Return nonzero to stop. */
typedef int (*SOMProgressFn)(void *user, long blocksDone, long blocksTotal);
enum { SOMCalcOK = 0, SOMCalcStopped = 1, SOMCalcError = -1 };
int som_calculate(SOMContext *ctx, SOMProgressFn progress, void *user);
/* index of the first assigned sample variable (in display order), or -1 */
int som_first_output_sample(SOMContext *ctx);

/* ---- globals ---- */

double som_sample_rate(SOMContext *ctx);
double som_duration(SOMContext *ctx);		/* seconds */
double som_max_delay(SOMContext *ctx);		/* seconds */
long som_sample_length(SOMContext *ctx);	/* samples */
long som_buffered_length(SOMContext *ctx);	/* samples, rounded up to whole blocks */
void som_set_globals(SOMContext *ctx, double sampleRate, double duration, double maxDelay);
const char *som_in_filename(SOMContext *ctx);
const char *som_out_filename(SOMContext *ctx);
void som_set_io_filenames(SOMContext *ctx, const char *in, const char *out);

/* ---- variables ---- */

int som_var_count(SOMContext *ctx);
/* the variable shown at a row of the variable list (sorted like the original) */
int som_var_at_row(SOMContext *ctx, int row);
int som_row_of_var(SOMContext *ctx, int var);
int som_find_var(SOMContext *ctx, const char *name);
void som_get_var_info(SOMContext *ctx, int var, SOMVarInfo *info);
/* direct access to the table / envelope / sample points (length from info) */
float *som_var_data(SOMContext *ctx, int var);

int  som_define_var(SOMContext *ctx, const char *name, int type);
void som_set_var_value(SOMContext *ctx, int var, double value);
void som_set_var_limits(SOMContext *ctx, int var, double lolim, double hilim, double stepsiz);
void som_set_var_pfield(SOMContext *ctx, int var, int pfield);
void som_set_var_type(SOMContext *ctx, int var, int type);
/* the logic of the original "Set Value" dialog: value, limits, number of steps, p-field */
void som_set_scalar_params(SOMContext *ctx, int var, double value, double lolim,
	double hilim, double numsteps, int pfield);
/* the logic of the original "Set Graph Bounds" dialog */
void som_set_graph_bounds(SOMContext *ctx, int var, double hilim, double lolim);
void som_find_limits(SOMContext *ctx, int var, double *minval, double *maxval, double *stepsiz);
void som_auto_scale(SOMContext *ctx, int var);		/* tables: rounded limits */
void som_auto_scale_exact(SOMContext *ctx, int var);	/* samples: exact limits */
/* restore a variable completely (used for undo).  data may be NULL. */
void som_restore_var(SOMContext *ctx, int var, const SOMVarInfo *info,
	const float *data, long length);
/* load points into a sample variable (e.g. an imported sound) */
void som_set_sample_data(SOMContext *ctx, int var, const float *data, long length);

void som_make_random_set(SOMContext *ctx, const int *vars, int count);
void som_randomize(SOMContext *ctx);
/* removes the given variables if they are unused, or every unused one when all != 0 */
int som_remove_unused(SOMContext *ctx, const int *vars, int count, int all);
void som_memory_report(SOMContext *ctx);

/* ---- wavetable filling and altering (the Fill and Alter menus) ---- */

typedef enum {
	SOMAlterReverse, SOMAlterInvert, SOMAlterAbs, SOMAlterClipNegative,
	SOMAlterSquare, SOMAlterAdd, SOMAlterMultiply,
	SOMAlterSquashUp, SOMAlterSquashDown, SOMAlterSquashIn, SOMAlterSquashOut,
	SOMAlterHanning, SOMAlterParzen, SOMAlterWelch, SOMAlterNormalize,
	SOMAlterSmooth, SOMAlterAnalyze
} SOMAlterOp;

/* clip: 513 points used by Add / Multiply, NULL otherwise */
void som_alter_table(SOMContext *ctx, int var, SOMAlterOp op, const float *clip);
void som_fill_fm(SOMContext *ctx, int var, double carrier, double modulator, double index);
void som_fill_gbuzz(SOMContext *ctx, int var, double numHarmonics, double lowest, double coef);
/* shape: 0 Hanning, 1 Welch, 2 Parzen; duration in percent */
void som_fill_grain_env(SOMContext *ctx, int var, int shape, double duration);
void som_comb_table(SOMContext *ctx, int var, int even, double teeth);
/* 32 harmonic amplitudes and phases in the range 0..99 */
void som_analyze_harmonics(SOMContext *ctx, int var, unsigned char ampl[32], unsigned char phas[32]);
void som_sine_fill(SOMContext *ctx, int var, const unsigned char ampl[32], const unsigned char phas[32]);
/* also fills "<name>ps", the power scaling table, if it exists; returns its index or -1 */
int som_cheby_fill(SOMContext *ctx, int var, const unsigned char ampl[32], const unsigned char phas[32]);

/* ---- sound file export ---- */

typedef enum {
	SOMExportAIFF16,		/* as the original: 16 bit, normalized */
	SOMExportWAV16,
	SOMExportWAV24,
	SOMExportWAVFloat,
	SOMExportSoundDesigner	/* the original's 'export to SD' format */
} SOMExportFormat;

/* returns 0 on success */
int som_export_sample(SOMContext *ctx, int var, const char *path, SOMExportFormat format, int normalize);
int som_export_stereo(SOMContext *ctx, int left, int right, const char *path,
	SOMExportFormat format, int normalize);

/* ---- language information (for the editor and the reference) ---- */

int som_function_count(void);
const char *som_function_name(int i);
int som_function_arg_count(int i);

/* the spirograph tables of the About box */
const signed char *som_spiro_sin(void);
const signed char *som_spiro_cos(void);
int som_spiro_size(void);

/* seed the random number generator (the original seeded from TickCount) */
void som_set_random_seed(SOMContext *ctx, unsigned long seed);

double som_version(void);

#ifdef __cplusplus
}
#endif

#endif
