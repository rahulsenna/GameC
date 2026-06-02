clang-format -i src/*.h src/*.cpp src/*.mm src/*.metal
./build.sh && lldb -b -o "run" -k "bt" ./build/engine