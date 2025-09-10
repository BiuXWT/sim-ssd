#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <vector>
#include <set>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <algorithm>


enum class MAPPING_MODE
{
    MAPPING_MODE_PAGE_LEVEL,
    MAPPING_MODE_BLOCK_LEVEL,
    MAPPING_MODE_HYBRID
};

enum class CACHE_MODE
{
    CACHE_MODE_NONE,
    CACHE_MODE_DRAM,
    CACHE_MODE_HMB,
    CACHE_MODE_DRAM_HMB
};

enum class SLC_CACHE_MODE{
    SLC_CACHE_MODE_NONE,
    SLC_CACHE_MODE_STATIC,
    SLC_CACHE_MODE_DYNAMIC
};

enum class GC_MODE{
    GC_MODE_NONE,
    GC_MODE_GREEDY,
    GC_MODE_COST_BENEFIT
};

struct CacheParam
{
    CACHE_MODE mode;
    uint32_t cache_size; // in MB
    uint32_t CMT_size; // in MB
    uint32_t Read_cache_size; // in MB
    uint32_t Write_cache_size; // in MB
};

struct GcParam
{
    GC_MODE mode;
    double gc_threshold_high;
    double gc_threshold_low;
};

struct SlcCacheParam
{
    SLC_CACHE_MODE mode;
    uint32_t slc_static_size; // in blocks
    double slc_dynamic_ratio; // in percentage
};


struct SSDParam{
    GcParam gc_param;
    SlcCacheParam slc_cache_param;
    MAPPING_MODE mapping_mode;
    uint32_t ChannelNum;
    uint32_t ChipPerChannel;
};

struct NandParam{
    uint32_t DiePerChip;
    uint32_t PlanePerDie;
    uint32_t BlockPerPlane;
    uint32_t PagePerBlock;
    uint32_t PageSize;
    uint32_t SpareSize;
};

struct Config{
    SSDParam ssd_param;
    NandParam nand_param;

};