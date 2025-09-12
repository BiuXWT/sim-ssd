#include "gc_wl.h"
#include "transaction.h"
#include "block_manager.h"
#include "nand_chip.h"

int GcWlUnit::GetRandomBlockId()
{
    return dist(rng);
}

bool GcWlUnit::GcIsUrgentMode(NandChipPtr nand_chip)
{
    if (!preemptible_gc_enabled)
        return true;

    PhysicalPageAddress addr;
    addr.channel_id = nand_chip->channel_id;
    for (uint32_t die_id = 0; die_id < die_per_chip; die_id++)
    {
        for (uint32_t plane_id = 0; plane_id < plane_per_die; plane_id++)
        {
            addr.die_id = die_id;
            addr.plane_id = plane_id;
            if (block_manager->GetFreeBlockPoolSize(addr) < block_pool_gc_hard_threshold)
            {
                return true;
            }
        }
    }

    return false;
}

void GcWlUnit::CheckGcRequired(const uint32_t free_block_pool_size, const PhysicalPageAddress &plane_address)
{
    if (free_block_pool_size < block_pool_gc_threshold)
    {
        uint32_t gc_candidate_block_id = UINT32_MAX;
        PlaneBookKeepingPtr plane = block_manager->GetPlaneBookKeepingEntry(plane_address);
        if (plane->ongoing_erase_blocks.size() >= max_ongoing_gc_reqs_per_plane)
        {
            return;
        }
        switch (gc_policy)
        {
        case GC_POLICY::GREEDY:
        { // 直接选择最冷块
            gc_candidate_block_id = 0;
            if (plane->ongoing_erase_blocks.find(0) != plane->ongoing_erase_blocks.end())
                gc_candidate_block_id++;
            for (size_t block_id = 1; block_id < block_per_plane; ++block_id)
            {
                if (plane->blocks[block_id]->invalid_page_count > plane->blocks[gc_candidate_block_id]->invalid_page_count 
                    && plane->blocks[block_id]->current_write_page_index == page_per_block // 已满
                    && IsSafeGcCandidate(plane, block_id))
                {
                    gc_candidate_block_id = block_id;
                }
                break;
            }
        case GC_POLICY::RGA:
        { // 从随机选择的几个块中选择最冷块
            std::set<uint32_t> candidate_set;
            while (candidate_set.size() < rga_set_size)
            {
                uint32_t id = GetRandomBlockId();
                if (plane->ongoing_erase_blocks.find(id) == plane->ongoing_erase_blocks.end() && IsSafeGcCandidate(plane, id))
                    candidate_set.insert(id);
            }
            gc_candidate_block_id = *candidate_set.begin();
            for (auto &id : candidate_set)
            {
                if (plane->blocks[id]->invalid_page_count > plane->blocks[gc_candidate_block_id]->invalid_page_count 
                    && plane->blocks[id]->current_write_page_index == page_per_block) // 已满
                {
                    gc_candidate_block_id = id;
                }
            }
            break;
        }
        case GC_POLICY::RANDOM:
        { // 随机选择一个块
            gc_candidate_block_id = GetRandomBlockId();
            uint32_t repeat = 0;
            while(!IsSafeGcCandidate(plane, gc_candidate_block_id)&& repeat++<block_per_plane){
                gc_candidate_block_id = GetRandomBlockId();
            }
            break;
        }
        case GC_POLICY::RANDOM_P:
        { // 随机选择一个块，要求有效页数小于某个阈值
            gc_candidate_block_id = GetRandomBlockId();
            uint32_t repeat = 0;
            // 如果该块未写满，或（该块不安全且尝试次数未超限），则继续随机选块
            while(plane->blocks[gc_candidate_block_id]->current_write_page_index < page_per_block
                  || (!IsSafeGcCandidate(plane, gc_candidate_block_id) && repeat++<block_per_plane)){
                gc_candidate_block_id = GetRandomBlockId();
            }
            break;
        }
        case GC_POLICY::RANDOM_PP:
        { // 随机选择一个块，要求有效页数小于某个阈值，否则选择最冷块
            gc_candidate_block_id = GetRandomBlockId();
            uint32_t repeat = 0;

            while(plane->blocks[gc_candidate_block_id]->current_write_page_index < page_per_block
                || plane->blocks[gc_candidate_block_id]->invalid_page_count < random_pp_threshold  
                || !IsSafeGcCandidate(plane, gc_candidate_block_id)
                && repeat++<block_per_plane){
                gc_candidate_block_id = GetRandomBlockId();
            }
            break;
        }
        case GC_POLICY::FIFO:
        { // 选择最早使用的块
            gc_candidate_block_id = plane->block_usage_history.front();
            plane->block_usage_history.pop();
            break;
        }
        default:
            PRINT_ERROR("Unsupported GC policy!")
            break;
        }
        }
    }
}

bool GcWlUnit::IsSafeGcCandidate(PlaneBookKeepingPtr plane, uint32_t gc_candidate_block_id)
{
    for(uint32_t stream_id = 0; stream_id < block_manager->GetInputStreamCnt(); ++stream_id){
        if(plane->data_open_blocks[stream_id]->block_id == gc_candidate_block_id
           || plane->gc_open_blocks[stream_id]->block_id == gc_candidate_block_id
           || plane->translation_open_blocks[stream_id]->block_id == gc_candidate_block_id){
            return false;
        }
    }
    if(plane->blocks[gc_candidate_block_id]->ongoing_user_program_cnt > 0)
        return false;
    
    if(plane->blocks[gc_candidate_block_id]->has_ongoing_gc)
        return false;
    return true;
}
