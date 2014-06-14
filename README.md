# Description
Eyes plays a similar role than
[incron](http://inotify.aiken.cz/?section=incron&page=doc).
It's however much less sophisticated.

See eyes.8 for details.

eyes.conf gives a set of eyes which must be run as root on a
slackware system; ueyes.conf a test one, can be used used as non-root.

# Ideas
May use pnotify for *BSD compatibility.
Something similar may have been hackable in bash around inotifywatch(1).

Nice to use on server, to mail new configuration files to administrators
when modified for instance.

Or to automatically reload programs when their configuration gets modified.

# TODO
Reload upon signal (commented out; working only once).
