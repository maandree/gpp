/* See LICENSE file for copyright and license details. */
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
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
	                "[-n count] [-R macroline-replacement-text] [-s symbol] [-u [-u]] "
	                "[shell [argument] ...]\n", argv0);
	exit(1);
}


static void
vweprintf(const char *fmt, va_list ap)
{
	int errnum = errno;
	fprintf(stderr, "%s: ", argv0);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " %s\n", strerror(errnum));
}

static void
weprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vweprintf(fmt, ap);
	va_end(ap);
}

static void
eprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vweprintf(fmt, ap);
	va_end(ap);
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
	if (fd < 0)
		eprintf("open %s O_RDONLY:", path);
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
	if (fd < 0)
		eprintf("open %s O_WRONLY|O_CREAT|O_TRUNC 0666:", path);
	return fd;
}


static void
append(char **restrict out_datap, size_t *restrict out_lenp, size_t *restrict out_sizep, const char *fmt, ...)
{
	va_list ap, ap2;
	size_t len;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	len = (size_t)vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	if (*out_lenp + len + 1 > *out_sizep) {
		*out_sizep = *out_lenp + len + 1;
		*out_datap = realloc(*out_datap, *out_sizep);
		if (!*out_datap)
			eprintf("realloc:");
	}
	vsprintf(&(*out_datap)[*out_lenp], fmt, ap);
	*out_lenp += len;
	va_end(ap);
}


int
main(int argc, char *argv[])
{
	const char *shell_[] = {"bash", NULL}, **shell = shell_;
	const char *input_file = NULL;
	const char *output_file = NULL;
	const char *symbol = NULL;
	const char *replacement = NULL;
	size_t symlen = 1;
	int iterations = -1;
	int unshebang = 0;
	long int tmp;
	char *arg, *p, *q;
	char *in_data = NULL, *out_data = NULL;
	size_t in_size = 0, in_len = 0, in_off = 0;
	size_t out_size = 0, out_len = 0, out_off = 0;
	int in_fd, out_fd, do_close, fds_in[2], fds_out[2];
	struct pollfd pfds[2];
	size_t npfds;
	char buffer[4096], c, *quotes = NULL, quote;
	size_t brackets, nquotes, quotes_size = 0;
	int symb, esc, dollar;
	size_t len, j, lineno, no = 0;
	int i, n, status, state, entered;
	ssize_t r;
	pid_t pid;

	ARGBEGIN {
	case 'D':
		arg = EARGF(usage());
		if (!*arg || *arg == '=')
			usage();
		p = strchr(arg, '=');
		if (p)
			*p++ = '\0';
		if (setenv(arg, p ? p : "1", 1))
			eprintf("setenv %s %s 1:", arg, p ? p : "1");
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
	case 'R':
		if (replacement)
			usage();
		replacement = EARGF(usage());
		break;
	case 's':
		if (symbol)
			usage();
		symbol = EARGF(usage());
		symlen = strlen(symbol);
		if (!symlen)
			usage();
		break;
	case 'u':
		if (unshebang == 2)
			usage();
		unshebang += 1;
		break;
	default:
		usage();
	} ARGEND;

	if (argc) {
		if (!**argv)
			usage();
		shell = (void *)argv;
	}

	if (setenv("_GPP", argv0, 1))
		weprintf("setenv _GPP %s 1:", argv0);

	if (iterations < 0)
		iterations = 1;
	if (!replacement)
		replacement = "";
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
			if (!in_data)
				eprintf("realloc:");
		}
		r = read(in_fd, &in_data[in_len], in_size - in_len);
		if (r <= 0) {
			if (!r)
				break;
			eprintf("read %s:", input_file);
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
		entered = 0;
		state = 0;
		lineno = 1;
		brackets = nquotes = 0;
		symb = esc = dollar = 0;
		while (in_off < in_len) {
			if (state == 2) {
			preprocess:
				c = in_data[in_off++];
				if (c == '\n') {
					lineno += 1;
					state = 0;
					brackets = nquotes = 0;
					symb = esc = dollar = 0;
					append(&out_data, &out_len, &out_size, "'\n");
				} else if (brackets) {
					if (esc) {
						esc = 0;
					} else if (nquotes) {
						if (dollar) {
							dollar = 0;
							if (c == '(') {
								quote = ')';
								goto add_to_quotes;
							} else if (c == '{') {
								quote = '}';
								goto add_to_quotes;
							}
						} else if (c == quotes[nquotes - 1]) {
							nquotes -= 1;
						} else if ((quotes[nquotes - 1] == ')' || quotes[nquotes - 1] == '}') &&
							   (c == '"' || c == '\'' || c == '`')) {
							quote = c;
							goto add_to_quotes;
						} else if (c == '\\' && quotes[nquotes - 1] != '\'') {
							esc = 1;
						} else if (c == '$') {
							dollar = 1;
						}
					} else if (c == '"' || c == '\'' || c == '`') {
						quote = c;
					add_to_quotes:
						if (nquotes == quotes_size) {
							quotes = realloc(quotes, quotes_size += 1);
							if (!quotes)
								eprintf("realloc:");
						}
						quotes[nquotes++] = quote;
					} else if (c == ')' || c == '}') {
						if (!--brackets) {
							append(&out_data, &out_len, &out_size, "%c\"'", c);
							continue;
						}
					} else if (c == '(' || c == '{') {
						brackets += 1;
					} else if (c == '\\') {
						esc = 1;
					}
					append(&out_data, &out_len, &out_size, "%c", c);
				} else if (c == *symbol && in_len - (in_off - 1) >= symlen &&
					   !memcmp(&in_data[in_off - 1], symbol, symlen)) {
					if (symb)
						append(&out_data, &out_len, &out_size, "%s", symbol);
					symb ^= 1;
					in_off += symlen - 1;
				} else if (symb) {
					symb = 0;
					if (c == '(' || c == '{') {
						brackets += 1;
						append(&out_data, &out_len, &out_size, "'\"$%c", c);
					} else if (c == '\'') {
						append(&out_data, &out_len, &out_size, "%s'\\''", symbol);
					} else {
						append(&out_data, &out_len, &out_size, "%s%c", symbol, c);
					}
				} else if (c == '\'') {
					append(&out_data, &out_len, &out_size, "'\\''");
				} else {
					if (out_len == out_size) {
						out_data = realloc(out_data, out_size += 4096);
						if (!out_data)
							eprintf("realloc:");
					}
					if ((out_data[out_len++] = c) == '\n') {
						lineno += 1;
						state = 0;
					}
				}
			} else if (state == 1) {
			append_char:
				if (out_len == out_size) {
					out_data = realloc(out_data, out_size += 4096);
					if (!out_data)
						eprintf("realloc:");
				}
				if ((out_data[out_len++] = in_data[in_off++]) == '\n') {
					lineno += 1;
					state = 0;
				}
			} else {
				if (in_len - in_off > symlen && !memcmp(&in_data[in_off], symbol, symlen) &&
				    (in_data[in_off + symlen] == '<' || in_data[in_off + symlen] == '>')) {
					state = 1;
					entered = in_data[in_off + symlen] == '<';
					in_off += symlen + 1;
				} else if (entered) {
					goto append_char;
				} else {
					append(&out_data, &out_len, &out_size, "printf '\\000%%zu\\000%%s\\n' %zu '", lineno);
					state = 2;
					goto preprocess;
				}
			}
		}
		if (state == 2)
			append(&out_data, &out_len, &out_size, "'");

		in_len = 0;
		in_off = 0;

		if (pipe(fds_in) || pipe(fds_out))
			eprintf("pipe:");
		pid = fork();
		switch (pid) {
		case -1:
			eprintf("fork:");
		case 0:
			close(fds_in[1]);
			close(fds_out[0]);
			if (dup2(fds_in[0], STDIN_FILENO) != STDIN_FILENO)
				eprintf("dup2 <pipe> STDIN_FILENO:");
			if (dup2(fds_out[1], STDOUT_FILENO) != STDOUT_FILENO)
				eprintf("dup2 <pipe> STDOUT_FILENO:");
			close(fds_in[0]);
			close(fds_out[1]);
			execvp(*shell, (void *)shell);
			eprintf("execvp %s:", *shell);
		default:
			close(fds_in[0]);
			close(fds_out[1]);
			break;
		}
		pfds[0].fd = fds_in[1];
		pfds[0].events = POLLOUT;
		pfds[1].fd = fds_out[0];
		pfds[1].events = POLLIN;
		npfds = 2;
		lineno = 1;
		state = 0;
		while (npfds) {
			n = poll(pfds, npfds, -1);
			if (n < 0)
				eprintf("poll:");
			for (i = 0; i < n; i++) {
				if (!pfds[i].revents)
					continue;
				if (pfds[i].fd == fds_in[1]) {
					if (out_off == out_len) {
						if (close(fds_in[1]))
							eprintf("write <pipe>:");
						pfds[i] = pfds[--npfds];
						continue;
					}
					r = write(fds_in[1], &out_data[out_off], out_len - out_off);
					if (r <= 0)
						eprintf("write <pipe>:");
					out_off += (size_t)r;
				} else {
					r = read(fds_out[0], buffer, sizeof(buffer));
					if (r <= 0) {
						if (r < 0 || close(fds_out[0]))
							eprintf("read <pipe>:");
						pfds[i] = pfds[--npfds];
						continue;
					}
					len = (size_t)r;
					for (j = 0; j < len; j++) {
						switch (state) {
						case 0:
							no = 0;
							if (!buffer[j]) {
								state = 1;
							} else {
								state = 3;
								goto state3;
							}
							break;
						case 1:
							if (isdigit(buffer[j])) {
								if (buffer[j] == '0') {
									append(&in_data, &in_len, &in_size, "%c", 0);
									state = 3;
									goto state3;
								}
								state = 2;
							}
							/* fall through */
						case 2:
							if (isdigit(buffer[j])) {
								if (buffer[j] > (SIZE_MAX - (buffer[j] & 15)) / 10) {
									append(&in_data, &in_len, &in_size, "%c%zu", 0, no);
									state = 3;
									goto state3;
								}
								no = no * 10 + (buffer[j] & 15);
							} else if (!buffer[j]) {
								while (lineno < no) {
									append(&in_data, &in_len, &in_size, "%s\n", replacement);
									lineno += 1;
								}
								state = 3;
							} else {
								append(&in_data, &in_len, &in_size, "%c%zu", 0, no);
								state = 3;
								goto state3;
							}
							break;
						default:
						state3:
							if (in_len == in_size) {
								in_size += 4096;
								in_data = realloc(in_data, in_size);
								if (!in_data)
									eprintf("realloc:");
							}
							in_data[in_len++] = buffer[j];
							if (buffer[j] == '\n') {
								lineno += 1;
								state = 0;
							}
							break;
						}
					}
				}
			}
		}
		if (waitpid(pid, &status, 0) != pid)
			eprintf("waitpid %s <&status> 0:", *shell);
		if (status)
			return WIFEXITED(status) ? WEXITSTATUS(status) : 1;

		out_len = 0;
		out_off = 0;
		in_off = 0;
	}

	free(quotes);

	free(out_data);
	out_fd = xcreate(output_file);
	while (in_off < in_len) {
		r = write(out_fd, &in_data[in_off], in_len - in_off);
		if (r <= 0)
			eprintf("write %s:", output_file);
		in_off += (size_t)r;
	}
	if (close(out_fd))
		eprintf("write %s:", output_file);
	free(in_data);
	return 0;
}
