all: build
	cd build && make -j4

dependencies:
	make -j4 -C deps

build: dependencies
	mkdir -p build && cd build && \
	cmake .. && make -j4 && cd ..
	ln -fs build/goban .
