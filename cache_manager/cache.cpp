#include "cache.h"

DataCache::DataCache(size_t capacity_in_pages)
    : capacity_in_pages(capacity_in_pages) {}

DataCache::~DataCache() {}

bool DataCache::Exists(const uint8_t stream_id, const uint64_t lpa) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    return slots.find(key) != slots.end();
}

bool DataCache::Empty() {
    return slots.empty();
}

bool DataCache::Full() {
    return slots.size() >= capacity_in_pages;
}

bool DataCache::CheckFreeSlotAvailability() {
    return slots.size() < capacity_in_pages;
}

bool DataCache::CheckFreeSlotAvailability(uint64_t required_free_slots) {
    return slots.size() + required_free_slots <= capacity_in_pages;
}

PageDataCacheSlot DataCache::GetSlot(const uint8_t stream_id, const uint64_t lpa) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = slots.find(key);
    if (it == slots.end()) {
        // 未找到，返回空对象
        return PageDataCacheSlot{};
    }
    if(lru_list.begin()->first != key){
        lru_list.splice(lru_list.begin(), lru_list, it->second->lru_pos);
    }
    return *(it->second);
}

PageDataCacheSlot DataCache::EvictOneDirtyPage() {
    if (slots.empty()) {
        // 可加日志
        return PageDataCacheSlot{};
    }
    auto it = lru_list.rbegin();
    while(it != lru_list.rend()){
        if(it->second->status == CacheStatus::DIRTY_NO_FLUSH){
            auto base_it = std::prev(it.base());
            auto evicted_item = *base_it->second;
            slots.erase(base_it->first);
            lru_list.erase(base_it);
            return evicted_item;
        }
        ++it;
    }
    auto evicted_it = lru_list.back();
    auto evicted_item = *evicted_it.second;
    evicted_item.status = CacheStatus::EMPTY;
    slots.erase(evicted_it.first);
    lru_list.pop_back();
    return evicted_item;
}

PageDataCacheSlot DataCache::EvictOnePageLRU() {
    if (slots.empty()) {
        return PageDataCacheSlot{};
    }
    slots.erase(lru_list.back().first);
    auto evicted_item = *lru_list.back().second;
    lru_list.pop_back();
    return evicted_item;
}

void DataCache::ChangeSlotStatusToWriteBack(const uint8_t stream_id, const uint64_t lpa) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = slots.find(key);
    if (it != slots.end()) {
        it->second->status = CacheStatus::DIRTY_FLUSH;
    }
}

void DataCache::RemoveSlot(const uint8_t stream_id, const uint64_t lpa) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = slots.find(key);
    if (it != slots.end()) {
        slots.erase(it);
        lru_list.erase(it->second->lru_pos);
    }
}

void DataCache::InsertReadData(const uint8_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data, const uint64_t timestamp, const uint64_t read_sector_bitmap) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    if(slots.find(key) == slots.end()){
        if(slots.size() >= capacity_in_pages){
            EvictOnePageLRU();
        }
        // 有空间，直接插入
        PageDataCacheSlotPtr slot = std::make_shared<PageDataCacheSlot>();
        slot->LPA = lpa;
        slot->data = data;
        slot->sector_bitmap = read_sector_bitmap;
        slot->status = CacheStatus::CLEAN;
        slot->time_stamp = timestamp;
        lru_list.push_front({key, slot});
        slot->lru_pos = lru_list.begin();
        slots.insert({key, slot});
    }
}

void DataCache::InsertWriteData(const uint8_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data, const uint64_t timestamp, const uint64_t write_sector_bitmap) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    if(slots.find(key) == slots.end()){
        if(slots.size() >= capacity_in_pages){
            EvictOnePageLRU();
        }
        // 有空间，直接插入
        PageDataCacheSlotPtr slot = std::make_shared<PageDataCacheSlot>();
        slot->LPA = lpa;
        slot->data = data;
        slot->sector_bitmap = write_sector_bitmap;
        slot->status = CacheStatus::DIRTY_NO_FLUSH;
        slot->time_stamp = timestamp;
        lru_list.push_front({key, slot});
        slot->lru_pos = lru_list.begin();
        slots.insert({key, slot});
    }
}

void DataCache::UpdateData(const uint8_t stream_id, const uint64_t lpa, const std::vector<uint8_t>& data, const uint64_t timestamp, const uint64_t write_sector_bitmap) {
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = slots.find(key);
    if (it != slots.end()) {
        it->second->LPA = lpa;
        it->second->sector_bitmap = write_sector_bitmap;
        it->second->data = data;
        it->second->time_stamp = timestamp;
        it->second->status = CacheStatus::DIRTY_NO_FLUSH;
        if(lru_list.begin()->first != key){
            lru_list.splice(lru_list.begin(), lru_list, it->second->lru_pos);
        }
    }
}
