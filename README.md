# Unofficial Firefox GTK4 port

## Download
The smallest download you can do is cloning with depth 1 (around 840 MB):
```
$ git clone --depth 1 https://github.com/xerxes2/gecko-dev.git
```

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

## Roadmap
1. Make it build (done)
2. Get it to start (80%)
3. Make the upstart sequence work properly (60%)
4. Fix input events
    * Keyboard (20%)
    * Pointer (20%)
    * Touch (20%)
5. Implement missing features
    * Clipboard (0%)
    * Drag-and-drop (0%)
    * Filepicker (0%)
    * Appchooser (0%)
    * Colorpicker (0%)
    * Emojipicker (0%)

## Discussion
Ticket at Mozilla: https://bugzilla.mozilla.org/show_bug.cgi?id=1701123

## Help
GTK4 porting doc: https://docs.gtk.org/gtk4/migrating-3to4.html

