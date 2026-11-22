.PHONY: sokoban sokoban_bench bench clean

sokoban:
	cmake --build build --target sokoban -j

sokoban_bench bench:
	cmake --build build --target sokoban_bench -j

clean:
	cmake --build build --target clean
