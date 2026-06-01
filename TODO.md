


# Architecture

## Animation

- [ ] high level animation blend tree.

---

## Memory
- [ ] **No-Copy GPU Buffers (Advanced):**
   If you want to send a massive amount of vertex or texture data to the GPU without copying it, Metal allows you to use `device->newBuffer(..., MTL::ResourceStorageModeShared)` or `newBufferWithBytesNoCopy(...)`. With the `NoCopy` variant, you can actually pass a pointer to your Arena's memory, and Metal will read directly from your Arena. (Note: The memory chunk usually has to be strictly page-aligned for this to work).


---

## Asset Pipeline

A checklist for building the first iteration of an offline asset pipeline, moving away from slow runtime parsing of raw formats.

- [ ] **1. Establish Directory Structure**
  - Create an `assets_src/` directory for raw DCC files (e.g., `.fbx`, `.png`, `.psd`).
  - Create an `assets_cooked/` directory for the engine-ready binary files that the game will actually load at runtime.

- [ ] **2. Create the `AssetBuilder` Tool**
  - Write a standalone command-line application (C++ or Python) that runs before the game launches.
  - Implement basic file iteration to scan the `assets_src/` directory for changes.

- [ ] **3. Implement Mesh Cooking (Offline Conversion)**
  - Integrate a library like `ufbx` into the `AssetBuilder`.
  - Parse raw `.fbx` files and compute tangent spaces, bounding boxes, and optimized vertex caches.
  - Write out a custom `.mesh` binary file that the engine can load directly into memory without any parsing logic.

- [ ] **4. Implement Texture Processing & Channel Packing**
  - Use a library like `stb_image` inside the `AssetBuilder` to read raw `.png` or `.tga` files.
  - Implement an **ORM Channel Packer**: Take separate Ambient Occlusion (R), Roughness (G), and Metallic (B) grayscale maps and automatically combine them into a single `_ORM.png` (or binary texture format).
  - (Optional for now) Add support for compressing textures into hardware-friendly formats (like ASTC or BC7) using Metal's texture tools or `ispc_texcomp`.

- [ ] **5. Engine Runtime Updates**
  - Strip the `ufbx` and `stb_image` (if used directly for rendering) out of the main game engine.
  - Write a fast binary reader that exclusively loads the cooked `.mesh` and processed texture files from the `assets_cooked/` folder directly into `gpuMalloc` memory.


---

## Modern GPU Engine Architecture

A checklist for transitioning the engine to a modern, pointer-based, bindless architecture.

- [ ] **1. Implement `gpuMalloc` Architecture**
  - Create a unified memory allocator that returns CPU-mapped GPU pointers (using Unified Memory / ReBAR).
  - Eliminate staging buffers and complex mapping APIs for dynamic data.
  - **Implementation Example (Metal C++):**
    ```cpp
    struct GpuPtr {
        void* cpu;       // Use this to write data in C++
        uint64_t gpu;    // Pass this to the shader
    };

    class GpuAllocator {
    private:
        id<MTLDevice> device;
        std::unordered_map<void*, id<MTLBuffer>> activeAllocations; 

    public:
        GpuPtr gpuMalloc(size_t size) {
            id<MTLBuffer> buffer = [device newBufferWithLength:size 
                                                       options:MTLResourceStorageModeShared];
            void* cpuPtr = [buffer contents];
            uint64_t gpuPtr = [buffer gpuAddress];
            activeAllocations[cpuPtr] = buffer; // Keep alive
            return { cpuPtr, gpuPtr };
        }

        void gpuFree(void* cpuPtr) {
            activeAllocations.erase(cpuPtr); // ARC will destroy the buffer
        }
    };
    ```

- [ ] **2. Root Pointer Data Model**
  - Replace individual Constant Buffers with a single 64-bit root struct pointer passed to shaders.
  - Pack all uniforms and pointers to data arrays inside this single root struct.

- [ ] **3. Bindless Texture Heap (Argument Buffers)**
  - Implement a global texture descriptor heap (using Metal Tier 2 Argument Buffers).
  - Stop binding textures individually. Pass 32-bit indices (or 64-bit pointers) in the root struct to index into the global heap.

- [ ] **4. Migrate to Raw Memory Loads**
  - Remove Texel Buffers (`Buffer<float4>`).
  - Use raw GPU pointers for vertex data, animation matrices, and material structs.
  - Implement "Wide Loads" (e.g., load 128-bits at once and unpack in the shader to reduce register pressure).

- [ ] **5. Offline Asset Pipeline**
  - Ensure textures and meshes are cooked offline into engine-ready binary formats.
  - Pack textures (ORM channel packing) and optimize meshes to remove runtime loading overhead.


---
# References

1. [Metal-CPP](https://developer.apple.com/metal/cpp/)
