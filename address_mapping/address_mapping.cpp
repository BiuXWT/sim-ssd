#include "address_mapping.h"

bool CachedMappingTable::Exists(uint64_t stream_id, uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    return it != address_map.end() && it->second->status == CMTEntryStatus::VALID;
}

uint64_t CachedMappingTable::RetrievePPA(const uint64_t stream_id, const uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    if (lru_list.begin()->first != key)
    {
        lru_list.splice(lru_list.begin(), lru_list, it->second->pos);
    }
    return it->second->ppa;
}

void CachedMappingTable::Update(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
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

void CachedMappingTable::Insert(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
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

uint64_t CachedMappingTable::GetBitMap(const uint64_t stream_id, const uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    return it->second->write_state_bitmap;
}

bool CachedMappingTable::IsSlotReservedForLpnAndWaiting(const uint64_t stream_id, const uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    if (it != address_map.end())
    {
        if (it->second->status == CMTEntryStatus::WAITING)
        {
            return true;
        }
    }

    return false;
}
bool CachedMappingTable::CheckFreeSlotAvailability()
{
    return address_map.size() < capacity_in_entries;
}

void CachedMappingTable::ReserveSlotForLpn(const uint64_t stream_id, const uint64_t lpa)
{

    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);

    if (address_map.find(key) != address_map.end())
    {
        throw std::logic_error("Duplicate lpa insertion into CMT!");
    }
    if (address_map.size() >= capacity_in_entries)
    {
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

CMTSlotPtr CachedMappingTable::EvictOne(uint64_t &lpa)
{
    if (address_map.size() == 0)
    {
        PRINT_ERROR("No slot to evict in CMT!")
    }
    address_map.erase(lru_list.back().first);
    lpa = UNIQUE_KEY_TO_LPN(lru_list.back().second->stream_id, lru_list.back().first);
    CMTSlotPtr evicted_slot = lru_list.back().second;
    lru_list.pop_back();
    return evicted_slot;
}

bool CachedMappingTable::IsDirty(const uint64_t stream_id, const uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    return it->second->dirty;
}
void CachedMappingTable::MakeClean(const uint64_t stream_id, const uint64_t lpa)
{
    uint64_t key = LPN_TO_UNIQUE_KEY(stream_id, lpa);
    auto it = address_map.find(key);
    if (it == address_map.end() || it->second->status != CMTEntryStatus::VALID)
    {
        PRINT_ERROR("Mapping entry not found or not valid in CMT!")
    }
    it->second->dirty = false;
}

AddressMappingDomain::AddressMappingDomain(CachedMappingTablePtr cmt_ptr, uint64_t *channel_ids_, uint64_t channel_no, uint64_t *chip_ids_,
                                           uint64_t chip_no, uint64_t *die_ids_, uint64_t die_no, uint64_t *plane_ids_, uint64_t plane_no,
                                           uint64_t total_physical_sector_no, uint64_t total_logical_sector_no, uint64_t sectors_per_page)
    : cmt(cmt_ptr), channel_no(channel_no), chip_no(chip_no), die_no(die_no), plane_no(plane_no),
      channel_ids(new uint64_t[channel_no]), chip_ids(new uint64_t[chip_no]),
      die_ids(new uint64_t[die_no]), plane_ids(new uint64_t[plane_no])
{
    total_physical_page_no = total_physical_sector_no / sectors_per_page;
    max_logical_sector_address = total_logical_sector_no;
    total_logical_page_no = (max_logical_sector_address / sectors_per_page) + (max_logical_sector_address % sectors_per_page != 0 ? 1 : 0);

    for (uint64_t i = 0; i < channel_no; i++)
    {
        channel_ids[i] = channel_ids_[i];
    }
    for (uint64_t i = 0; i < chip_no; i++)
    {
        chip_ids[i] = chip_ids_[i];
    }
    for (uint64_t i = 0; i < die_no; i++)
    {
        die_ids[i] = die_ids_[i];
    }
    for (uint64_t i = 0; i < plane_no; i++)
    {
        plane_ids[i] = plane_ids_[i];
    }
    if (cmt_ptr == nullptr)
    {
        cmt = std::make_shared<CachedMappingTable>(total_logical_page_no); // 默认CMT大小为逻辑页数
    }
    else
    {
        cmt = cmt_ptr;
    }
}

void AddressMappingDomain::UpdateMappingInfo(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap)
{
    cmt->Update(stream_id, lpa, ppa, write_state_bitmap);
}

uint64_t AddressMappingDomain::GetPageStatus(const uint64_t stream_id, const uint64_t lpa)
{
    return cmt->GetBitMap(stream_id, lpa);
}

uint64_t AddressMappingDomain::GetPPA(const uint64_t stream_id, const uint64_t lpa)
{
    return cmt->RetrievePPA(stream_id, lpa);
}

bool AddressMappingDomain::Mapping_entry_accessible(const uint64_t stream_id, const uint64_t lpa)
{
    return cmt->Exists(stream_id, lpa);
}
