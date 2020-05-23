all: dependencies build
	cd build && make -j4

dependencies:
	make -j4 -C deps

build: FORCE
	mkdir -p build && cd build && \
	cmake .. && make -j4 && cd ..
	ln -fs build/goban .

FORCE: ;
