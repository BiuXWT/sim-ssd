#include "block_manager.h"
#include "transaction.h"
#include "gc_wl.h"

uint64_t BlockSlot::page_bitmap_size = 0;

void BlockSlot::Erase()
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

BlockPtr PlaneBookKeeping::GetOneFreeBlock(uint64_t stream_id, bool for_mapping_data)
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

void PlaneBookKeeping::CheckBookKeepingCorrectness(const PhysicalPageAddress &plane_address)
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

BlockManager::BlockManager(GcWlUnitPtr gc_ptr, uint64_t block_pe_cycle,
                           uint64_t total_stream_count, uint64_t total_channel_count, uint64_t chips_per_channel,
                           uint64_t dies_per_chip, uint64_t planes_per_die, uint64_t blocks_per_plane,
                           uint64_t pages_per_block)
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
                        plane->blocks[block_id] = std::make_shared<BlockSlot>();
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
                        BlockSlot::page_bitmap_size = pages_per_block/(8*sizeof(uint64_t))+(pages_per_block%(8*sizeof(uint64_t))?1:0);
                        block->invalid_page_bitmap.resize(BlockSlot::page_bitmap_size);
                        for (size_t i = 0; i < BlockSlot::page_bitmap_size; i++)
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

void BlockManager::AllocateBlockAndPageInPlaneForUserWrite(const uint64_t stream_id, PhysicalPageAddress &page_address)
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

void BlockManager::AllocateBlockAndPageInPlaneForGcWrite(const uint64_t stream_id, PhysicalPageAddress &page_address, bool for_mapping_data)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    plane->valid_pages_count++;
    plane->free_pages_count--;
    page_address.block_id = plane->gc_open_blocks[stream_id]->block_id;
    page_address.page_id = plane->gc_open_blocks[stream_id]->current_write_page_index++;
    if (plane->gc_open_blocks[stream_id]->current_write_page_index == pages_per_block)
    {
        // 当前块写满，分配新块
        plane->gc_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, for_mapping_data);
        gc_unit->CheckGcRequired(plane->GetFreeBlockCount(), page_address);
    }
    plane->CheckBookKeepingCorrectness(page_address);
}

void BlockManager::AllocateBlockAndPageInPlaneForTranslationGcWrite(const uint64_t stream_id, PhysicalPageAddress &page_address)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    plane->valid_pages_count++;
    plane->free_pages_count--;
    page_address.block_id = plane->translation_open_blocks[stream_id]->block_id;
    page_address.page_id = plane->translation_open_blocks[stream_id]->current_write_page_index++;
    if (plane->translation_open_blocks[stream_id]->current_write_page_index == pages_per_block)
    {
        // 当前块写满，分配新块
        plane->translation_open_blocks[stream_id] = plane->GetOneFreeBlock(stream_id, true);
        gc_unit->CheckGcRequired(plane->GetFreeBlockCount(), page_address);
    }
    plane->CheckBookKeepingCorrectness(page_address);
}

void BlockManager::InvalidatePageInBlock(const uint64_t stream_id, const PhysicalPageAddress &page_address)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    plane->valid_pages_count--;
    plane->invalid_pages_count++;
    if(plane->blocks[page_address.block_id]->stream_id != stream_id)
    {
        PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
    }
    plane->blocks[page_address.block_id]->invalid_page_count++;
    plane->blocks[page_address.block_id]->invalid_page_bitmap[page_address.page_id / 64] |= (1ULL << (page_address.page_id % 64));
}

void BlockManager::AddErasedBlockToPool(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    auto block = plane->blocks[block_address.block_id];
    plane->free_pages_count += (pages_per_block - block->invalid_page_count);
    plane->invalid_pages_count -= block->invalid_page_count;

    block->Erase();
    plane->AddToFreeBlockPool(block, gc_unit->UseDynamicWearLeveling());
    plane->CheckBookKeepingCorrectness(block_address);
}

uint64_t BlockManager::GetFreeBlockPoolSize(const PhysicalPageAddress &plane_address)
{
    auto plane = GetPlaneBookKeepingEntry(plane_address);
    return plane->free_block_pool.size();
}

uint64_t BlockManager::GetColdestBlockId(const PhysicalPageAddress &plane_address)
{
    uint64_t coldest_block_id = 0;
    auto plane = GetPlaneBookKeepingEntry(plane_address);
    uint64_t min_erase_count = std::numeric_limits<uint64_t>::max();
    for (const auto& block : plane->blocks)
    {
        if (block->erase_count < min_erase_count)
        {
            min_erase_count = block->erase_count;
            coldest_block_id = block->block_id;
        }
    }
    return coldest_block_id;
}

uint64_t BlockManager::GetMinMaxEraseDifference(const PhysicalPageAddress &plane_address)
{
    auto plane = GetPlaneBookKeepingEntry(plane_address);
    uint64_t min_erase_count = std::numeric_limits<uint64_t>::max();
    uint64_t max_erase_count = 0;
    for (const auto& block : plane->blocks)
    {
        if (block->erase_count < min_erase_count)
        {
            min_erase_count = block->erase_count;
        }
        if (block->erase_count > max_erase_count)
        {
            max_erase_count = block->erase_count;
        }
    }
    return max_erase_count - min_erase_count;
}

PlaneBookKeepingPtr BlockManager::GetPlaneBookKeepingEntry(const PhysicalPageAddress &plane_address)
{
    return plane_manager[plane_address.channel_id][plane_address.chip_id][plane_address.die_id][plane_address.plane_id];
}

bool BlockManager::BlockHasOngoingGC(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    auto block = plane->blocks[block_address.block_id];
    return block->has_ongoing_gc;
}

bool BlockManager::CanExecGC(const PhysicalPageAddress &block_address)
{
    auto plane_record = GetPlaneBookKeepingEntry(block_address);
    auto block = plane_record->blocks[block_address.block_id];
    return (block->ongoing_user_program_cnt + block->ongoing_user_read_cnt == 0);
}

void BlockManager::GcStartedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->has_ongoing_gc = true;
}

void BlockManager::GcFinishedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->has_ongoing_gc = false;
}

void BlockManager::ReadTransactionStartedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->ongoing_user_read_cnt++;
}

void BlockManager::ReadTransactionFinishedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->ongoing_user_read_cnt--;
}

void BlockManager::ProgramTransactionStartedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->ongoing_user_program_cnt++;
}

void BlockManager::ProgramTransactionFinishedOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    plane->blocks[block_address.block_id]->ongoing_user_program_cnt--;
}

bool BlockManager::IsHavingOngoingProgramOnBlock(const PhysicalPageAddress &block_address)
{
    auto plane = GetPlaneBookKeepingEntry(block_address);
    return plane->blocks[block_address.block_id]->ongoing_user_program_cnt > 0;
}

bool BlockManager::IsPageValid(const PhysicalPageAddress &page_address)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    auto block = plane->blocks[page_address.block_id];
    return (block->invalid_page_bitmap[page_address.page_id / 64] & (1ULL << (page_address.page_id % 64))) == 0;
}

void BlockManager::ProgramTransactionIssued(PhysicalPageAddress &page_address)
{
    auto plane = GetPlaneBookKeepingEntry(page_address);
    plane->blocks[page_address.block_id]->ongoing_user_program_cnt++;
}
