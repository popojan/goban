# Red Carpet Goban

Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Builds on Windows 7/10 and Linux.

For windows binaries and more screenshots see [hraj.si/goban](http://hraj.si/goban).

[![screen06](/res/screen06_s.png)](https://www.youtube.com/watch?v=S3kmepVEipk)]

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
