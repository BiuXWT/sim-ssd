#pragma once
#include "param.h"
#include "transaction.h"
#include "garbage_collection.h"

enum class BlockServiceStatus
{
    IDLE,
    GC,
    USER,
    GC_USER,
    GC_UWAIT,
    GC_USER_UWAIT
};

class Block{
public:
    uint32_t block_id;
    uint32_t current_write_page_index;
    BlockServiceStatus current_status;
    uint32_t invalid_page_count;
    static uint32_t page_bitmap_size;
    std::vector<uint64_t> invalid_page_bitmap;
    uint32_t erase_count;
    TransactionErasePtr ongoing_erase_tr;
    uint32_t stream_id;
    bool holds_mapping_data;
    bool hot_block;
    bool has_ongoing_gc;
    int ongoing_user_read_cnt;
    int ongoing_user_program_cnt;
    bool is_bad=false;
    void Erase();
};
using BlockPtr = std::shared_ptr<Block>;

class PlaneBookKeeping {
public:
    uint32_t total_pages_count;
    uint32_t free_pages_count;
    uint32_t valid_pages_count;
    uint32_t invalid_pages_count;

    std::vector<BlockPtr> data_open_blocks; // per stream_id
    std::vector<BlockPtr> gc_open_blocks; // per stream_id
    std::vector<BlockPtr> translation_open_blocks; // per stream_id

    std::queue<uint64_t> block_usage_history; //block 使用历史，存放block_id
    std::set<uint64_t> ongoing_erase_blocks; // 正在擦除的block_id

    BlockPtr GetOneFreeBlock(uint32_t stream_id,bool for_mapping_data);
    uint64_t GetFreeBlockCount() const { return free_block_pool.size(); }
    void CheckBookKeepingCorrectness(PhysicalPageAddress& plane_address);
    void AddToFreeBlockPool(BlockPtr block, bool consider_dynamic_wl);

    std::vector<BlockPtr> blocks;
    std::vector<BlockPtr> bad_blocks;
    std::multimap<uint64_t,BlockPtr> free_block_pool; // key: erase_count, value: block_ptr
};
using PlaneBookKeepingPtr = std::shared_ptr<PlaneBookKeeping>;

class BlockManager {
    BlockManager(GarbageCollectionPtr gc_ptr,uint32_t block_pe_cycle,uint32_t total_stream_count,
    uint32_t total_channel_count,uint32_t chips_per_channel,uint32_t dies_per_chip,
    uint32_t planes_per_die,uint32_t blocks_per_plane,uint32_t pages_per_block);
    ~BlockManager();
    void AllocateBlockAndPageInPlaneForUserWrite(const uint32_t stream_id, PhysicalPageAddress &page_address);
    void AllocateBlockAndPageInPlaneForGcWrite(const uint32_t stream_id, PhysicalPageAddress &page_address, bool for_mapping_data);
    void AllocateBlockAndPageInPlaneForTranslationGcWrite(const uint32_t stream_id, PhysicalPageAddress &page_address);
    void InvalidatePageInBlock(const uint32_t stream_id, const PhysicalPageAddress &page_address);
    void AddErasedBlockToPool(const PhysicalPageAddress &block_address);
    uint32_t GetFreeBlockPoolSize(const PhysicalPageAddress &plane_address);

    uint32_t GetColdestBlockId(const PhysicalPageAddress &plane_address);
    uint32_t GetMinMaxEraseDifference(const PhysicalPageAddress &plane_address);
    void SetGarbageCollectionUnit(GarbageCollectionPtr gc_ptr) { gc_unit = gc_ptr; }
    PlaneBookKeepingPtr GetPlaneBookKeepingEntry(const PhysicalPageAddress &plane_address);
    bool BlockHasOngoingGC(const PhysicalPageAddress &block_address);
    bool CanExecGC(const PhysicalPageAddress &block_address);
    void GcStartedOnBlock(const PhysicalPageAddress &block_address);
    void GcFinishedOnBlock(const PhysicalPageAddress &block_address);
    void ReadTransactionStartedOnBlock(const PhysicalPageAddress &block_address);
    void ReadTransactionFinishedOnBlock(const PhysicalPageAddress &block_address);
    void ProgramTransactionStartedOnBlock(const PhysicalPageAddress &block_address);
    void ProgramTransactionFinishedOnBlock(const PhysicalPageAddress &block_address);
    bool IsHavingOngoingProgramOnBlock(const PhysicalPageAddress &block_address) const;
    bool IsPageValid(const PhysicalPageAddress &page_address);

private:
    GarbageCollectionPtr gc_unit;
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

    void ProgramTransactionIssued(PhysicalPageAddress &page_address);
};
using BlockManagerPtr = std::shared_ptr<BlockManager>;