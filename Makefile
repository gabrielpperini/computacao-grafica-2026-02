./bin/Linux/main: src/* include/* third_party/imgui/* third_party/imgui/backends/* tests/* CMakeLists.txt CMakePresets.json
	cmake --preset default-config
	cmake --build --preset default-build

.PHONY: clean run
clean:
	rm -f bin/Linux/main

run: ./bin/Linux/main
	cd bin/Linux && ./main
