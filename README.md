# Red Carpet Goban [<img src='https://travis-ci.com/popojan/goban.svg?branch=master' alt='Build Status' align='right'>](https://travis-ci.com/popojan/goban)
  
Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Builds on Windows 7/10 and Linux.

For windows binaries and more screenshots see [hraj.si/goban](http://hraj.si/goban).

[![screenshot](/res/screenshot.png)](https://hraj.si/goban/image/red-carpet-goban-0.png)

## Tasks
*   refactor and clean-up code
*   redesign GUI allowing for new features
*   add SGF support to replay and save games
*   enable shader templating to share common code between shaders
*   enable shader configuration via GUI or configuration files
*   allow external goban control via GTP protocol

## Dependencies
*   `glew`
*   `freetype2`
*   `libRocket`
*   `boost >= 1.64`
*   `GLM`
*   `glyphy`
*   `libsndfile`
*   `portaudio`
