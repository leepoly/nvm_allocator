//
// Created by 王柯 on 2021-03-24.
// Modified by Yiwei Li on 2023-04-06.
//

#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <atomic>
#include <vector>
#include <tuple>
#include <sys/mman.h>
#include <cstdlib>

#ifndef NVMKV_NVMAllocator_H
#define NVMKV_NVMAllocator_H

#define ALLOC_SIZE ((size_t)4<<30) // 4GB
#define CACHELINESIZE (64)

#define MAP_SYNC 0x080000
#define MAP_SHARED_VALIDATE 0x03

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#define cas(_p, _u, _v)  (__atomic_compare_exchange_n (_p, _u, _v, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))

inline void mfence(void) {
    asm volatile("mfence":: :"memory");
}

inline void clflush(char *data, size_t len) {
    volatile char *ptr = (char *) ((unsigned long) data & (~(CACHELINESIZE - 1)));
    mfence();
    for (; ptr < data + len; ptr += CACHELINESIZE) {
        asm volatile("clflush %0" : "+m" (*(volatile char *) ptr));
    }
    mfence();
}

using namespace std;

class NVMAllocator {
public:
    bool is_used = false;
    char *dram[100];
    char *dram_curr = NULL;
    uint64_t dram_left = 0;
    uint64_t dram_cnt = 0;

    char *nvm[100];
    char *nvm_curr = NULL;
    uint64_t nvm_left = 0;
    uint64_t nvm_cnt = 0;

    uint64_t size_used = 0;
    std::vector<std::tuple<char*, uint64_t, uint64_t>> addr_map;

    NVMAllocator() {};

    virtual void init() {
        dram[dram_cnt] = new char[ALLOC_SIZE];
        dram_curr = dram[dram_cnt];
        dram_left = ALLOC_SIZE;
        dram_cnt++;

#ifdef __linux__
        string nvm_filename = "/mnt/aep1/test";
        nvm_filename = nvm_filename + to_string(nvm_cnt);
        int nvm_fd = open(nvm_filename.c_str(), O_CREAT | O_RDWR, 0644);
        if (posix_fallocate(nvm_fd, 0, ALLOC_SIZE) < 0)
            puts("fallocate fail\n");
        nvm[nvm_cnt] = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, nvm_fd, 0);
#else
        nvm[nvm_cnt] = new char[ALLOC_SIZE];
#endif
        nvm_curr = nvm[nvm_cnt];
        nvm_left = ALLOC_SIZE;
        nvm_cnt++;
    }

    virtual void *alloc(uint64_t size, bool _on_nvm = true) {
        size = size / 64 * 64 + ((size % 64) != 0) * 64;
        size_used += size;

        if (_on_nvm) {
            if (unlikely(size > nvm_left)) {
#ifdef __linux__
                string nvm_filename = "/mnt/aep1/test";
                nvm_filename = nvm_filename + to_string(nvm_cnt);
                int nvm_fd = open(nvm_filename.c_str(), O_CREAT | O_RDWR, 0644);
                if (posix_fallocate(nvm_fd, 0, ALLOC_SIZE) < 0)
                    puts("fallocate fail\n");
                nvm[nvm_cnt] = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, nvm_fd, 0);
#else
                nvm[nvm_cnt] = new char[ALLOC_SIZE];
#endif
                nvm_curr = nvm[nvm_cnt];
                nvm_left = ALLOC_SIZE;
                nvm_cnt++;
                nvm_left -= size;
                void *tmp = nvm_curr;
                nvm_curr = nvm_curr + size;
                return tmp;
            } else {
                auto new_map = make_tuple(nvm_curr - size, size_used - size, size);
                addr_map.push_back(new_map);

                nvm_left -= size;
                void *tmp = nvm_curr;
                nvm_curr = nvm_curr + size;
                return tmp;
            }
        } else {
            if (unlikely(size > dram_left)) {
                dram[dram_cnt] = new char[ALLOC_SIZE];
                dram_curr = dram[dram_cnt];
                dram_left = ALLOC_SIZE;
                dram_cnt++;
                dram_left -= size;
                void *tmp = dram_curr;
                dram_curr = dram_curr + size;
                return tmp;
            } else {
                dram_left -= size;
                void *tmp = dram_curr;
                dram_curr = dram_curr + size;
                return tmp;
            }
        }
    }

    virtual void free() {
        if (dram != NULL) {
            dram_left = 0;
            for (int i = 0; i < dram_cnt; ++i) {
                delete[]dram[i];
            }
            dram_curr = NULL;
        }
    }

    uint64_t map_address(uint64_t ptr) {
        for (auto item : addr_map) {
            uint64_t offset = (uint64_t)(std::get<0>(item));
            uint64_t map_addr = std::get<1>(item);
            uint64_t size = std::get<2>(item);
            if (ptr >= offset && (ptr - offset < size)) {
                return map_addr + (ptr - offset);
            }
        }
        return 0;
    }

    uint64_t profile(bool _on_nvm = true) {
        if (_on_nvm)
            return nvm_cnt * ALLOC_SIZE - nvm_left;
        else
            return dram_cnt * ALLOC_SIZE - dram_left;
    }
};

#endif // NVMKV_NVMAllocator_H
