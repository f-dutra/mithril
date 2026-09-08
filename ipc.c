#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "util.h"
#include "extern.h"
#include "ipc.h"

#define IPC_MAXMSG    4096
#define IPC_MAXARGS   2
#define IPC_REPLYLEN  256
#define IPC_SOCKENV   "MITHRIL_SOCKET"

typedef int (*IpcHandler)(char **argv);

static int h_closesel(char **argv);
static int h_focusmon(char **argv);
static int h_focusstack(char **argv);
static int h_incmfact(char **argv);
static int h_incnmaster(char **argv);
static int h_movekeyboard(char **argv);
static int h_quit(char **argv);
static int h_resizekeyboard(char **argv);
static int h_restart(char **argv);
static int h_sendtomon(char **argv);
static int h_sendtows(char **argv);
static int h_setlayout(char **argv);
static int h_swaptiled(char **argv);
static int h_togglegaps(char **argv);
static int h_togglefloating(char **argv);
static int h_togglefullscr(char **argv);
static int h_togglemaximize(char **argv);
static int h_toggleminimize(char **argv);
static int h_togglesticky(char **argv);
static int h_view(char **argv);

static const struct {
	const char *name;
	const char *usage;
	int nargs;
	IpcHandler handler;
} commands[] = {
	{ "close",       	"close",   				 0, h_closesel       },
	{ "focusmon",       "focusmon <next/prev>", 		 1, h_focusmon       },
	{ "focus",          "focus <next/prev>", 		 1, h_focusstack     },
	{ "incmfact",       "incmfact <float>", 	      1, h_incmfact       },
	{ "incnmaster",     "incnmaster <int>", 	      1, h_incnmaster     },
	{ "move",           "move <up/down/left/right>",   1, h_movekeyboard   },
	{ "quit",           "quit",					 0, h_quit           },
	{ "resize",         "resize <up/down/left/right>", 1, h_resizekeyboard },
	{ "restart",        "restart",		 		 0, h_restart        },
	{ "sendmon",      	"sendmon <monitor>", 		 1, h_sendtomon      },
	{ "send",       	"send <workspace>",  		 1, h_sendtows       },
	{ "setlayout",      "setlayout <name>",  		 1, h_setlayout      },
	{ "swap",           "swap <next/prev>",	 		 1, h_swaptiled      },
	{ "togglegaps",     "togglegaps",	   	 		 0, h_togglegaps     },
	{ "togglefloating", "togglefloating", 	 		 0, h_togglefloating },
	{ "fullscreen",     "fullscreen", 				 0, h_togglefullscr  },
	{ "maximize",       "maximize",				 0, h_togglemaximize },
	{ "minimize",       "minimize",				 0, h_toggleminimize },
	{ "sticky",         "sticky", 				 0, h_togglesticky   },
	{ "view",           "view <workspace>",			 1, h_view           },
};

static int ipcfd = -1;
static char sockpath[256];

static int
parse_dir(const char *s, int *out)
{
	if (!strcmp(s, "next"))
		*out = 1;
	else if (!strcmp(s, "prev"))
		*out = -1;
	else if (!parseint(s, out) || *out == 0)
		return -1;
	return 0;
}

static int
parse_movedir(const char *s, int *out)
{
	if (!strcmp(s, "left"))
		*out = MoveLeft;
	else if (!strcmp(s, "right"))
		*out = MoveRight;
	else if (!strcmp(s, "up"))
		*out = MoveUp;
	else if (!strcmp(s, "down"))
		*out = MoveDown;
	else
		return -1;
	return 0;
}

int
h_closesel(char **argv)
{
	Arg arg = {0};
	closesel(&arg);
	return 0;
}

int
h_focusmon(char **argv)
{
	Arg arg = {0};
	if (parse_dir(argv[0], &arg.i))
		return -1;
	focusmon(&arg);
	return 0;
}

int
h_focusstack(char **argv)
{
	Arg arg = {0};
	if (parse_dir(argv[0], &arg.i))
		return -1;
	focusstack(&arg);
	return 0;
}

int
h_incmfact(char **argv)
{
	Arg arg = {0};
	if (!parsedouble(argv[0], &arg.f))
		return -1;
	incmfact(&arg);
	return 0;
}

int
h_incnmaster(char **argv)
{
	Arg arg = {0};
	if (!parseint(argv[0], &arg.i))
		return -1;
	incnmaster(&arg);
	return 0;
}

int
h_movekeyboard(char **argv)
{
	Arg arg = {0};
	if (parse_movedir(argv[0], &arg.i))
		return -1;
	movekeyboard(&arg);
	return 0;
}

int
h_quit(char **argv)
{
	Arg arg = {0};
	quit(&arg);
	return 0;
}

int
h_resizekeyboard(char **argv)
{
	Arg arg = {0};
	if (parse_movedir(argv[0], &arg.i))
		return -1;
	resizekeyboard(&arg);
	return 0;
}

int
h_restart(char **argv)
{
	Arg arg = { .i = 1};
	quit(&arg);
	return 0;
}

int
h_sendtomon(char **argv)
{
	Arg arg = {0};
	if (parse_dir(argv[0], &arg.i))
		return -1;
	sendtomon(&arg);
	return 0;
}

int
h_sendtows(char **argv)
{
	Arg arg = {0};
	if (!parseint(argv[0], &arg.i) || arg.i < 1)
		return -1;
	sendtows(&arg);
	return 0;
}

int
h_setlayout(char **argv)
{
	Arg arg = {0};
	arg.v = argv[0];
	setlayout(&arg);
	return 0;
}

int
h_swaptiled(char **argv)
{
	Arg arg = {0};
	if (parse_dir(argv[0], &arg.i))
		return -1;
	swaptiled(&arg);
	return 0;
}

int
h_togglegaps(char **argv)
{
	Arg arg = {0};
	togglegaps(&arg);
	return 0;
}

int
h_togglefloating(char **argv)
{
	Arg arg = {0};
	togglefloating(&arg);
	return 0;
}

int
h_togglefullscr(char **argv)
{
	Arg arg = {0};
	togglefullscr(&arg);
	return 0;
}

int
h_togglemaximize(char **argv)
{
	Arg arg = {0};
	togglemaximize(&arg);
	return 0;
}

int
h_toggleminimize(char **argv)
{
	Arg arg = {0};
	toggleminimize(&arg);
	return 0;
}

int
h_togglesticky(char **argv)
{
	Arg arg = {0};
	togglesticky(&arg);
	return 0;
}

int
h_view(char **argv)
{
	Arg arg = {0};
	if (!parseint(argv[0], &arg.i) || arg.i < 1)
		return -1;
	view(&arg);
	return 0;
}

void
ipcsockpath(char *buf, size_t len)
{
	const char *env = getenv(IPC_SOCKENV);
	const char *disp = getenv("DISPLAY");

	if (env && *env) {
		snprintf(buf, len, "%s", env);
		return;
	}
	snprintf(buf, len, "/tmp/mithril%s.sock", disp ? disp : "");
}

int
ipc_init(void)
{
	struct sockaddr_un addr;

	ipcsockpath(sockpath, sizeof sockpath);

	if (strlen(sockpath) >= sizeof addr.sun_path) {
		fprintf(stderr, "mithril: ipc: socket path too long: %s\n", sockpath);
		sockpath[0] = '\0';
		return -1;
	}

	if ((ipcfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "mithril: ipc: socket: %s\n", strerror(errno));
		sockpath[0] = '\0';
		return -1;
	}

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sockpath);

	unlink(sockpath); /* remove stale socket from previous run */

	if (bind(ipcfd, (struct sockaddr *)&addr, sizeof addr) == -1) {
		fprintf(stderr, "mithril: ipc: bind %s: %s\n", sockpath, strerror(errno));
		goto error;
	}

	if (chmod(sockpath, S_IRUSR | S_IWUSR) == -1) {
		fprintf(stderr, "mithril: ipc: chmod %s: %s\n", sockpath, strerror(errno));
		goto error;
	}

	if (listen(ipcfd, 8) == -1) {
		fprintf(stderr, "mithril: ipc: listen: %s\n", strerror(errno));
		goto error;
	}

	return ipcfd;

error:
	close(ipcfd);
	unlink(sockpath);
	ipcfd = -1;
	sockpath[0] = '\0';
	return -1;
}

void
ipc_cleanup(void)
{
	if (ipcfd >= 0) {
		close(ipcfd);
		ipcfd = -1;
	}
	if (sockpath[0]) {
		unlink(sockpath);
		sockpath[0] = '\0';
	}
}

int
ipc_getfd(void)
{
	return ipcfd;
}

ssize_t
readmsg(int fd, char *buf, size_t cap)
{
	size_t total = 0;
	ssize_t n;

	while (total < cap) {
		n = read(fd, buf + total, cap - total);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			break; /* client is done sending */
		total += (size_t)n;
	}
	return (ssize_t)total;
}

void
sendreply(int fd, const char *msg)
{
	size_t len = strlen(msg);
	while (len > 0) {
		ssize_t w = write(fd, msg, len);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		msg += w;
		len -= (size_t)w;
	}
}

void
ipc_handle(void)
{
	int cfd, i, argc;
	char buf[IPC_MAXMSG];
	char *argv[IPC_MAXARGS];
	char reply[IPC_REPLYLEN];
	ssize_t n;

	if (ipcfd < 0)
		return;

	if ((cfd = accept(ipcfd, NULL, NULL)) == -1) {
		if (errno != EINTR && errno != EAGAIN)
			fprintf(stderr, "mithril: ipc: accept: %s\n", strerror(errno));
		return;
	}

	n = readmsg(cfd, buf, sizeof buf - 1);
	if (n < 0) {
		close(cfd);
		return;
	}
	buf[n] = '\0';

	argc = 0;
	for (i = 0; i < (int)n; ) {
		if (argc >= IPC_MAXARGS) {
			snprintf(reply, sizeof reply, "mithril: ipc: too many arguments\n");
			sendreply(cfd, reply);
			close(cfd);
			return;
		}
		argv[argc++] = buf + i;
		i += (int)strlen(buf + i) + 1;
	}

	if (argc == 0) {
		snprintf(reply, sizeof reply, "mithril: ipc: empty command\n");
		sendreply(cfd, reply);
		close(cfd);
		return;
	}

	for (i = 0; i < (int)LENGTH(commands); i++) {
		if (strcmp(argv[0], commands[i].name))
			continue;

		if (argc - 1 != commands[i].nargs
		|| commands[i].handler(argv + 1) != 0) {
			snprintf(reply, sizeof reply, "mithrilctl: usage: %s\n", commands[i].usage);
			sendreply(cfd, reply);
		}
		close(cfd);
		return;
	}

	snprintf(reply, sizeof reply, "mithrilctl: unknown command '%.220s'\n", argv[0]);
	sendreply(cfd, reply);
	close(cfd);
}

