#pragma once
#include "param.h"
class NandDrive;
using NandDrivePtr = std::shared_ptr<NandDrive>;

class NandDrive {
public:
    static constexpr uint16_t DIE_CNT    = 1;
    static constexpr uint16_t PLANE_CNT  = 1;
    static constexpr uint16_t BLOCK_CNT  = 64;
    static constexpr uint16_t PAGE_CNT   = 8;
    static constexpr uint16_t PG_SZ      = 16384;
    static constexpr size_t   PAGE_TOTAL_BYTES = PG_SZ;
    static constexpr size_t   TOTAL_PAGES = static_cast<size_t>(DIE_CNT) * PLANE_CNT * BLOCK_CNT * PAGE_CNT;
    static constexpr size_t   TOTAL_BYTES = TOTAL_PAGES * PAGE_TOTAL_BYTES;
    static constexpr size_t   TOTAL_USER_BYTES = TOTAL_PAGES * PG_SZ;

    struct Addr {
        uint16_t die;
        uint16_t plane;
        uint16_t block;
        uint16_t page;
    };

    NandDrive();
    ~NandDrive();
    void init();
    int erase_block(const Addr *addr);
    int write_page(const Addr *addr, const uint8_t *data);
    int read_page(const Addr *addr, uint8_t *data);
    
    void print_page(const Addr *addr);
    static int addr_valid(const Addr *addr);
    int total_size_MB() const;

private:
    // 动态分配的一维缓冲：按 (die, plane, block, page) 线性展开
    uint8_t* buffer = nullptr;

    static size_t page_index(const Addr* addr) {
        return (((static_cast<size_t>(addr->die) * PLANE_CNT + addr->plane) * BLOCK_CNT + addr->block) * PAGE_CNT + addr->page);
    }
    static uint8_t* page_ptr(uint8_t* base, const Addr* addr) {
        return base + page_index(addr) * PAGE_TOTAL_BYTES;
    }
};
