#pragma once
#include "param.h"

	enum class CMTEntryStatus {FREE, WAITING, VALID};

class CMTSlot{
public:
    uint64_t ppa;
    uint64_t write_state_bitmap; // 记录Page中哪些sector已写入
    bool dirty;//是否为“脏页”（即是否需要写回 Flash）
    CMTEntryStatus status;
    std::list<std::pair<uint64_t, CMTSlotPtr>>::iterator pos;
    uint64_t stream_id;
};

class CachedMappingTable{
public:
    bool Exists(const uint64_t stream_id, const uint64_t lpn);
    uint64_t RetrivePPA(const uint64_t stream_id, const uint64_t lpn);
    void Update(const uint64_t stream_id, const uint64_t lpn, const uint64_t ppa, const uint64_t write_state_bitmap);
    void Insert(const uint64_t stream_id, const uint64_t lpn, const uint64_t ppa, const uint64_t write_state_bitmap);
    uint64_t GetBitMap(const uint64_t stream_id, const uint64_t lpn);
    bool IsSlotReservedForLpnAndWaiting(const uint64_t stream_id, const uint64_t lpn);
    bool CheckFreeSlotAvailability();
    void ReserveSlotForLpn(const uint64_t stream_id, const uint64_t lpn);
    CMTSlotPtr EvictOne(uint64_t &lpn);
    bool IsDirty(const uint64_t stream_id, const uint64_t lpn);
    void MakeClean(const uint64_t stream_id, const uint64_t lpn);

private:
    std::unordered_map<uint64_t,CMTSlotPtr> address_map; // key: LPN, value: slot_ptr
    std::list<std::pair<uint64_t,CMTSlotPtr>> lru_list; // LRU链表，存储 (LPN, slot) pair
    size_t capacity_in_entries; // 缓存容量，以映射表项数为
};



class AddressMapping {
public:
};