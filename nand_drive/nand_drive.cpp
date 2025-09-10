#include "nand_drive.h"

NandDrive::NandDrive() {
    buffer = new uint8_t[TOTAL_BYTES];
    init();
}

NandDrive::~NandDrive() {
    delete[] buffer;
    buffer = nullptr;
}

void NandDrive::init() {
    if (!buffer) return; 
    std::memset(buffer, 0xFF, TOTAL_BYTES);
}

int NandDrive::erase_block(const Addr *addr) {
    if (!addr) return -1;
    if (addr->die >= DIE_CNT || addr->plane >= PLANE_CNT || addr->block >= BLOCK_CNT) return -1;
    for (uint16_t p = 0; p < PAGE_CNT; p++) {
        Addr tmp{addr->die, addr->plane, addr->block, p};
        std::memset(page_ptr(buffer, &tmp), 0xFF, PAGE_TOTAL_BYTES);
    }
    return 0;
}

int NandDrive::write_page(const Addr *addr, const uint8_t *main) {
    if (!addr) return -1;
    if (addr->die >= DIE_CNT || addr->plane >= PLANE_CNT || addr->block >= BLOCK_CNT || addr->page >= PAGE_CNT) return -1;
    if (!main) return -1;
    uint8_t* ptr = page_ptr(buffer, addr);
    std::memcpy(ptr, main, PG_SZ);
    return 0;
}

int NandDrive::read_page(const Addr *addr, uint8_t *main) {
    if (!addr) return -1;
    if (addr->die >= DIE_CNT || addr->plane >= PLANE_CNT || addr->block >= BLOCK_CNT || addr->page >= PAGE_CNT) return -1;
    if (!main) return -1;
    uint8_t* ptr = page_ptr(buffer, addr);
    std::memcpy(main, ptr, PG_SZ);
    return 0;
}

void NandDrive::print_page(const Addr *addr) {
    if (!addr) return;
    uint8_t* ptr = page_ptr(buffer, addr);
    std::printf("Die %u Plane %u Block %u Page %u Main: ", addr->die, addr->plane, addr->block, addr->page);
    for (int i = 0; i < 16 && i < PG_SZ; i++) std::printf("%02X ", ptr[i]);
    std::printf("\n");
}

int NandDrive::addr_valid(const Addr *addr) {
    if (!addr) return 0;
    if (addr->die >= DIE_CNT) return 0;
    if (addr->plane >= PLANE_CNT) return 0;
    if (addr->block >= BLOCK_CNT) return 0;
    if (addr->page >= PAGE_CNT) return 0;
    return 1;
}

int NandDrive::total_size_MB() const {
    return static_cast<int>(TOTAL_USER_BYTES / (1024 * 1024));
}
