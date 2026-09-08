#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static void
sockpath(char *buf, size_t len)
{
	const char *env = getenv("MITHRIL_SOCKET");
	const char *disp = getenv("DISPLAY");

	if (env && *env) {
		snprintf(buf, len, "%s", env);
		return;
	}
	snprintf(buf, len, "/tmp/mithril%s.sock", disp ? disp : "");
}

static int
sendall(int fd, const char *buf, size_t len)
{
	size_t off = 0;
	while (off < len) {
		ssize_t w = write(fd, buf + off, len - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)w;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un addr;
	char path[256];
	char rbuf[512];
	int fd, i;
	ssize_t n;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <command> [args...]\n", argv[0]);
		return 2;
	}

	sockpath(path, sizeof path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return 1;
	}

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof addr.sun_path) {
		fprintf(stderr, "%s: socket path too long: %s\n", argv[0], path);
		close(fd);
		return 1;
	}
	strcpy(addr.sun_path, path);

	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
		fprintf(stderr, "%s: cannot connect to %s: %s\n",
			argv[0], path, strerror(errno));
		fprintf(stderr, "(is mithril running, and was it built with IPC support?)\n");
		close(fd);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		if (sendall(fd, argv[i], strlen(argv[i]) + 1) == -1) {
			perror("write");
			close(fd);
			return 1;
		}
	}
	shutdown(fd, SHUT_WR);

	while ((n = read(fd, rbuf, sizeof rbuf)) > 0)
		fwrite(rbuf, 1, (size_t)n, stdout);

	close(fd);
	return 0;
}

