all: libRocket glyphy

libRocket:
	git clone https://github.com/popojan/libRocket.git
	cd libRocket && git checkout rem-fix && mkdir -p buildDir && cd buildDir && cmake ../Build -DBUILD_SHARED_LIBS=0 && make

glyphy:
	git clone https://github.com/behdad/glyphy.git
	cd glyphy && ./autogen.sh && ./configure --enable-static && make
