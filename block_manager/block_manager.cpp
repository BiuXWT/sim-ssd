#include "block_manager.h"

uint32_t Block::page_bitmap_size = 0;

void Block::Erase()
{
    current_write_page_index = 0;
    invalid_page_count = 0;
    erase_count++;
    stream_id = 0xff;
    holds_mapping_data = false;
    ongoing_erase_tr = nullptr;
    for (size_t i = 0; i < page_bitmap_size; i++)
    {
        invalid_page_bitmap[i] = 0ULL;
    }
}

BlockPtr PlaneBookKeeping::GetOneFreeBlock(uint32_t stream_id, bool for_mapping_data)
{
    if (free_block_pool.empty())
    {
        PRINT_ERROR("Requesting a free block from an empty pool!")
    }
    auto block = free_block_pool.begin()->second;
    free_block_pool.erase(free_block_pool.begin());
    block->stream_id = stream_id;
    block->holds_mapping_data = for_mapping_data;
    block_usage_history.push(block->block_id);
    return block;
}

void PlaneBookKeeping::CheckBookKeepingCorrectness(PhysicalPageAddress &plane_address)
{
    int all_pages_cnt = free_pages_count + valid_pages_count + invalid_pages_count;
    if (all_pages_cnt != total_pages_count)
    {
        PRINT_ERROR("Inconsistent status in the plane bookkeeping record!")
    }
    if (free_pages_count == 0)
    {
        PRINT_ERROR("Plane " << "@" << plane_address.channel_id << "@" << plane_address.chip_id << "@" << plane_address.die_id << "@" << plane_address.plane_id << " pool size: " << GetFreeBlockCount() << " ran out of free pages! Bad resource management! It is not safe to continue simulation!");
    }
}

void PlaneBookKeeping::AddToFreeBlockPool(BlockPtr block, bool consider_dynamic_wl)
{
    if (consider_dynamic_wl)
    {
        free_block_pool.insert({block->erase_count, block});
    }
    else
    {
        free_block_pool.insert({0, block});
    }
}

BlockManager::BlockManager(GarbageCollectionPtr gc_ptr, uint32_t block_pe_cycle,
                           uint32_t total_stream_count, uint32_t total_channel_count, uint32_t chips_per_channel,
                           uint32_t dies_per_chip, uint32_t planes_per_die, uint32_t blocks_per_plane,
                           uint32_t pages_per_block)
    : gc_unit(gc_ptr), block_pe_cycle(block_pe_cycle), total_stream_count(total_stream_count),
      total_channel_count(total_channel_count), chips_per_channel(chips_per_channel),
      dies_per_chip(dies_per_chip), planes_per_die(planes_per_die),
      blocks_per_plane(blocks_per_plane), pages_per_block(pages_per_block)
{
    plane_manager.resize(total_channel_count);
    for (size_t channel_id = 0; channel_id < total_channel_count; channel_id++)
    {
        plane_manager[channel_id].resize(chips_per_channel);
        for (size_t chip_id = 0; chip_id < chips_per_channel; chip_id++)
        {
            plane_manager[channel_id][chip_id].resize(dies_per_chip);
            for (size_t die_id = 0; die_id < dies_per_chip; die_id++)
            {
                plane_manager[channel_id][chip_id][die_id].resize(planes_per_die);
                for (size_t plane_id = 0; plane_id < planes_per_die; plane_id++)
                {
                    plane_manager[channel_id][chip_id][die_id][plane_id] = std::make_shared<PlaneBookKeeping>();
                    auto plane = plane_manager[channel_id][chip_id][die_id][plane_id];
                    plane->total_pages_count = blocks_per_plane * pages_per_block;
                    plane->free_pages_count = plane->total_pages_count;
                    plane->valid_pages_count = 0;
                    plane->invalid_pages_count = 0;
                    plane->ongoing_erase_blocks.clear();
                    plane->blocks.resize(blocks_per_plane);

                    for(size_t block_id = 0; block_id < blocks_per_plane; block_id++)
                    {
                        plane->blocks[block_id] = std::make_shared<Block>();
                        auto block = plane->blocks[block_id];
                        block->block_id = block_id;
                        block->current_write_page_index = 0;
                        block->current_status = BlockServiceStatus::IDLE;
                        block->invalid_page_count = 0;
                        block->erase_count = 0;
                        block->holds_mapping_data = false;
                        block->has_ongoing_gc = false;
                        block->ongoing_erase_tr = nullptr;
                        block->ongoing_user_program_cnt = 0;
                        block->ongoing_user_read_cnt = 0;
                        block->stream_id = 0xff; // 初始时不属于任何流
                        block->hot_block = false;
                        // 计算每个Block的页位图数组大小
                        // page_bitmap_size 表示需要多少个uint64_t元素来存储所有页的有效性信息
                        // 1. 一个uint64_t有64位，可以表示64个页的状态
                        // 2. pages_per_block/(8*sizeof(uint64_t)) 计算能被完整uint64_t覆盖的页组数
                        // 3. pages_per_block%(8*sizeof(uint64_t))?1:0 判断是否有剩余页，若有则再多分配一个uint64_t
                        Block::page_bitmap_size = pages_per_block/(8*sizeof(uint64_t))+(pages_per_block%(8*sizeof(uint64_t))?1:0);
                        block->invalid_page_bitmap.resize(Block::page_bitmap_size);
                        for (size_t i = 0; i < Block::page_bitmap_size; i++)
                        {
                            block->invalid_page_bitmap[i] = 0ULL;
                        }
                        plane->AddToFreeBlockPool(block, true); // 初始时将所有块加入空闲块池，考虑动态磨损均衡
                    }
                    plane->data_open_blocks.resize(total_stream_count);
                    plane->gc_open_blocks.resize(total_stream_count);
                    plane->translation_open_blocks.resize(total_stream_count);
                    for (size_t stream_id = 0; stream_id < total_stream_count; stream_id++)
                    {
                        plane->data_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, false);
                        plane->gc_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, false);
                        plane->translation_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, true);
                    }
                }
            }
        }
    }
}

BlockManager::~BlockManager()
{
}

void BlockManager::AllocateBlockAndPageInPlaneForUserWrite(const uint32_t stream_id, PhysicalPageAddress &page_address)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    plane->valid_pages_count++;
    plane->free_pages_count--;
    page_address.block_id = plane->data_open_blocks[stream_id]->block_id;
    page_address.page_id = plane->data_open_blocks[stream_id]->current_write_page_index++;
    ProgramTransactionIssued(page_address);

    if(plane->data_open_blocks[stream_id]->current_write_page_index == pages_per_block)
    {
        // 当前块写满，分配新块
        plane->data_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, false);
        gc_unit->CheckGcRequired(plane->GetFreeBlockCount(), page_address);
    }
    plane->CheckBookKeepingCorrectness(page_address);
}

void BlockManager::AllocateBlockAndPageInPlaneForGcWrite(const uint32_t stream_id, PhysicalPageAddress &page_address, bool for_mapping_data)
{
}

void BlockManager::AllocateBlockAndPageInPlaneForTranslationGcWrite(const uint32_t stream_id, PhysicalPageAddress &page_address)
{
}

void BlockManager::InvalidatePageInBlock(const uint32_t stream_id, const PhysicalPageAddress &page_address)
{
}

void BlockManager::AddErasedBlockToPool(const PhysicalPageAddress &block_address)
{
}

uint32_t BlockManager::GetFreeBlockPoolSize(const PhysicalPageAddress &plane_address)
{
    auto plane = GetPlaneBookKeepingEntry(plane_address);
    return plane->free_block_pool.size();
}

uint32_t BlockManager::GetColdestBlockId(const PhysicalPageAddress &plane_address)
{
    return 0;
}

uint32_t BlockManager::GetMinMaxEraseDifference(const PhysicalPageAddress &plane_address)
{
    return 0;
}

PlaneBookKeepingPtr BlockManager::GetPlaneBookKeepingEntry(const PhysicalPageAddress &plane_address)
{
    return PlaneBookKeepingPtr();
}

bool BlockManager::BlockHasOngoingGC(const PhysicalPageAddress &block_address)
{
    return false;
}

bool BlockManager::CanExecGC(const PhysicalPageAddress &block_address)
{
    return false;
}

void BlockManager::GcStartedOnBlock(const PhysicalPageAddress &block_address)
{
}

void BlockManager::GcFinishedOnBlock(const PhysicalPageAddress &block_address)
{
}

void BlockManager::ReadTransactionStartedOnBlock(const PhysicalPageAddress &block_address)
{
}

void BlockManager::ReadTransactionFinishedOnBlock(const PhysicalPageAddress &block_address)
{
}

void BlockManager::ProgramTransactionStartedOnBlock(const PhysicalPageAddress &block_address)
{
}

void BlockManager::ProgramTransactionFinishedOnBlock(const PhysicalPageAddress &block_address)
{
}

bool BlockManager::IsHavingOngoingProgramOnBlock(const PhysicalPageAddress &block_address) const
{
    return false;
}

bool BlockManager::IsPageValid(const PhysicalPageAddress &page_address)
{
    return false;
}

void BlockManager::ProgramTransactionIssued(PhysicalPageAddress &page_address)
{
}
