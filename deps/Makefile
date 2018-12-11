all: libRocket glyphy spdlog

libRocket:
	git clone https://github.com/libRocket/libRocket.git
	cd libRocket && mkdir -p buildDir && cd buildDir && cmake ../Build && make

glyphy:
	git clone https://github.com/behdad/glyphy.git
	cd glyphy && ./autogen.sh && ./configure --enable-static && make

spdlog:
	git clone https://github.com/gabime/spdlog.git
