#pragma once
#include "param.h"
class NandChip;
using NandChipPtr = std::shared_ptr<NandChip>;

class Page{
public:
    Page(int page_size):data(page_size,0xFF){}
    std::vector<uint8_t> data; // 主数据区
};

class Block{
public:
    Block(int block_id, int pages_per_block, int page_size):block_id(block_id), pages(pages_per_block, Page(page_size)){}
    uint32_t block_id;
    std::vector<Page> pages;
};

class Plane{
    private:
    double bad_block_ratio = 0.02; // 坏块比例
    std::vector<uint32_t> bad_block_ids;
    void set_random_bad_blocks(){
        uint32_t bad_block_count = static_cast<uint32_t>(blocks_no * bad_block_ratio);
        if(bad_block_count == 0 && bad_block_ratio > 0) bad_block_count = 2; // 至少2个坏块
        std::set<uint32_t> bad_blocks_set;
        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<uint32_t> dist(0, blocks_no - 1);
        while (bad_blocks_set.size() < bad_block_count) {
            bad_blocks_set.insert(dist(rng));
        }
        bad_block_ids.assign(bad_blocks_set.begin(), bad_blocks_set.end());
        bad_block_no = bad_block_ids.size();
        healthy_block_no = blocks_no - bad_block_no;
    }
public:
    Plane(int blocks_per_plane, int pages_per_block, int page_size)
        : blocks_no(blocks_per_plane), pages_per_block(pages_per_block) {
        blocks.reserve(blocks_no);
        for (uint32_t i = 0; i < blocks_no; ++i) {
            blocks.emplace_back(i, page_size);
        }
        set_random_bad_blocks();
    }
    uint32_t blocks_no;
    uint32_t bad_block_no;
    uint32_t healthy_block_no;
    uint32_t pages_per_block;
    std::vector<uint32_t> bad_block_ids;
    std::vector<Block> blocks;
};


class Die{
    enum class DieStatus { BUSY, IDLE };
public:
    uint32_t plane_no;
    DieStatus status;
    std::vector<Plane> planes;
};

class NandChip {
    enum class InternalState {
        IDLE,
        BUSY
    };
        struct Addr {
        uint32_t die;
        uint32_t plane;
        uint32_t block;
        uint32_t page;
    };

public:
    NandChip(uint32_t channel_id, uint32_t chip_id, uint32_t dies_per_chip, uint32_t planes_per_die,
             uint32_t blocks_per_plane, uint32_t pages_per_block);
    ~NandChip();
    uint32_t channel_id;
    uint32_t chip_id;
    std::vector<uint8_t> GetMetaData(uint32_t die, uint32_t plane, uint32_t block, uint32_t page);
    int erase_block(const Addr *addr);
    int write_page(const Addr *addr, const uint8_t *data);
    int read_page(const Addr *addr, uint8_t *data);

private:

    std::vector<Die> dies;

};
