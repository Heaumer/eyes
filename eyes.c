#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/inotify.h>

#define SEP '\t'

enum {
	Maxeyes		= 1024,
	Maxevents	= 32,
	Maxname		= 255,	/* sometimes MAX_NAME, MAXNAMLEN */
	Maxaction	= 64
};

typedef struct eye eye;
struct eye {
	int		wd;					/* inotify watch descriptor */
	char	name[Maxname];		/* file name */
	char 	action[Maxaction];	/* associated action */
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

void
doact(char *action, char *fn, char *flags)
{
	switch(fork()) {
	case -1:
		perror("fork");
		break;
	case 0:
		execlp(action, action, fn, flags, NULL);
		perror("execlp");
		break;
	default:
		wait(NULL);
	}

	return;
}

char *
skipsep(char *p)
{
	while (*p == SEP)
		p++;

	return p;
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
		p = skipsep(p);

		/* mask or action if no mask  */
		q = strchr(p, SEP);
		if (q == NULL) {
			mask = lookupmask("default");
			q = p;
		} else {
			*q++ = '\0';
			q = skipsep(q);
			mask = getmask(p);
		}

		wd = inotify_add_watch(fd, buf, mask);
		if (wd == -1) {
			perror("inotify_add_watch");
			return -1;
		}
/*		fprintf(stderr, "%s, %d, %s\n", buf, mask, q);*/
		eyes[neyes].wd = wd;
		strncpy(eyes[neyes].name, buf, sizeof(eyes[neyes].name)-1);
		strncpy(eyes[neyes].action, q, sizeof(eyes[neyes].action)-1);
		neyes++;

	} while(!feof(f) && neyes < Maxeyes);

	fclose(f);

	return 0;
}

void
reload(int _)
{
	int i;

	_=_;

	/* reset */
	for (i = 0; i < neyes; i++) {
		inotify_rm_watch(ifd, eyes[i].wd);
	}
	neyes = 0;

	/* and reload */
	if (loadeyes(efile, ifd) < 0) {
		exit(1);
	}
}

void ign(int _) {_=_;}

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

int
main(int argc, char *argv[])
{
	char buf[Maxevents*sizeof(struct inotify_event)];
	struct inotify_event *ev;
	char mask[BUFSIZ];
	ssize_t n;
	int fg, i;
	char *p;
	eye e;

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
		return 0;
	default:
		break;
	}

	signal(SIGUSR1, reload);
	signal(SIGHUP, ign);
	signal(SIGTERM, ign);

	ifd = inotify_init();
	if (ifd == -1) {
		perror("inotify_init");
		return 1;
	}

	if (loadeyes(efile, ifd) < 0)
		return 1;


	for (;;) {
		n = read(ifd, buf, sizeof(buf));
		if (n <= 0) {
			break;
		}
		for (p = buf; p < buf+n; p += sizeof(struct inotify_event)+ev->len) {
			ev = (struct inotify_event *)p;
/*			fprintf(stderr, "%d\n", geteye(ev->wd)); */
			i = geteye(ev->wd);
			/* XXX bad event; happens when reloading eyes */
			if (i == -1)
				continue;
			e = eyes[i];
/*			fprintf(stderr, "%ul\n", ev->mask); */
			rlookupmask(ev->mask, mask, sizeof(mask));
/*			fprintf(stderr, "%s(%s, %s)\n", e.action, e.name, mask); */
			doact(e.action, e.name, mask);
		}
	}

	for (i = 0; i < neyes; i++) {
		inotify_rm_watch(ifd, eyes[i].wd);
	}

	return 0;
}
