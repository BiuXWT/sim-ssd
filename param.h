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

#define PRINT_ERROR(MSG)               \
    {                                  \
        std::cerr << "ERROR:";         \
        std::cerr << MSG << std::endl; \
        std::cin.get();                \
        exit(1);                       \
    }
#define PRINT_MESSAGE(M) std::cout << M << std::endl;

#define LPN_TO_UNIQUE_KEY(STREAM, LPA) ((((uint64_t)STREAM) << 56) | LPA)
#define UNIQUE_KEY_TO_LPN(STREAM, LPA) ((~(((uint64_t)STREAM) << 56)) & LPA)

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

enum class SLC_CACHE_MODE
{
    SLC_CACHE_MODE_NONE,
    SLC_CACHE_MODE_STATIC,
    SLC_CACHE_MODE_DYNAMIC
};

enum class GC_POLICY
{
    GREEDY,
    RGA,
    RANDOM,
    RANDOM_P,
    RANDOM_PP,
    FIFO
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

enum class CMTSharingMode
{
    SHARED,
    EQUAL_SIZE_PARTITIONING
};

struct CacheParam
{
    CACHE_MODE mode;
    uint64_t cache_size;       // in MB
    uint64_t CMT_size;         // in MB
    uint64_t Read_cache_size;  // in MB
    uint64_t Write_cache_size; // in MB
};

struct GcParam
{
    GC_POLICY mode = GC_POLICY::GREEDY;
    double gc_threshold_high = 0.8;
    double gc_threshold_low = 0.2;
};

struct SlcCacheParam
{
    SLC_CACHE_MODE mode = SLC_CACHE_MODE::SLC_CACHE_MODE_DYNAMIC;
    uint64_t slc_static_size = 128; // in blocks
    double slc_dynamic_ratio = 0.3; // in percentage
};

struct SSDParam
{
    GcParam gc_param;
    SlcCacheParam slc_cache_param;
    MAPPING_MODE mapping_mode = MAPPING_MODE::MAPPING_MODE_PAGE_LEVEL;
    uint64_t ChannelNum = 2;
    uint64_t ChipPerChannel = 2;
};

struct NandParam
{
    uint64_t DiePerChip = 2;
    uint64_t PlanePerDie = 2;
    uint64_t BlockPerPlane = 64;
    uint64_t PagePerBlock = 8;
    uint64_t PageSize = 16384;
    uint64_t SpareSize = 2208;
};

struct Config
{
    SSDParam ssd_param;
    NandParam nand_param;
};

class FTL;
class UserRequest;
class TransactionErase;
class PlaneBookKeeping;
class BlockManager;
class BlockSlot;
class AddressMappingPageLevel;
class NandDriver;
class NandChip;
class PhysicalPageAddress;
class GcWlUnit;
class CMTSlot;
class CachedMappingTable;
class Transaction;
class TransactionRead;
class TransactionWrite;
class TransactionErase;

using FTLPtr = std::shared_ptr<FTL>;
using TransactionPtr = std::shared_ptr<Transaction>;
using TransactionReadPtr = std::shared_ptr<TransactionRead>;
using TransactionWritePtr = std::shared_ptr<TransactionWrite>;
using TransactionErasePtr = std::shared_ptr<TransactionErase>;
using UserRequestPtr = std::shared_ptr<UserRequest>;
using AddressMappingPageLevelPtr = std::shared_ptr<AddressMappingPageLevel>;
using BlockPtr = std::shared_ptr<BlockSlot>;
using BlockManagerPtr = std::shared_ptr<BlockManager>;
using PlaneBookKeepingPtr = std::shared_ptr<PlaneBookKeeping>;
using NandDriverPtr = std::shared_ptr<NandDriver>;
using NandChipPtr = std::shared_ptr<NandChip>;
using PhysicalPageAddressPtr = std::shared_ptr<PhysicalPageAddress>;
using GcWlUnitPtr = std::shared_ptr<GcWlUnit>;
using TransactionErasePtr = std::shared_ptr<TransactionErase>;
using CMTSlotPtr = std::shared_ptr<CMTSlot>;
using CachedMappingTablePtr = std::shared_ptr<CachedMappingTable>;
