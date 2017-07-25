/*
 * Copyright 2017 Kurt Van Dijck <dev.kurt@vandijck-laurijssen.be>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define NAME "configfileform"
#ifndef VERSION
#define VERSION "<undefined version>"
#endif

/* generic error logging */
#define LOG_ERR	1
#define LOG_INFO 5
#define mylog(loglevel, fmt, ...) \
	({\
		fprintf(stderr, "%s: " fmt "\n", NAME, ##__VA_ARGS__); \
		if (loglevel <= LOG_ERR)\
			exit(1);\
	})
#define ESTR(num)	strerror(num)

/* program options */
static const char help_msg[] =
	NAME ": generate an HTML form a config file\n"
	"usage:	" NAME " [OPTIONS ...] [CONFIGFILE]\n"
	"	" NAME " [OPTIONS ...] -r DATA [CONFIGFILE]\n"
	"\n"
	"Options\n"
	" -V, --version		Show version\n"
	" -v, --verbose		Be more verbose\n"
	" -o, --out=FILE	Write output to FILE\n"
	" -r, --request=REQURI	Decode REQURI and apply to input\n"
	" -p, --print=NAME	Print NAME from input\n"
	;

#ifdef _GNU_SOURCE
static struct option long_opts[] = {
	{ "help", no_argument, NULL, '?', },
	{ "version", no_argument, NULL, 'V', },
	{ "verbose", no_argument, NULL, 'v', },

	{ "out", required_argument, NULL, 'o', },
	{ "request", required_argument, NULL, 'r', },
	{ "print", required_argument, NULL, 'p', },

	{ },
};
#else
#define getopt_long(argc, argv, optstring, longopts, longindex) \
	getopt((argc), (argv), (optstring))
#endif
static const char optstring[] = "Vv?o:r:p:";

static int verbose;
static char *buf;
static int buflen, bufsize;
static char *request;
static char *printname;

static char *shell_decode(char *str)
{
	char *dst, *saved_str;
	int esc = 0;

	for (saved_str = dst = str; *str; ++str) {
		if (*str == '\\' && !esc) {
			esc = 1;
		} else {
			esc = 0;
			*dst++ = *str;
		}
	}
	*dst = 0;
	if (strchr("\"'", *saved_str) && *(dst-1) == *saved_str) {
		/* remove quotes */
		*(dst-1) = 0;
		++saved_str;
	}
	return saved_str;
}

static const char *shell_encode(const char *str)
{
	static char *enc;
	static int encsize;
	int len, special;
	char *dst;

	len = strlen(str ?: "");
	if (len*2+2+1 > encsize) {
		encsize = len*2+2+1;
		enc = realloc(enc, encsize);
	}
	special = 0;
	for (dst = enc, *dst++ = '\''; *str; ++str) {
		static const char escchars[] = "\"'";
		static const char specialchars[] = " \\\"'&<>";

		if (strchr(specialchars, *str))
			special = 1;
		if (strchr(escchars, *str))
			*dst++ = '\\';
		*dst++ = *str;
	}
	if (special) {
		*dst++ = '\'';
		*dst = 0;
		return enc;
	}
	*dst = 0;
	return enc+1;
}

static const char *html_encode(const char *str)
{
	static char *enc;
	static int encsize;
	int len;
	char *dst;

	len = strlen(str ?: "");
	if (len*6+1 > encsize) {
		encsize = len*6+1;
		enc = realloc(enc, encsize);
	}
	for (dst = enc; *str; ++str) {
		static const char *const escapes[] = {
			"lt", "gt", "amp", "quot", "apos",
		};
		static const char escchars[] = "<>&\"'";
		char *idx;

		idx = strchr(escchars, *str);
		if (idx)
			dst += sprintf(dst, "&%s;", escapes[idx - escchars]);
		else
			*dst++ = *str;
	}
	*dst = 0;
	return enc;
}

static void append_line(const char *line)
{
	int len;
	char *value;

	if (*line == '#') {
		if (request) {
			printf("%s\n", line);
			return;
		}
		for (++line; *line == ' '; ++line);
		/* insert newline on empty lines */
		if (!*line)
			line = "<br />\n";
		len = strlen(line);
		if (buflen + 1 + len + 1 > bufsize) {
			bufsize = (buflen + 1 + len + 1 + 1024) & ~1023;
			buf = realloc(buf, bufsize);
			if (!buf)
				mylog(LOG_ERR, "realloc %u: %s", bufsize, ESTR(errno));
		}
		if (buflen)
			buf[buflen++] = ' ';
		strcpy(buf+buflen, line);
		buflen += len;

	} else if ((value = strchr(line, '=')) != NULL) {
		*value++ = 0;
		if (request) {
			char *newvalue = getenv(line);

			if (verbose)
				fprintf(stderr, "%s %s=%s\n",
						newvalue ? "changing" : "writing",
						line, shell_encode(newvalue ?: value));
			printf("%s=%s\n", line, shell_encode(newvalue ?: value));
			return;
		}
		printf("<p>%s\n<br />%s&nbsp;<input type='input' name='%s' value='%s'></p>\n",
				buf, line, line, html_encode(shell_decode(value)));
		/* flush */
		buflen = 0;

	} else if (!request && buflen) {
		/* print paragraph with all comments */
		printf("<p>%s</p>\n", buf);
		buflen = 0;
	} else if (request) {
		printf("%s\n", line);
	}
}

/* uri decoding */
static int a2i(int ascii)
{
	if (ascii >= '0' && ascii <= '9')
		return ascii - '0';
	else if (ascii >= 'a' && ascii <= 'z')
		return ascii - 'a' + 10;
	else if (ascii >= 'A' && ascii <= 'Z')
		return ascii - 'A' + 10;
	else
		return 0;
}

static int consume_hex(char **pinput)
{
	int ret = 0;

	ret |= a2i(*(++*pinput));
	ret <<= 4;
	ret |= a2i(*(++*pinput));
	return ret;
}

static char *consume_uri_param(char **pinput)
{
	char *str, *savedstr;

	if (!pinput || !*pinput || !**pinput)
		return NULL;

	for (str = savedstr = *pinput; **pinput && **pinput != '&'; ++(*pinput)) {
		if (**pinput == '%')
			*str++ = consume_hex(pinput);
		else if (**pinput == '+')
			*str++ = ' ';
		else
			*str++ = **pinput;
	}
	/* skip & character */
	if (**pinput == '&')
		*(*pinput)++ = 0;
	/* null terminate the parameter */
	*str = 0;
	return savedstr;
}

int main(int argc, char *argv[])
{
	int opt, ret;
	char *line = NULL, *key, *value;
	size_t linesize = 0;

	/* argument parsing */
	while ((opt = getopt_long(argc, argv, optstring, long_opts, NULL)) >= 0)
	switch (opt) {
	case 'V':
		fprintf(stderr, "%s %s\nCompiled on %s %s\n",
				NAME, VERSION, __DATE__, __TIME__);
		exit(0);
	case 'v':
		++verbose;
		break;
	case 'o':
		ret = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (ret < 0)
			mylog(LOG_ERR, "open %s: %s", optarg, ESTR(errno));
		if (dup2(ret, STDOUT_FILENO) < 0)
			mylog(LOG_ERR, "dup2 %i %i: %s", ret, STDOUT_FILENO, ESTR(errno));
		close(ret);
		break;
	case 'r':
		request = optarg;
		break;
	case 'p':
		printname = optarg;
		break;

	default:
		fprintf(stderr, "unknown option '%c'\n", opt);
	case '?':
		fputs(help_msg, stderr);
		exit(1);
		break;
	}

	/* redirect stdin? */
	if (optind < argc) {
		ret = open(argv[optind], O_RDONLY);
		if (ret < 0)
			mylog(LOG_ERR, "open %s: %s", argv[optind], ESTR(errno));
		if (dup2(ret, STDIN_FILENO) < 0)
			mylog(LOG_ERR, "dup2 %i %i: %s", ret, STDIN_FILENO, ESTR(errno));
		close(ret);
	}

	if (request) {
		/* use environment for storing request variables */
		if (verbose >= 2)
			fprintf(stderr, "cgi: %s\n", request);
		clearenv();
		for (;;) {
			key = consume_uri_param(&request);
			if (!key)
				break;
			value = strchr(key, '=');
			if (value)
				*value++ = 0;
			if (verbose >= 2)
				fprintf(stderr, "cgi: %s=%s\n", key, value);
			setenv(key, value ?: "", 1);
		}
		if (printname) {
			printf("%s", getenv(printname) ?: "");
		}
	}

	while (1) {
		ret = getline(&line, &linesize, stdin);
		if (ret < 0 && feof(stdin))
			break;
		if (ret < 0)
			mylog(LOG_ERR, "getline failed: %s", ESTR(errno));
		if (line[ret-1] == '\n')
			line[ret-1] = 0;
		append_line(line);
	}
	/* flush buffers */
	if (!request)
		append_line("");

	return 0;
}
