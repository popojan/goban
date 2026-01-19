# Red Carpet Goban

Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Targets Windows, Linux, and macOS.

For windows binaries and more screenshots see [hraj.si/goban](http://hraj.si/goban).

Command to run Chinese translation with suppressed logging:

```
./goban --verbosity error --config data/zh/config.json
```

![screenshot](/res/screenshot.png)

## Dependencies
* `clipp`
* `freetype2`
* `GLFW`
* `GLM`
* `glyphy`
* `libsgfcplusplus`
* `libsndfile`
* `nlohmann::json`
* `portaudio`
* `RmlUi`
* `spdlog`
