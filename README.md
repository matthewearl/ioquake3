[![No Maintenance Intended](http://unmaintained.tech/badge.svg)](http://unmaintained.tech/)

# Overbounce visualizer mod

A rough and ready tool for visualizing overbounces in Quake 3.
[See my video for an example of it in action.](https://www.youtube.com/watch?v=WmO2cdTU7EM&t=322s)
Note that I made this work just well enough to make the above video, don't
expect production quality!

It is fairly resource intensive at the moment because I'm using the same routine
that draws player shadows and bullet marks in order to clip the shader to the
level geometry.  I'm also doing a rather dumb grid-based tracing to detect
nearby surfaces at the right heights.  It would be much more efficient to modify
the engine to simply draw the overlay on triangles as they are drawn, however, I
wanted to stick to modifying just the QVMs.

Maybe there's a better way to do this while sticking to QVMs?  If so, please
make a fork.  I don't plan on maintaining this code.

For the original ioquake3 README [click here.](README-original.md)

## Installation

1. Download a zip file from the [releases page](https://github.com/matthewearl/ioquake3/releases),
and unpack it into your game directory (ie. into the directory above your
`baseq3`.  directory).  This should create an `ob-hud` directory alongside the
`baseq3` directory
1. Launch Quake 3, and select `ob-hud` from the mods menu.
1. Run `cg_overbounce_hud 1`, `pmove_fixed 1`, `pmove_msec 8` in the console,
   and then create a server, eg. `devmap q3dm13`.

## Known issues

- It's pretty slow.  I have a fairly modern machine (for Quake 3), and FPS can
  drop to sub-100 at times.
- The overlay can sometimes look messed up, missing some parts.  I suspect this
  is due to me (ab)using the same routine that draws bullet marks and shadows.
  Maybe there's a way of tweaking it to get it to work?
- Currently only works for 8ms frame times (`pmove_fixed 8`).  It should be
  pretty easy to make it work for different values.
