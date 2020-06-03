all: dependencies build
	cd build && make -j4

dependencies:
	make -j4 -C deps

build: FORCE
	mkdir -p build && cd build && \
	cmake .. && make -j4 && cd ..
	ln -fs build/goban .

glad:
	glad --out-path src/glad --profile="compatibility" --api="gl=3.1" --generator="c" --spec="gl" --extensions="" --reproducible
	glad --out-path src/glad --api="wgl=1.0" --generator="c" --spec="wgl" --extensions="WGL_ARB_extensions_string" --reproducible

FORCE: ;
