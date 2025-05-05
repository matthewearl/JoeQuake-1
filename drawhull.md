## Collision hull visualisation

This is a branch of JoeQuake that supports rendering of the collision hull with
the `r_draw_hull` cvar.

:warning: IT IS NOT VERY TESTED, I TAKE NO RESPONSIBILITY IF IT DOES NOT WORK /
DELETES YOUR HARD DRIVE / MAKES YOUR COMPUTER CATCH FIRE.

Furthermore, I don't intend to maintain this branch.  If there is sufficient
interest though I may tidy it up and submit a PR to be merged.

That said, if you would like to give it a go a [windows build is available
here](https://github.com/matthewearl/JoeQuake-1/actions/runs/14837808530).  You'll
need to be logged in for the download links to work.

The windows build is based on an unreleased version of JoeQuake which requires
additional DLLs to be installed since they are not yet packaged in any official
release.  I needed these particular files:
- `libmad-0.dll`
- `libvorbisfile-3.dll`
- `libvorbis-0.dll`
- `libogg-0.dll`

...which you can download from
[here](https://github.com/shalrathy/quakespasm-shalrathy/tree/master/Windows/codecs/x86).

A [version that compiles on linux (with SDL) is available here](https://github.com/matthewearl/JoeQuake-1/tree/portals).

You can also just browse the source.  The main two files are:
- [hullmesh.c](trunk/hullmesh.c):  Module for converting the player hull bsp tree into a mesh.
- [gl_hullmesh.c](trunk/gl_hullmesh.c):  Module for rendering the above mesh
  geometry.
