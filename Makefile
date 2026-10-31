# zrn-sdk: make (libzrnui), make check (all tests), make examples (.zexec of
# each example), make renders (docs/renders). At most -j4 on the laptop.
ZRN = tools/zrn
TESTBIN = build/tests
EXAMPLES = hello-c settings-c hello-cpp hello-py hello-rust

all:
	$(MAKE) -C libzrnui -j4

$(TESTBIN)/test_core: tests/test_core.c all
	@mkdir -p $(TESTBIN)
	cc -O1 -g -Wall -Ilibzrnui/include tests/test_core.c -Llibzrnui/build -lzrnui -lpng -lm \
		-Wl,-rpath,$(CURDIR)/libzrnui/build -o $@

check: $(TESTBIN)/test_core
	EGL_LOG_LEVEL=fatal $(TESTBIN)/test_core $(TESTBIN)
	python3 -I tests/test_tools.py
	cd rust && cargo test --offline --quiet
	c++ -std=c++17 -fsyntax-only -Wall -Wextra -Ilibzrnui/include examples/hello-cpp/main.cpp

examples: all
	for e in $(EXAMPLES); do $(ZRN) -C examples/$$e pack || exit 1; done

R = docs/renders
renders: all
	@mkdir -p $(R)
	$(ZRN) -C examples/settings-c render --scale 4 --style dotmatrix $(R)/dotmatrix-settings.png
	$(ZRN) -C examples/settings-c render --scale 4 --style dotmatrix --page Wi-Fi $(R)/dotmatrix-wifi.png
	$(ZRN) -C examples/settings-c render --scale 4 --style metro $(R)/metro-settings.png
	$(ZRN) -C examples/settings-c render --scale 4 --style metro --page Wi-Fi $(R)/metro-wifi.png
	$(ZRN) -C examples/hello-c render --scale 2 $(R)/hello-c.png
	$(ZRN) -C examples/hello-cpp render --scale 2 $(R)/hello-cpp.png
	$(ZRN) -C examples/hello-py render --scale 2 $(R)/hello-py.png
	$(ZRN) -C examples/hello-rust render --scale 2 $(R)/hello-rust.png
	$(ZRN) -C examples/hello-c render --scale 2 --style metro $(R)/hello-c-metro.png

clean:
	$(MAKE) -C libzrnui clean
	rm -rf build

.PHONY: all check examples renders clean
