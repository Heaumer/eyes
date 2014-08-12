#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if 0
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/inotify.h>

#define SEP '\t'

extern int snprintf(char *str, size_t size, const char *format, ...);

enum {
	Maxeyes		= 1024,
	Maxevents	= 32,
	Maxname		= 255,	/* sometimes MAX_NAME, MAXNAMLEN */
	Maxaction	= 1024
};

typedef struct eye eye;
struct eye {
	int		wd;						/* inotify watch descriptor */
	char	name[Maxname];			/* file name */
	char 	argv[Maxaction];	/* associated action */
};

static char *efile = "/etc/eyes.conf";
static eye eyes[Maxeyes];
static int neyes = 0;
static int ifd = 0;

#include "masks.h"

uint32_t
lookupmask(char *s)
{
	int i;

	for (i = 0; masks[i].name != NULL; i++) {
		if (strcmp(s, masks[i].name) == 0)
			return masks[i].value;
	}

	return 0;
}

void
rlookupmask(uint32_t m, char *dest, int n)
{
	int i;

	memset(dest, '\0', n);

	for (i = 0; masks[i].name != NULL; i++) {
		/*
		 * XXX all_events there all the time.
		 * close_write for close too, etc.
		 */
		if (m & masks[i].value) {
			strncat(dest, masks[i].name, n-1);
			strncat(dest, "|", n-1);
		}
	}

	/* remove last '|' if any */
	if (strlen(dest)-1 > 0)
		dest[strlen(dest)-1] = '\0';
}

uint32_t
getmask(char *m)
{
	uint32_t mask;
	char *p;

	mask = 0;

	for (;;) {
		p = strchr(m, '|');
		if (p == NULL)
			break;

		*p = '\0';

		mask |= lookupmask(m);

		m = p+1;
	}

	/* don't forget the last one */
	mask |= lookupmask(m);

	return mask;
}

char *
skipc(char *p, char c)
{
	while (*p == c)
		p++;

	return p;
}

void
doact(char *argv, char *fn, char *flags)
{
	char argv2[Maxaction];
	int i, j;

	memset(argv2, '\0', sizeof(argv2));

	/* substitute $$ for $, $@ for fn and $% for flags */
	for (i = j = 0; argv[i] != '\0' && j < Maxaction;) {
		if (argv[i] == '$') {
			if (argv[i+1] == '$') {
				argv2[j++] = '$';
				i += 2;
				continue;
			} else if (argv[i+1] == '@') {
				j += snprintf(argv2+j, sizeof(argv2)-1-j, "'%s'", fn);
				i += 2;
				continue;
			} else if (argv[i+1] == '%') {
				j += snprintf(argv2+j, sizeof(argv2)-1-j, "'%s'", flags);
				i += 2;
				continue;
			}
		}
		argv2[j++] = argv[i++];
	}

/*	fprintf(stderr, "Executing on %s: '%s'\n", fn, argv2);*/
	system(argv2);
}

int
loadeyes(char *fn, int fd)
{
	char buf[BUFSIZ];
	uint32_t mask;
	char *p, *q;
	int wd, n;
	FILE *f;

	f = fopen(fn, "r");
	if (f == NULL) {
		perror("fopen");
		return -1;
	}

	n = 0;

	do {
		n++;
		memset(buf, sizeof(buf), '\0');
		if (fgets(buf, sizeof(buf), f) == NULL) {
			break;
		}

		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';

		/* comments */
		if (buf[0] == '#')
			continue;

		/* filename */
		p = strchr(buf, SEP);
		if (p == NULL) {
			fprintf(stderr, "warning: bad format on %s:%d", fn, n);
			continue;
		}
		*p++ = '\0';
		p = skipc(p, SEP);

		/* mask or action if no mask  */
		q = strchr(p, SEP);
		if (q == NULL) {
			mask = lookupmask("default");
			q = p;
		} else {
			*q++ = '\0';
			q = skipc(q, SEP);
			mask = getmask(p);
		}

/*		fprintf(stderr, "%s, %d (%s), %s\n", buf, mask, p, q);*/

		wd = inotify_add_watch(fd, buf, mask);
		if (wd == -1) {
			perror("inotify_add_watch");
			return -1;
		}
		eyes[neyes].wd = wd;
		strncpy(eyes[neyes].name, buf, sizeof(eyes[neyes].name)-1);
		strncpy(eyes[neyes].argv, q, sizeof(eyes[neyes].argv)-1);
		neyes++;

	} while(!feof(f) && neyes < Maxeyes);

	fclose(f);

	return 0;
}

#if 0
void
shandler(int s)
{
	int i;

	switch (s) {
	case SIGTERM:
	case SIGHUP:
		break;
	case SIGUSR1:
/*		fprintf(stderr, "sigusr1\n");*/
		/* reset */
		for (i = 0; i < neyes; i++) {
			inotify_rm_watch(ifd, eyes[i].wd);
		}
		neyes = 0;
	
		/* and reload */
		if (loadeyes(efile, ifd) < 0) {
			exit(1);
		}
		eventloop();
		break;
	}
}
#endif

int
geteye(int wd)
{
	int i;

	for (i = 0; i < neyes; i++)
		if (eyes[i].wd == wd)
			return i;

	return -1;
}

void
help(char *argv0)
{
	fprintf(stderr, "%s [-f] [eyes.conf]\n", argv0);
	fprintf(stderr, "%s [-h]\n", argv0);
	exit(0);
}

void
eventloop(void)
{
	char buf[Maxevents*sizeof(struct inotify_event)];
	struct inotify_event *ev;
	char mask[BUFSIZ];
	ssize_t n;
	char *p;
	int i;
	eye e;

	for (;;) {
		n = read(ifd, buf, sizeof(buf));
		if (n <= 0) {
			break;
		}
		for (p = buf; p < buf+n; p += sizeof(struct inotify_event)+ev->len) {
			ev = (struct inotify_event *)p;
/*			fprintf(stderr, "wd: %d\n", geteye(ev->wd));*/
			i = geteye(ev->wd);
			/* XXX bad event; happens when reloading eyes */
			if (i == -1)
				continue;
			e = eyes[i];
/*			fprintf(stderr, "mask: %ul, name: %s\n", ev->mask, e.name);*/
			rlookupmask(ev->mask, mask, sizeof(mask));
			doact(e.argv, e.name, mask);
		}
	}

	for (i = 0; i < neyes; i++)
		inotify_rm_watch(ifd, eyes[i].wd);
}

int
main(int argc, char *argv[])
{
	int fg;
	int i;

	fg = 0;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0)
			help(argv[0]);
		else if (strcmp(argv[i], "-f") == 0)
			fg = 1;
		else
			efile = argv[i];
	}

	if (fg == 0)
	switch(fork()) {
	case -1:
		perror("fork");
		return 1;
	case 0:
		break;
	default:
		return 0;
	}

#if 0
	signal(SIGUSR1, shandler);
	signal(SIGHUP, shandler);
	signal(SIGTERM, shandler);
#endif

	ifd = inotify_init();
	if (ifd == -1) {
		perror("inotify_init");
		return 1;
	}

	if (loadeyes(efile, ifd) < 0)
		return 1;

	eventloop();

	return 0;
}
