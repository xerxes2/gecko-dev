# Unofficial Firefox GTK4 port

## Build
```
$ cp mozconfig.proto mozconfig
$ ./mach configure
$ ./mach build
```

## Coding
This port is using the widget/gtk4 directory exclusively and outside of it you must use "#ifdef MOZ_GTK4".
It's focused on Wayland so X11 is disabled, for now at least. All code must be fully compatible with the
official Mozilla GTK3 port.

All contributions very much welcome.

## Discussion
Ticket at Mozilla: https://bugzilla.mozilla.org/show_bug.cgi?id=1701123

## Help
GTK porting doc: https://docs.gtk.org/gtk4/migrating-3to4.html

