all: build
build:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
test:
	ctest --test-dir build --output-on-failure
install:
	cmake --install build
docs:
	doxygen Doxyfile
bench:
	cmake --build build --target bench_inventory
coverage:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_COVERAGE=ON && cmake --build build && ctest --test-dir build
