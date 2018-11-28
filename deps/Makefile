all: libRocket glyphy boost-process

libRocket:
	git clone https://github.com/libRocket/libRocket.git
	cd libRocket && mkdir -p build && cd build && cmake ../Build && make

glyphy:
	git clone https://github.com/behdad/glyphy.git
	cd glyphy && ./autogen.sh && make

boost-process:
	git clone https://github.com/BorisSchaeling/boost-process.git
