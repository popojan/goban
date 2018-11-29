# Red Carpet Goban
[![Build Status](https://travis-ci.com/popojan/goban.svg?branch=automate-linux-build)](https://travis-ci.com/popojan/goban)

Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Builds on Windows 7/10 and Linux.

[![screen06](/res/screen06_s.png)](https://www.youtube.com/watch?v=S3kmepVEipk)

For more screenshots and win32 binaries see [hraj.si](http://hraj.si).

## Tasks
* automate build process
* refactor and clean-up code
* add SGF support to replay and save games
* add stone placement sound
* add shader configuration via GUI or xml
* publish anaglyphic version of the shader

## Dependencies
* `glew`
* `freetype2`
* `libRocket`
* `boost`
* `boost.process`
* `GLM`
* `glyphy`
