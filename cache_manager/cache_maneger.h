#pragma once
#include "cache.h"
#include "nand_drive.h"
#include "ftl.h"

enum class Caching_Mode {WRITE_CACHE, READ_CACHE, WRITE_READ_CACHE, TURNED_OFF};
enum class Cache_Sharing_Mode { SHARED,//each application has access to the entire cache space
		                        EQUAL_PARTITIONING}; 

class CacheManager { 
public:
    CacheManager(FTLPtr ftl_ptr,NandDrivePtr nand_drive_ptr,uint64_t capacity_in_bytes,
    Caching_Mode* caching_mode_per_stream,Cache_Sharing_Mode cache_sharing_mode,uint64_t stream_cnt,
    uint64_t sector_per_page,uint64_t back_pressure_buffer_max_depth);
    ~CacheManager();

    void check_read(uint64_t stream_id, const uint64_t lpa, std::vector<uint8_t>& data, const uint64_t timestamp);
    void check_write(uint64_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data, const uint64_t timestamp);

private:
    NandDrivePtr nand_drive;
    FTLPtr ftl;

    uint64_t capacity_in_pages,capacity_in_bytes;
    uint64_t sector_no_per_page;
    bool memory_channel_is_busy;
    std::vector<std::vector<DataCache>> per_stream_cache; //每个流的缓存

    std::set<uint64_t> bloom_filter;
    uint64_t bloom_filter_reset_step = 1000000000; //重置时间间隔
    uint64_t next_bloom_filter_reset_milestone = 0;
};