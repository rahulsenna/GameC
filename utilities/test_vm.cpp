#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

void sigsegv_handler(int sig) {
    std::cout << "\n[CRASH CAUGHT] The OS threw a segmentation fault! It successfully prevented us from writing to decommitted memory.\n";
    exit(0); // Exit gracefully to show the test passed
}

int main() {
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t reserve_size = 1024 * 1024 * 1024; // 1 GB
    
    std::cout << "1. Reserving 1 GB of virtual memory (PROT_NONE)...\n";
    void* base_ptr = mmap(NULL, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base_ptr == MAP_FAILED) {
        std::cerr << "Failed to reserve memory.\n";
        return 1;
    }
    std::cout << "   -> Reserved at address: " << base_ptr << "\n\n";

    std::cout << "2. Committing the first page (" << page_size << " bytes)...\n";
    if (mprotect(base_ptr, page_size, PROT_READ | PROT_WRITE) != 0) {
        std::cerr << "Failed to commit memory.\n";
        return 1;
    }
    std::cout << "   -> Success!\n\n";

    std::cout << "3. Writing data to the committed page...\n";
    int* data = static_cast<int*>(base_ptr);
    data[0] = 42;
    std::cout << "   -> Success! Wrote value: " << data[0] << "\n\n";

    std::cout << "4. Decommitting the memory...\n";
    // Tell OS to reclaim physical RAM (MADV_DONTNEED / MADV_FREE)
    madvise(base_ptr, page_size, MADV_DONTNEED);
    // Remove read/write access so the virtual addresses crash on access
    if (mprotect(base_ptr, page_size, PROT_NONE) != 0) {
        std::cerr << "Failed to decommit memory.\n";
        return 1;
    }
    std::cout << "   -> Success!\n\n";

    std::cout << "5. Setting up signal handler to catch the incoming crash...\n";
    // When we write to PROT_NONE, it usually throws SIGSEGV or SIGBUS
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGBUS, sigsegv_handler);

    std::cout << "6. Attempting to write to the decommitted memory... (This should crash!)\n";
    data[0] = 99; // THIS WILL CRASH
    
    std::cout << "\n[FAILED] We somehow wrote to the memory! This shouldn't happen.\n";
    return 1;
}
