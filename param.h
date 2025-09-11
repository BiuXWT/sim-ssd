#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <vector>
#include <set>
#include <unordered_map>
#include <queue>
#include <map>
#include <memory>
#include <cassert>
#include <random>
#include <algorithm>

#define PRINT_ERROR(MSG) {\
							std::cerr << "ERROR:" ;\
							std::cerr << MSG << std::endl; \
							std::cin.get();\
							exit(1);\
						 }
#define PRINT_MESSAGE(M) std::cout << M << std::endl;


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

enum class TransactionType
{
    READ,
    WRITE,
    ERASE,
    TRIM
};
enum class TransactionSourceType
{
    USERIO,
    CACHE,
    GC,
    MAPPING
};
enum class Priority
{
    URGENT,
    HIGH,
    MEDIUM,
    LOW
};
enum class UserRequestType
{
    READ,
    WRITE,
    TRIM
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
    GC_MODE mode=GC_MODE::GC_MODE_GREEDY;
    double gc_threshold_high=0.8;
    double gc_threshold_low=0.2;
};

struct SlcCacheParam
{
    SLC_CACHE_MODE mode=SLC_CACHE_MODE::SLC_CACHE_MODE_DYNAMIC;
    uint32_t slc_static_size=128; // in blocks
    double slc_dynamic_ratio=0.3; // in percentage
};


struct SSDParam{
    GcParam gc_param;
    SlcCacheParam slc_cache_param;
    MAPPING_MODE mapping_mode=MAPPING_MODE::MAPPING_MODE_PAGE_LEVEL;
    uint32_t ChannelNum=2;
    uint32_t ChipPerChannel=2;
};

struct NandParam{
    uint32_t DiePerChip=2;
    uint32_t PlanePerDie=2;
    uint32_t BlockPerPlane=64;
    uint32_t PagePerBlock=8;
    uint32_t PageSize=16384;
    uint32_t SpareSize=2208;
};

struct Config{
    SSDParam ssd_param;
    NandParam nand_param;

};