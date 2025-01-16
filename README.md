# Unofficial Firefox GTK4 port

## Build
```
$ cp mozconfig.proto mozconfig
$ ./mach configure
$ ./mach build
```

## Coding
This port is using the widget/gtk4 directory exclusively and outside of it you must use "#ifdef MOZ_GTK4".
All code must be fully compatible with the official Mozilla GTK3 port.

