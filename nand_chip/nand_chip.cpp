#include "nand_chip.h"
#include <cstring>
#include <cstdio>

NandChip::NandChip(uint64_t channel_id, uint64_t chip_id, uint64_t dies_per_chip, uint64_t planes_per_die,
                   uint64_t blocks_per_plane, uint64_t pages_per_block, uint64_t page_size)
    : channel_id(channel_id), chip_id(chip_id) {
    dies.resize(dies_per_chip);
    for (uint64_t i = 0; i < dies_per_chip; ++i) {
        dies[i].plane_no = planes_per_die;
        dies[i].status = Die::DieStatus::IDLE;
        dies[i].planes.reserve(planes_per_die);
        for (uint64_t j = 0; j < planes_per_die; ++j) {
            dies[i].planes.emplace_back(blocks_per_plane, pages_per_block, page_size);
        }
    }
}

NandChip::~NandChip() {
}

std::vector<uint8_t> NandChip::GetMetaData(uint64_t die, uint64_t plane, uint64_t block, uint64_t page) {
    if (die >= dies.size() || plane >= dies[die].planes.size() || 
        block >= dies[die].planes[plane].blocks.size() || 
        page >= dies[die].planes[plane].pages_per_block) {
        return std::vector<uint8_t>(); // 返回空向量表示错误
    }
    
    // 返回元数据，这里可以根据需要自定义
    std::vector<uint8_t> metadata(16, 0x00); // 假设元数据长度为16字节
    return metadata;
}

int NandChip::erase_block(const Addr *addr) {
    if (!addr) return -1;
    if (addr->die >= dies.size() || addr->plane >= dies[addr->die].planes.size() || 
        addr->block >= dies[addr->die].planes[addr->plane].blocks.size()) {
        return -1;
    }
    
    // 擦除整个块，将所有页重置为0xFF
    Block& block = dies[addr->die].planes[addr->plane].blocks[addr->block];
    for (auto& page : block.pages) {
        std::fill(page.data.begin(), page.data.end(), 0xFF);
    }
    return 0;
}

int NandChip::write_page(const Addr *addr, const uint8_t *data) {
    if (!addr || !data) return -1;
    if (addr->die >= dies.size() || addr->plane >= dies[addr->die].planes.size() || 
        addr->block >= dies[addr->die].planes[addr->plane].blocks.size() ||
        addr->page >= dies[addr->die].planes[addr->plane].pages_per_block) {
        return -1;
    }
    
    Page& page = dies[addr->die].planes[addr->plane].blocks[addr->block].pages[addr->page];
    std::memcpy(page.data.data(), data, page.data.size());
    return 0;
}

int NandChip::read_page(const Addr *addr, uint8_t *data) {
    if (!addr || !data) return -1;
    if (addr->die >= dies.size() || addr->plane >= dies[addr->die].planes.size() || 
        addr->block >= dies[addr->die].planes[addr->plane].blocks.size() ||
        addr->page >= dies[addr->die].planes[addr->plane].pages_per_block) {
        return -1;
    }
    
    const Page& page = dies[addr->die].planes[addr->plane].blocks[addr->block].pages[addr->page];
    std::memcpy(data, page.data.data(), page.data.size());
    return 0;
}
