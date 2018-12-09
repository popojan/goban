# Red Carpet Goban &nbsp; [![Build Status](https://travis-ci.com/popojan/goban.svg?branch=master)](https://travis-ci.com/popojan/goban) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/152679e4f18742f3af1997ce6f0517e9)](https://www.codacy.com/app/popojan/goban?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=popojan/goban&amp;utm_campaign=Badge_Grade)

Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Builds on Windows 7/10 and Linux.

For windows binaries and more screenshots see [hraj.si/goban](http://hraj.si/goban).

[![screen06](/res/screen06_s.png)](https://www.youtube.com/watch?v=S3kmepVEipk)]

## Tasks
*   refactor and clean-up code
*   add SGF support to replay and save games
*   add stone placement sound
*   add shader configuration via GUI or xml
*   publish anaglyphic version of the shader

## Dependencies
*   `glew`
*   `freetype2`
*   `libRocket`
*   `boost`
*   `GLM`
*   `glyphy`
