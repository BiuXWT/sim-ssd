#pragma once
#include "param.h"



class PageDataCacheSlot;
class GlobalTranslationDirectorySlot;
class MappingTableCacheSlot;
class DataCache;

using PageDataCacheSlotPtr = std::shared_ptr<PageDataCacheSlot>;
using GlobalTranslationDirectorySlotPtr = std::shared_ptr<GlobalTranslationDirectorySlot>;
using MappingTableCacheSlotPtr = std::shared_ptr<MappingTableCacheSlot>;
using DataCachePtr = std::shared_ptr<DataCache>;

extern Config config;

enum class CacheStatus
{
    EMPTY,
    CLEAN,
    DIRTY_NO_FLUSH, //脏页且没有写回
    DIRTY_FLUSH
};

struct PageDataCacheSlot
{
    uint64_t sector_bitmap; //page中有效的sector位图
    uint64_t LPA;
    uint64_t time_stamp;
    std::vector<uint8_t> data; // 缓存页数据
    CacheStatus status;
    std::list<std::pair<uint64_t,PageDataCacheSlotPtr>>::iterator lru_pos; //指向LRU链表中的位置 
};


struct GlobalTranslationDirectorySlot{
    uint64_t MPPN; //映射表所在的物理页号
    uint64_t time_stamp;
};

struct MappingTableCacheSlot{
    uint64_t PPA;
    uint64_t sector_bitmap; //记录Page中哪些sector已写入
    bool dirty;//是否为“脏页”（即是否需要写回 Flash）
    std::list<std::pair<uint64_t,MappingTableCacheSlotPtr>>::iterator pos; //指向LRU链表中的位置 
};



class DataCache { 
public:
    DataCache(size_t capacity_in_pages=0);
    ~DataCache();
    bool Exists(const uint64_t stream_id, const uint64_t lpa);
    bool Empty();
    bool Full();
    bool CheckFreeSlotAvailability();
    bool CheckFreeSlotAvailability(uint64_t required_free_slots);

    PageDataCacheSlot GetSlot(const uint64_t stream_id, const uint64_t lpa);
    PageDataCacheSlot EvictOneDirtyPage();
    PageDataCacheSlot EvictOnePageLRU();

    void ChangeSlotStatusToWriteBack(const uint64_t stream_id, const uint64_t lpa);
    void RemoveSlot(const uint64_t stream_id, const uint64_t lpa);
    void InsertReadData(const uint64_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data,const uint64_t timestamp, const uint64_t read_sector_bitmap);
    void InsertWriteData(const uint64_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data,const uint64_t timestamp, const uint64_t write_sector_bitmap);
    void UpdateData(const uint64_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data,const uint64_t timestamp, const uint64_t write_sector_bitmap);

private:
    std::unordered_map<uint64_t, PageDataCacheSlotPtr> slots; // key: LPN_TO_UNIQUE_KEY(STREAM,LPA)
    std::list<std::pair<uint64_t, PageDataCacheSlotPtr>> lru_list; // LRU链表，存储 (key, slot) pair
    size_t capacity_in_pages; // 缓存容量，以页为单位
};
