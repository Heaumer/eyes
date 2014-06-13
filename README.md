# Description
Eyes plays a similar role than
[incron](http://inotify.aiken.cz/?section=incron&page=doc).
It's however much less sophisticated.

Command line option:

	eyes [-f] [eyes.conf]
	eyes [-h]

The *-f* launch in foreground; eyes.conf specify the path to the
configuration. Default is no *-f* and */etc/eyes.conf*.

# Eyes.conf
Text file CSV-like, column separator being [\t]+. Three columns:

	/path/to/file		mask		action

Mask is a string of lowercase separated by '|' flag names.
For instance, use

- `close` instead of `IN_CLOSE`;
- `close|modify` instead of `IN_CLOSE|IN_MODIFY`;
- etc.

Name are automatically generated from <sys/inotify.h> via
mkmasks.sh.

Action is a program which can be found in $PATH (execlp);
it's called with filename and event mask (mask's column format)
as arguments.

No argument may be specified within the action field;
include them in a shell script if you do need them.

Config may be reload by sending a SIGUSR1.

# Building

	% ./mkmasks.sh && cc -Wall -Wextra -ansi -pedantic eyes.c -o eyes && ./eyes

Reload config with:

	% pkill -SIGUSR1 eyes

# Ideas
May use pnotify for *BSD compatibility.
Something similar may have been hackable in bash around inotifywatch(1).

Nice to use on server, to mail new configuration files to administrators
when modified for instance.

# TODO
clean sample eyes.conf
makefile, man page
