//
// Created by 王柯 on 2021-03-24.
// Modified by Yiwei Li on 2023-04-06.
//

#include <cassert>
#include <cstdint>
#include "nvm_allocator.h"

int main() {
    NVMAllocator* myallocator = new NVMAllocator;
    myallocator->init();

    const int arr_size = 100;

    bool on_nvm = true;
    uint64_t* arr = (uint64_t*) myallocator->alloc(sizeof(uint64_t) * arr_size, on_nvm);
    assert(arr);
    printf("allocated %d bytes on_nvm: %x\n", sizeof(uint64_t) * arr_size, on_nvm);
    arr[1] = arr[0] = 1;
    for (int i = 2; i < arr_size; i++) {
        arr[i] = arr[i - 1] + arr[i - 2];
    }
    int id = 99;
    printf("result: %d %ld\n", id, arr[id]);

    myallocator->free();
    delete myallocator;
#ifdef __linux__
    system("rm /mnt/aep1/test*");
#endif
    return 0;
}