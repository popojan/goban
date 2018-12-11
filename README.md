# Red Carpet Goban

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
*   `boost >= 1.64`
*   `GLM`
*   `glyphy`
