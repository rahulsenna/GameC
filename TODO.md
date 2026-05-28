


# Architecture

## Memory
1. **No-Copy GPU Buffers (Advanced):**
   If you want to send a massive amount of vertex or texture data to the GPU without copying it, Metal allows you to use `device->newBuffer(..., MTL::ResourceStorageModeShared)` or `newBufferWithBytesNoCopy(...)`. With the `NoCopy` variant, you can actually pass a pointer to your Arena's memory, and Metal will read directly from your Arena. (Note: The memory chunk usually has to be strictly page-aligned for this to work).










# References

1. [Metal-CPP](https://developer.apple.com/metal/cpp/)