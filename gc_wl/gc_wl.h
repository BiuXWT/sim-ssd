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
    GcWlUnit(AddressMappingPageLevelPtr amu, BlockManagerPtr bmu, NandDriverPtr nd,
             GC_POLICY gc_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold,
             uint64_t channel_count, uint64_t chip_per_channel, uint64_t die_per_chip,
             uint64_t plane_per_die, uint64_t block_per_plane, uint64_t page_per_block, uint64_t sectors_per_page,
             bool use_copyback, double rho, uint64_t max_on_going_gc_reqs_per_plane,
             bool dynamic_wl_enabled, bool static_wl_enabled, uint64_t static_wl_threshold);
    ~GcWlUnit() = default;

    bool GcIsUrgentMode(NandChipPtr chip);
    void CheckGcRequired(const uint64_t free_block_pool_size, const PhysicalPageAddressPtr plane_address);
    GC_POLICY GetGcPolicy() const { return gc_policy; }
    uint64_t GetGcPolicySpecificParam();
    uint64_t GetMinimumNumberOfFreePagesBeforeGc();
    bool StopServicingWrites(const PhysicalPageAddressPtr plane_address);
    bool IsSafeGcCandidate(PlaneBookKeepingPtr plane, uint64_t gc_candidate_block_id);
    bool UseStaticWearLeveling() const { return static_wl_enabled; }
    bool UseDynamicWearLeveling() const { return dynamic_wl_enabled; }

private:
    GC_POLICY gc_policy;
    AddressMappingPageLevelPtr address_mapping;
    BlockManagerPtr block_manager;
    NandDriverPtr nand_driver;

    bool use_copyback;
    bool preemptible_gc_enabled;
    double gc_hard_threshold;
    uint64_t block_pool_gc_threshold;
    uint64_t block_pool_gc_hard_threshold;
    uint64_t max_ongoing_gc_reqs_per_plane;
    uint64_t rga_set_size;

    bool dynamic_wl_enabled;
    bool static_wl_enabled;
    uint64_t static_wl_threshold;

    std::mt19937 rng{std::random_device{}()}; // 随机数生成器
    std::uniform_int_distribution<int> dist;  // 随机数分布
    int GetRandomBlockId();

    std::queue<BlockPtr> block_usage_fifo; // 用于 FIFO 策略的块使用历史队列
    uint64_t random_pp_threshold;          // 用于 RANDOM_PP 策略的阈值

    uint64_t channel_count;
    uint64_t chip_per_channel;
    uint64_t die_per_chip;
    uint64_t plane_per_die;
    uint64_t block_per_plane;
    uint64_t page_per_block;
    uint64_t sectors_per_page;
};
