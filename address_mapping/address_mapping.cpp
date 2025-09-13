#include "address_mapping.h"

bool CachedMappingTable::Exists(uint64_t stream_id, uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    return it != address_map.end() && it->second->status == CMTEntryStatus::VALID;
}

uint64_t CachedMappingTable::RetrivePPA(const uint64_t stream_id, const uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    if(lru_list.begin()->first != key){
        lru_list.splice(lru_list.begin(), lru_list, it->second->pos);
    }
    return it->second->ppa;
}

void CachedMappingTable::Update(const uint64_t stream_id, const uint64_t lpn, const uint64_t ppa, const uint64_t write_state_bitmap)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    it->second->ppa = ppa;
    it->second->write_state_bitmap = write_state_bitmap;
    it->second->dirty = true;
    it->second->stream_id = stream_id;
}

void CachedMappingTable::Insert(const uint64_t stream_id, const uint64_t lpn, const uint64_t ppa, const uint64_t write_state_bitmap)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    it->second->ppa = ppa;
    it->second->write_state_bitmap = write_state_bitmap;
    it->second->dirty = false;
    it->second->status = CMTEntryStatus::VALID;
    it->second->stream_id = stream_id;
}

uint64_t CachedMappingTable::GetBitMap(const uint64_t stream_id, const uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    return it->second->write_state_bitmap;
}

bool CachedMappingTable::IsSlotReservedForLpnAndWaiting(const uint64_t stream_id, const uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it != address_map.end()) {
        if (it->second->status == CMTEntryStatus::WAITING) {
            return true;
        }
    }

    return false;
}
bool CachedMappingTable::CheckFreeSlotAvailability(){
    return address_map.size() < capacity_in_entries;
}

void CachedMappingTable::ReserveSlotForLpn(const uint64_t stream_id, const uint64_t lpn)
{

        uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);

		if (address_map.find(key) != address_map.end()) {
			throw std::logic_error("Duplicate lpa insertion into CMT!");
		}
		if (address_map.size() >= capacity_in_entries) {
			throw std::logic_error("CMT overfull!");
		}

		CMTSlotPtr cmtEnt = std::make_shared<CMTSlot>();
        cmtEnt->dirty = false;
        cmtEnt->stream_id = stream_id;
        lru_list.push_front({key, cmtEnt});
        cmtEnt->pos = lru_list.begin();
        cmtEnt->status = CMTEntryStatus::WAITING;
        address_map.insert({key, cmtEnt});
}

CMTSlotPtr CachedMappingTable::EvictOne(uint64_t &lpn)
{
    if(address_map.size() == 0){
        PRINT_ERROR("No slot to evict in CMT!")
    }
    address_map.erase(lru_list.back().first);
    lpn = UNIQUE_KEY_TO_LPN(lru_list.back().second->stream_id, lru_list.back().first);
    CMTSlotPtr evicted_slot = lru_list.back().second;
    lru_list.pop_back();
    return evicted_slot;
}

bool CachedMappingTable::IsDirty(const uint64_t stream_id, const uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    return it->second->dirty;
}
void CachedMappingTable::MakeClean(const uint64_t stream_id, const uint64_t lpn)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    it->second->dirty = false;
}
