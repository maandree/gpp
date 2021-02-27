/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"


char *argv0;


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-D name[=value]] [-f file | [-i input-file] [-o output-file]] "
	                "[-n count] [-s symbol] [-u [-u]] [shell [argument] ...]\n", argv0);
	exit(1);
}


static int
get_fd(const char *path)
{
	const char *p;
	char *s;
	long int tmp;

	if (!strcmp(path, "/dev/stdin"))
		return STDIN_FILENO;
	if (!strcmp(path, "/dev/stdout"))
		return STDOUT_FILENO;
	if (!strcmp(path, "/dev/stderr"))
		return STDERR_FILENO;

	if (!strncmp(path, "/dev/fd/", sizeof("/dev/fd/") - 1))
		p = &path[sizeof("/dev/fd/") - 1];
	else if (!strncmp(path, "/proc/self/fd/", sizeof("/proc/self/fd/") - 1))
		p = &path[sizeof("/proc/self/fd/") - 1];
	else
		return -1;

	if (!isdigit(*p))
		return -1;
	errno = 0;
	tmp = strtol(p, &s, 10);
	if (errno || tmp > INT_MAX || *s)
		return -1;
	return (int)tmp;
}


static int
xopen(const char *path, int *do_close)
{
	int fd = get_fd(path);
	if (fd >= 0) {
		*do_close = 0;
		return fd;
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "%s: open %s O_RDONLY: %s\n", argv0, path, strerror(errno));
		exit(1);
	}
	*do_close = 1;
	return fd;
}


static int
xcreate(const char *path)
{
	int fd = get_fd(path);
	if (fd >= 0)
		return fd;
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		fprintf(stderr, "%s: open %s O_WRONLY|O_CREAT|O_TRUNC 0666: %s\n", argv0, path, strerror(errno));
		exit(1);
	}
	return fd;
}


int
main(int argc, char *argv[])
{
	const char *shell_[] = {"bash", NULL}, **shell = shell_;
	const char *input_file = NULL;
	const char *output_file = NULL;
	const char *symbol = NULL;
	int iterations = -1;
	int unshebang = 0;
	long int tmp;
	char *arg, *p, *q;
	char *in_data = NULL, *out_data = NULL;
	size_t in_size = 0, in_len = 0, in_off = 0;
	size_t out_size = 0, out_len = 0, out_off = 0;
	int in_fd, out_fd, do_close;
	ssize_t r;

	ARGBEGIN {
	case 'D':
		arg = EARGF(usage());
		if (!*arg || *arg == '=')
			usage();
		p = strchr(arg, '=');
		if (p)
			*p++ = '\0';
		if (setenv(arg, p ? p : "1", 1)) {
			fprintf(stderr, "%s: setenv %s %s 1: %s\n", argv0, arg, p ? p : "1", strerror(errno));
			return 1;
		}
		break;
	case 'f':
		if (input_file || output_file)
			usage();
		input_file = output_file = EARGF(usage());
		if (!*input_file)
			usage();
		break;
	case 'i':
		if (input_file)
			usage();
		input_file = EARGF(usage());
		if (!*input_file)
			usage();
		break;
	case 'n':
		if (iterations >= 0)
			usage();
		arg = EARGF(usage());
		if (!isdigit(*arg))
			usage();
		errno = 0;
		tmp = strtol(arg, &arg, 10);
		if (errno || tmp > INT_MAX || *arg)
			usage();
		iterations = (int)tmp;
		break;
	case 'o':
		if (output_file)
			usage();
		output_file = EARGF(usage());
		if (!*output_file)
			usage();
		break;
	case 's':
		if (symbol)
			usage();
		symbol = EARGF(usage());
		break;
	case 'u':
		if (unshebang == 2)
			usage();
		unshebang += 1;
		break;
	default:
		usage();
	} ARGEND;

	if (argc)
		shell = (void *)argv;

	if (setenv("_GPP", argv0, 1))
		fprintf(stderr, "%s: setenv _GPP %s 1: %s\n", argv0, argv0, strerror(errno));

	if (iterations < 0)
		iterations = 1;
	if (!symbol)
		symbol = "@";
	if (!input_file || (input_file[0] == '-' && !input_file[1]))
		input_file = "/dev/stdin";
	if (!output_file || (output_file[0] == '-' && !output_file[1]))
		output_file = "/dev/stdout";

	in_fd = xopen(input_file, &do_close);
	for (;;) {
		if (in_len == in_size) {
			in_data = realloc(in_data, in_size += 8096);
			if (!in_data) {
				fprintf(stderr, "%s: realloc: %s\n", argv0, strerror(errno));
				return 1;
			}
		}
		r = read(in_fd, &in_data[in_len], in_size - in_len);
		if (r <= 0) {
			if (!r)
				break;
			fprintf(stderr, "%s: read %s: %s\n", argv0, input_file, strerror(errno));
		}
		in_len += (size_t)r;
	}
	if (do_close)
		close(in_fd);

	if (unshebang && in_len >= 2 && in_data[0] == '#' && in_data[1] == '!') {
		p = memchr(in_data, '\n', in_len);
		if (!p)
			goto after_unshebang;
		in_off = (size_t)(++p - in_data);
		if (in_off == in_len)
			goto after_unshebang;
		if (unshebang >= 2) {
			q = memchr(p--, '\n', in_len - in_off);
			if (!q)
				goto after_unshebang;
			memmove(p, &p[1], (size_t)(q - in_data) - in_off--);
			*--q = '\n';
		}
	}
after_unshebang:

	while (iterations--) {
		/* TODO parse: in -> out */

		in_len = 0;
		in_off = 0;

		/* TODO shell: out -> in */

		out_len = 0;
		out_off = 0;
	}

	free(out_data);
	out_fd = xcreate(output_file);
	while (in_off < in_len) {
		r = write(out_fd, &in_data[in_off], in_len - in_off);
		if (r <= 0) {
			fprintf(stderr, "%s: write %s: %s\n", argv0, output_file, strerror(errno));
			return 1;
		}
		in_off += (size_t)r;
	}
	if (close(out_fd))
		fprintf(stderr, "%s: write %s: %s\n", argv0, output_file, strerror(errno));
	free(in_data);
	return 0;
}
