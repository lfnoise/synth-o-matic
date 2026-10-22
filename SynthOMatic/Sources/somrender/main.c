/*
 *  somrender
 *
 *  Command line renderer for Synth-O-Matic patches.
 *
 *    somrender [options] patch [patch ...]
 *
 *      -o file      output sound file (default: patch name + .wav)
 *      -v var       sample variable to write (default: the first output)
 *      -f format    wav16 | wav24 | wavf | aiff | sd   (default wav16)
 *      -n           don't normalize
 *      -s           print statistics only, write nothing
 *      -d secs      override the duration
 *      -p           print the program and variables
 *      -w file      also re-save the patch (current file format)
 *      -e program   render a program given on the command line instead of a patch
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SOMEngine.h"

static void
message(void *user, const char *text) {
	fputs(text, stderr);
}

static unsigned char *
readfile(const char *path, size_t *len) {
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	long n;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(n ? n : 1);
	if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
	fclose(f);
	*len = n;
	return buf;
}

static const char *typenames[] = { "Variable", "Wavetable", "Envelope", "Sample" };

static int
render(const char *path, const char *program, const char *outpath, const char *varname, SOMExportFormat format,
	int normalize, int statsOnly, double duration, int print, const char *savepath)
{
	SOMContext *ctx;
	unsigned char *data;
	size_t len;
	int var, i, n, result = 0;
	SOMVarInfo info;
	char outbuf[1024];

	ctx = som_create();
	som_set_message_callback(ctx, message, NULL);
	if (program) {
		som_new_document(ctx);
		som_set_program(ctx, program);
		path = "program";
	} else {
		data = readfile(path, &len);
		if (!data) { fprintf(stderr, "%s: can't read\n", path); som_destroy(ctx); return 1; }
		if (som_read_patch(ctx, data, len) != 0) {
			fprintf(stderr, "%s: not a Synth-O-Matic patch\n", path);
			free(data);
			som_destroy(ctx);
			return 1;
		}
		free(data);
	}
	if (duration > 0.) som_set_globals(ctx, 0., duration, -1.);
	printf("%s: %.0f Hz, %.3f s\n", path, som_sample_rate(ctx), som_duration(ctx));
	if (som_compile(ctx) != 0 || !som_is_compiled(ctx)) {
		printf("  compile failed (line %d)\n", som_error_line(ctx));
		result = 2;
		goto done;
	}
	if (print) {
		printf("----\n%s----\n", som_get_program(ctx));
		n = som_var_count(ctx);
		for (i=0; i<n; ++i) {
			som_get_var_info(ctx, som_var_at_row(ctx, i), &info);
			printf("  %-16s %-10s p%-2d asg=%d ref=%d value=%g [%g %g step %g]\n",
				info.name, typenames[info.type], info.pfield, info.assigned, info.referenced,
				info.value, info.lolim, info.hilim, info.stepsiz);
		}
	}
	if (som_calculate(ctx, NULL, NULL) != SOMCalcOK) {
		printf("  calculate failed\n");
		result = 3;
		goto done;
	}
	var = varname ? som_find_var(ctx, varname) : som_first_output_sample(ctx);
	if (var < 0) {
		printf("  no output sample\n");
		result = 4;
		goto done;
	}
	som_get_var_info(ctx, var, &info);
	{
		float *s = som_var_data(ctx, var);
		double peak = 0., sum = 0.;
		long nan = 0, k;
		for (k=0; k<info.length; ++k) {
			if (isnan(s[k]) || isinf(s[k])) { nan++; continue; }
			if (fabs(s[k]) > peak) peak = fabs(s[k]);
			sum += s[k] * (double)s[k];
		}
		printf("  %s: %ld samples, peak %.4g, rms %.4g%s\n", info.name, info.length, peak,
			info.length ? sqrt(sum/info.length) : 0., nan ? "  ** NaN/Inf **" : "");
		if (nan) result = 5;
	}
	if (!statsOnly) {
		if (!outpath) {
			const char *base = strrchr(path, '/');
			snprintf(outbuf, sizeof(outbuf), "%s.%s", base ? base+1 : path,
				format == SOMExportAIFF16 ? "aif" : format == SOMExportSoundDesigner ? "sd" : "wav");
			outpath = outbuf;
		}
		if (som_export_sample(ctx, var, outpath, format, normalize) == 0)
			printf("  wrote %s\n", outpath);
		else result = 6;
	}
	if (savepath) {
		size_t plen;
		unsigned char *pdata = som_write_patch(ctx, &plen);
		FILE *f = fopen(savepath, "wb");
		if (f) { fwrite(pdata, 1, plen, f); fclose(f); printf("  saved %s\n", savepath); }
		free(pdata);
	}
done:
	som_destroy(ctx);
	return result;
}

int
main(int argc, char **argv) {
	const char *outpath = NULL, *varname = NULL, *savepath = NULL;
	SOMExportFormat format = SOMExportWAV16;
	int normalize = 1, statsOnly = 0, print = 0, i, failures = 0, files = 0;
	double duration = 0.;

	for (i=1; i<argc; ++i) {
		if (strcmp(argv[i], "-o") == 0 && i+1 < argc) outpath = argv[++i];
		else if (strcmp(argv[i], "-v") == 0 && i+1 < argc) varname = argv[++i];
		else if (strcmp(argv[i], "-w") == 0 && i+1 < argc) savepath = argv[++i];
		else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) duration = atof(argv[++i]);
		else if (strcmp(argv[i], "-n") == 0) normalize = 0;
		else if (strcmp(argv[i], "-e") == 0 && i+1 < argc) {
			files++;
			if (render(NULL, argv[++i], outpath, varname, format, normalize, statsOnly, duration, print, savepath))
				failures++;
		}
		else if (strcmp(argv[i], "-s") == 0) statsOnly = 1;
		else if (strcmp(argv[i], "-p") == 0) print = 1;
		else if (strcmp(argv[i], "-f") == 0 && i+1 < argc) {
			const char *f = argv[++i];
			if (strcmp(f, "wav16") == 0) format = SOMExportWAV16;
			else if (strcmp(f, "wav24") == 0) format = SOMExportWAV24;
			else if (strcmp(f, "wavf") == 0) format = SOMExportWAVFloat;
			else if (strcmp(f, "aiff") == 0) format = SOMExportAIFF16;
			else if (strcmp(f, "sd") == 0) format = SOMExportSoundDesigner;
			else { fprintf(stderr, "unknown format %s\n", f); return 1; }
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr, "usage: somrender [-o out] [-v var] [-f wav16|wav24|wavf|aiff|sd] "
				"[-n] [-s] [-p] [-d secs] [-w resaved.som] patch ...\n");
			return 1;
		}
		else {
			files++;
			if (render(argv[i], NULL, outpath, varname, format, normalize, statsOnly, duration, print, savepath))
				failures++;
		}
	}
	if (files == 0) {
		fprintf(stderr, "usage: somrender [options] patch ...\n");
		return 1;
	}
	if (files > 1) printf("%d of %d patches rendered\n", files - failures, files);
	return failures ? 1 : 0;
}
