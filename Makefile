all: build
	cd build && make

dependencies:
	make -C deps

build: dependencies
	mkdir -p build && cd build && \
	cmake .. && make && cd ..
	cp build/goban .
