# Red Carpet Goban

Ray traced 3D igo/baduk/weiqi/go board and GUI rendered using GLSL.

External GTP engines supported, GnuGo required as the main engine.

Targets Windows 7/10 and Linux.

For windows binaries and more screenshots see [hraj.si/goban](http://hraj.si/goban).

Command to run Chinese translation with suppresed logging:

```
./goban --verbosity error --config data/zh/config.json
```

![screenshot](/res/screenshot.png)

## Dependencies
* `boost`
* `clipp`
* `freetype2`
* `GLM`
* `glyphy`
* `libRocket`
* `libsgfcplusplus`
* `libsndfile`
* `nlohmann::json`
* `portaudio`
* `spdlog`
