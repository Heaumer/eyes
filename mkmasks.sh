#!/bin/sh
# Create masks.h from a <sys/inotify.h>
# using a map instead of raw printing in awk
# avoid double definitions (happened for close here)

header=/usr/include/sys/inotify.h

if [ "$1" != "" ]; then
	header=$1
fi

awk '
/^#define[ \t]+IN/ {
	masks[tolower(substr($2, 4))] = $2
}

END {
	masks["default"] = "IN_MODIFY"

	print "struct {"
	print "	char		*name;"
	print "	uint32_t	value;"
	print "} masks[] = {"
	for (key in masks) {
		print "\t{ \"" key "\",\t\t" masks[key] " },"
	}
	print "\t{ NULL,\t\t0 },"
	print "};"
}
' $header > masks.h
