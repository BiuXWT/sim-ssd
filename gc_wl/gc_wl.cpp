#include "gc_wl.h"
#include "transaction.h"
#include "block_manager.h"
#include "nand_chip.h"

int GcWlUnit::GetRandomBlockId()
{
    return dist(rng);
}

GcWlUnit::GcWlUnit(AddressMappingPageLevelPtr amu, BlockManagerPtr bmu, NandDriverPtr nd, GC_POLICY gc_policy,
                   double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold, uint64_t channel_count,
                   uint64_t chip_per_channel, uint64_t die_per_chip, uint64_t plane_per_die, uint64_t block_per_plane,
                   uint64_t page_per_block, uint64_t sectors_per_page, bool use_copyback, double rho,
                   uint64_t max_ongoing_gc_reqs_per_plane, bool dynamic_wl_enabled, bool static_wl_enabled, uint64_t static_wl_threshold)
    : gc_policy(gc_policy), address_mapping(amu), block_manager(bmu), nand_driver(nd),
      use_copyback(use_copyback), preemptible_gc_enabled(preemptible_gc_enabled), gc_hard_threshold(gc_hard_threshold),
      max_ongoing_gc_reqs_per_plane(max_ongoing_gc_reqs_per_plane), dynamic_wl_enabled(dynamic_wl_enabled),
      static_wl_enabled(static_wl_enabled), static_wl_threshold(static_wl_threshold),
      channel_count(channel_count), chip_per_channel(chip_per_channel), die_per_chip(die_per_chip),
      plane_per_die(plane_per_die), block_per_plane(block_per_plane), page_per_block(page_per_block), sectors_per_page(sectors_per_page)
{
    block_pool_gc_threshold = static_cast<uint64_t>(gc_threshold * block_per_plane);
    if (block_pool_gc_threshold < 1)
        block_pool_gc_threshold = 1;
    block_pool_gc_hard_threshold = static_cast<uint64_t>(gc_hard_threshold * block_per_plane);
    if (block_pool_gc_hard_threshold < 1)
        block_pool_gc_hard_threshold = 1;
    random_pp_threshold = static_cast<uint64_t>(rho * page_per_block);
    if (random_pp_threshold < max_ongoing_gc_reqs_per_plane)
        random_pp_threshold = max_ongoing_gc_reqs_per_plane;
}

bool GcWlUnit::GcIsUrgentMode(NandChipPtr nand_chip)
{
    if (!preemptible_gc_enabled)
        return true;

    PhysicalPageAddressPtr addr = std::make_shared<PhysicalPageAddress>();
    addr->channel_id = nand_chip->channel_id;
    for (uint64_t die_id = 0; die_id < die_per_chip; die_id++)
    {
        for (uint64_t plane_id = 0; plane_id < plane_per_die; plane_id++)
        {
            addr->die_id = die_id;
            addr->plane_id = plane_id;
            if (block_manager->GetFreeBlockPoolSize(addr) < block_pool_gc_hard_threshold)
            {
                return true;
            }
        }
    }

    return false;
}

void GcWlUnit::CheckGcRequired(const uint64_t free_block_pool_size, const PhysicalPageAddressPtr plane_address)
{
    if (free_block_pool_size < block_pool_gc_threshold)
    {
        uint64_t gc_candidate_block_id = UINT32_MAX;
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
                if (plane->blocks[block_id]->invalid_page_count > plane->blocks[gc_candidate_block_id]->invalid_page_count && plane->blocks[block_id]->current_write_page_index == page_per_block // 已满
                    && IsSafeGcCandidate(plane, block_id))
                {
                    gc_candidate_block_id = block_id;
                }
                break;
            }
        case GC_POLICY::RGA:
        { // 从随机选择的几个块中选择最冷块
            std::set<uint64_t> candidate_set;
            while (candidate_set.size() < rga_set_size)
            {
                uint64_t id = GetRandomBlockId();
                if (plane->ongoing_erase_blocks.find(id) == plane->ongoing_erase_blocks.end() && IsSafeGcCandidate(plane, id))
                    candidate_set.insert(id);
            }
            gc_candidate_block_id = *candidate_set.begin();
            for (auto &id : candidate_set)
            {
                if (plane->blocks[id]->invalid_page_count > plane->blocks[gc_candidate_block_id]->invalid_page_count && plane->blocks[id]->current_write_page_index == page_per_block) // 已满
                {
                    gc_candidate_block_id = id;
                }
            }
            break;
        }
        case GC_POLICY::RANDOM:
        { // 随机选择一个块
            gc_candidate_block_id = GetRandomBlockId();
            uint64_t repeat = 0;
            while (!IsSafeGcCandidate(plane, gc_candidate_block_id) && repeat++ < block_per_plane)
            {
                gc_candidate_block_id = GetRandomBlockId();
            }
            break;
        }
        case GC_POLICY::RANDOM_P:
        { // 随机选择一个块，要求有效页数小于某个阈值
            gc_candidate_block_id = GetRandomBlockId();
            uint64_t repeat = 0;
            // 如果该块未写满，或（该块不安全且尝试次数未超限），则继续随机选块
            while (plane->blocks[gc_candidate_block_id]->current_write_page_index < page_per_block || (!IsSafeGcCandidate(plane, gc_candidate_block_id) && repeat++ < block_per_plane))
            {
                gc_candidate_block_id = GetRandomBlockId();
            }
            break;
        }
        case GC_POLICY::RANDOM_PP:
        { // 随机选择一个块，要求有效页数小于某个阈值，否则选择最冷块
            gc_candidate_block_id = GetRandomBlockId();
            uint64_t repeat = 0;

            while (plane->blocks[gc_candidate_block_id]->current_write_page_index < page_per_block || plane->blocks[gc_candidate_block_id]->invalid_page_count < random_pp_threshold || (!IsSafeGcCandidate(plane, gc_candidate_block_id) && repeat++ < block_per_plane))
            {
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

uint64_t GcWlUnit::GetGcPolicySpecificParam()
{
    switch (gc_policy)
    {
    case GC_POLICY::RGA:
        return rga_set_size;

    case GC_POLICY::RANDOM_PP:
        return random_pp_threshold;

    default:
        break;
    }
    return 0;
}

uint64_t GcWlUnit::GetMinimumNumberOfFreePagesBeforeGc()
{
    return block_pool_gc_threshold;
}

bool GcWlUnit::StopServicingWrites(const PhysicalPageAddressPtr plane_address)
{
    return block_manager->GetFreeBlockPoolSize(plane_address) < max_ongoing_gc_reqs_per_plane;
}

bool GcWlUnit::IsSafeGcCandidate(PlaneBookKeepingPtr plane, uint64_t gc_candidate_block_id)
{
    for (uint64_t stream_id = 0; stream_id < block_manager->GetInputStreamCnt(); ++stream_id)
    {
        if (plane->data_open_blocks[stream_id]->block_id == gc_candidate_block_id || plane->gc_open_blocks[stream_id]->block_id == gc_candidate_block_id || plane->translation_open_blocks[stream_id]->block_id == gc_candidate_block_id)
        {
            return false;
        }
    }
    if (plane->blocks[gc_candidate_block_id]->ongoing_user_program_cnt > 0)
        return false;

    if (plane->blocks[gc_candidate_block_id]->has_ongoing_gc)
        return false;
    return true;
}
