# Description
Eyes plays a similar role than
[incron](http://inotify.aiken.cz/?section=incron&page=doc), or
[gamin](https://people.gnome.org/~veillard/gamin/).

It's however much less sophisticated, thus much smaller (hundreds vs. thousands
of lines).

See eyes.8 for details.

eyes.conf gives a set of eyes which must be run as root on a
slackware system; ueyes.conf is a test one, runnable as non-root.

# TODO
Reload upon signal (commented out; working only once), or use named fifo.

Make it portable, at least to other UNIX:

* [pnotify](https://pnotify.googlecode.com/svn/trunk/README),
* [fsnotifier](https://github.com/skirge/fsnotifier-freebsd/),
* maybe simplier: file pooling (gamin can it).

Allow file globing (/path/to/foo/\*).
