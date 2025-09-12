#pragma once
#include "param.h"

/*
•	GREEDY：效率优先，磨损不均。
•	RGA：兼顾效率和均衡。
•	RANDOM/RANDOM_P/RANDOM_PP：均衡优先，效率较低。
•	FIFO：顺序回收，简单易实现。 
*/

class GcWlUnit
{
public:
    GcWlUnit();
    ~GcWlUnit();

    bool GcIsUrgentMode(NandChipPtr chip);
    void CheckGcRequired(const uint32_t free_block_pool_size, const PhysicalPageAddress &plane_address);
    GC_POLICY GetGcPolicy() const { return gc_policy; }
    uint32_t GetGcPolicySpecificParam();
    uint32_t GetMinimumNumberOfFreePagesBeforeGc();
    bool StopServicingWrites(const PhysicalPageAddress &plane_address);
    bool IsSafeGcCandidate(PlaneBookKeepingPtr plane, uint32_t gc_candidate_block_id);

private:
    GC_POLICY gc_policy;
    AddressMappingPtr address_mapping;
    BlockManagerPtr block_manager;
    NandDriverPtr nand_driver;

    bool use_copyback;
    bool preemptible_gc_enabled;
    double gc_hard_threshold;
    uint32_t block_pool_gc_threshold;
    uint32_t block_pool_gc_hard_threshold;
    uint32_t max_ongoing_gc_reqs_per_plane;
    uint32_t rga_set_size;

    std::mt19937 rng{std::random_device{}()}; // 随机数生成器
    std::uniform_int_distribution<int> dist;  // 随机数分布
    int GetRandomBlockId();

    std::queue<BlockPtr> block_usage_fifo; // 用于 FIFO 策略的块使用历史队列
    uint32_t random_pp_threshold;          // 用于 RANDOM_PP 策略的阈值

    uint32_t channel_count;
    uint32_t chip_per_channel;
    uint32_t die_per_chip;
    uint32_t plane_per_die;
    uint32_t block_per_plane;
    uint32_t page_per_block;
    uint32_t sectors_per_page;
};