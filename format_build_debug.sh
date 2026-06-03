set -e
clang-format -i src/*.h src/*.cpp src/*.mm src/*.metal

./build.sh

/usr/bin/log stream \
    --level debug \
    --predicate 'process == "engine" AND senderImagePath contains "Metal" AND NOT(eventMessage CONTAINS "DescriptorHeapNewHandle") AND NOT(eventMessage CONTAINS "MemoryUsed")' \
    | tee build/metal.log &
LOG_PID=$!

echo "$LOG_PID"

cleanup() {
    kill -- -"$LOG_PID" 2>/dev/null || true
}

trap cleanup EXIT INT TERM


# man MetalValidation

# MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1 \
MTL_DEBUG_LAYER=1 \
MTL_SHADER_VALIDATION=1 \
METAL_DEVICE_WRAPPER_TYPE=1 \
lldb -b -o "run" -k "bt" ./build/engine

pgrep -x log | xargs kill