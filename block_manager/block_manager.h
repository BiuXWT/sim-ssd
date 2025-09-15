#pragma once
#include "param.h"

enum class BlockServiceStatus
{
    IDLE,
    GC,
    USER,
    GC_USER,
    GC_UWAIT,
    GC_USER_UWAIT
};

class BlockSlot
{
public:
    uint64_t block_id;
    uint64_t current_write_page_index;
    BlockServiceStatus current_status;
    uint64_t invalid_page_count;
    static uint64_t page_bitmap_size;
    std::vector<uint64_t> invalid_page_bitmap;
    uint64_t erase_count;
    TransactionErasePtr ongoing_erase_tr;
    uint64_t stream_id;
    bool hot_block;
    bool has_ongoing_gc;
    int ongoing_user_read_cnt;
    int ongoing_user_program_cnt;
    bool is_bad = false;
    void Erase();
};

class PlaneBookKeeping
{
public:
    uint64_t total_pages_count;
    uint64_t free_pages_count;
    uint64_t valid_pages_count;
    uint64_t invalid_pages_count;

    std::vector<BlockPtr> data_open_blocks;        // per stream_id
    std::vector<BlockPtr> gc_open_blocks;          // per stream_id
    std::vector<BlockPtr> translation_open_blocks; // per stream_id

    std::queue<uint64_t> block_usage_history; // block 使用历史，存放block_id
    std::set<uint64_t> ongoing_erase_blocks;  // 正在擦除的block_id

    BlockPtr GetOneFreeBlock(uint64_t stream_id);
    uint64_t GetFreeBlockCount() const { return free_block_pool.size(); }
    void CheckBookKeepingCorrectness(const PhysicalPageAddressPtr plane_address);
    void AddToFreeBlockPool(BlockPtr block, bool consider_dynamic_wl);

    std::vector<BlockPtr> blocks;
    std::vector<BlockPtr> bad_blocks;
    std::multimap<uint64_t, BlockPtr> free_block_pool; // key: erase_count, value: block_ptr
};

class BlockManager
{
public:
    BlockManager(GcWlUnitPtr gc_ptr, uint64_t block_pe_cycle, uint64_t total_stream_count,
                 uint64_t total_channel_count, uint64_t chips_per_channel, uint64_t dies_per_chip,
                 uint64_t planes_per_die, uint64_t blocks_per_plane, uint64_t pages_per_block);
    ~BlockManager() = default;
    void AllocateBlockAndPageInPlaneForUserWrite(const uint64_t stream_id, PhysicalPageAddressPtr page_address);
    void AllocateBlockAndPageInPlaneForGcWrite(const uint64_t stream_id, PhysicalPageAddressPtr page_address);
    void AllocateBlockAndPageInPlaneForTranslationGcWrite(const uint64_t stream_id, PhysicalPageAddressPtr page_address);
    void InvalidatePageInBlock(const uint64_t stream_id, const PhysicalPageAddressPtr page_address);
    void AddErasedBlockToPool(const PhysicalPageAddressPtr block_address);
    uint64_t GetFreeBlockPoolSize(const PhysicalPageAddressPtr plane_address);

    uint64_t GetColdestBlockId(const PhysicalPageAddressPtr plane_address);
    uint64_t GetMinMaxEraseDifference(const PhysicalPageAddressPtr plane_address);
    uint64_t GetInputStreamCnt() const { return total_stream_count; }
    void SetGarbageCollectionUnit(GcWlUnitPtr gc_ptr) { gc_unit = gc_ptr; }
    PlaneBookKeepingPtr GetPlaneBookKeepingEntry(const PhysicalPageAddressPtr plane_address);
    bool BlockHasOngoingGC(const PhysicalPageAddressPtr block_address);
    bool CanExecGC(const PhysicalPageAddressPtr block_address);
    void GcStartedOnBlock(const PhysicalPageAddressPtr block_address);
    void GcFinishedOnBlock(const PhysicalPageAddressPtr block_address);
    void ReadTransactionStartedOnBlock(const PhysicalPageAddressPtr block_address);
    void ReadTransactionFinishedOnBlock(const PhysicalPageAddressPtr block_address);
    void ProgramTransactionStartedOnBlock(const PhysicalPageAddressPtr block_address);
    void ProgramTransactionFinishedOnBlock(const PhysicalPageAddressPtr block_address);
    bool IsHavingOngoingProgramOnBlock(const PhysicalPageAddressPtr block_address);
    bool IsPageValid(const PhysicalPageAddressPtr page_address);

private:
    GcWlUnitPtr gc_unit;
    // 定义一个[channel] [chip] [die] [plane]：4维数组
    std::vector<std::vector<std::vector<std::vector<PlaneBookKeepingPtr>>>> plane_manager;

    uint64_t block_pe_cycle; // 每个块的擦写寿命
    uint64_t total_stream_count;
    uint64_t total_channel_count;
    uint64_t chips_per_channel;
    uint64_t dies_per_chip;
    uint64_t planes_per_die;
    uint64_t blocks_per_plane;
    uint64_t pages_per_block;

    void ProgramTransactionIssued(PhysicalPageAddressPtr page_address);
};