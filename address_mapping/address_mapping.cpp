#include "address_mapping.h"
#include "transaction.h"
#include "block_manager.h"
#include "ftl.h"
#include "gc_wl.h"
#include "nand_driver.h"

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

//============================================== AddressMappingDomain ==============================================

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

//============================================== AddressMappingPageLevel ==============================================

uint64_t AddressMappingPageLevel::GetCMTCapacity()
{
    return cmt_capacity_in_entries;
}

uint64_t AddressMappingPageLevel::GetLogicalPagesNo(uint64_t stream_id)
{
    return domains[stream_id]->total_logical_page_no;
}

void AddressMappingPageLevel::GetDataMappingForGC(uint64_t stream_id, uint64_t lpa, uint64_t &ppa, uint64_t &write_state_bitmap)
{
    if (domains[stream_id]->Mapping_entry_accessible(stream_id, lpa))
    {
        ppa = domains[stream_id]->GetPPA(stream_id, lpa);
        write_state_bitmap = domains[stream_id]->GetPageStatus(stream_id, lpa);
    }
    else
    {
        PRINT_ERROR("Mapping entry not found in CMT during GC!")
    }
}

void AddressMappingPageLevel::AllocateNewPageForGC(TransactionWritePtr tr)
{
    AllocatePageInPlaneForGCWrite(tr);
    tr->physical_address_determined = true;
}

PhysicalPageAddressPtr AddressMappingPageLevel::ConvertPPAtoAddress(const uint64_t ppa)
{
    // Ensure pages_per_channel and related variables are initialized before use
    if (pages_per_channel == 0 || pages_per_chip == 0 || pages_per_die == 0 || pages_per_plane == 0 || pages_per_block == 0)
    {
        throw std::logic_error("One or more page mapping variables are not initialized!");
    }
    PhysicalPageAddressPtr target = std::make_shared<PhysicalPageAddress>();
    target->channel_id = ppa / pages_per_channel;
    target->chip_id = (ppa % pages_per_channel) / pages_per_chip;
    target->die_id = ((ppa % pages_per_channel) % pages_per_chip) / pages_per_die;
    target->plane_id = (((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) / pages_per_plane;
    target->block_id = ((((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) % pages_per_plane) / pages_per_block;
    target->page_id = (((((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) % pages_per_plane) % pages_per_block);

    return target;
}

void AddressMappingPageLevel::ConvertPPAtoAddress(const uint64_t ppa, PhysicalPageAddressPtr address)
{
    if (pages_per_channel == 0 || pages_per_chip == 0 || pages_per_die == 0 || pages_per_plane == 0 || pages_per_block == 0)
    {
        throw std::logic_error("One or more page mapping variables are not initialized!");
    }
    address->channel_id = ppa / pages_per_channel;
    address->chip_id = (ppa % pages_per_channel) / pages_per_chip;
    address->die_id = ((ppa % pages_per_channel) % pages_per_chip) / pages_per_die;
    address->plane_id = (((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) / pages_per_plane;
    address->block_id = ((((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) % pages_per_plane) / pages_per_block;
    address->page_id = (((((ppa % pages_per_channel) % pages_per_chip) % pages_per_die) % pages_per_plane) % pages_per_block);
}

uint64_t AddressMappingPageLevel::ConvertAddresstoPPA(const PhysicalPageAddressPtr address)
{
    return pages_per_chip * (address->channel_id * chips_per_channel + address->chip_id) + pages_per_die * address->die_id + pages_per_plane * address->plane_id + pages_per_block * address->block_id + address->page_id;
}

void AddressMappingPageLevel::SetBarrierForPhysicalBlock(const PhysicalPageAddressPtr address)
{
    auto block = block_manager->GetPlaneBookKeepingEntry(address)->blocks[address->block_id];
    auto addr = std::make_shared<PhysicalPageAddress>(*address);
    for (uint64_t page_id = 0; page_id < block->current_write_page_index; ++page_id)
    {
        if (block_manager->IsPageValid(block, page_id))
        {
            addr->page_id = page_id;
            uint64_t lpa = nand_driver->GetLPA(addr);
            uint64_t ppa = domains[block->stream_id]->GetPPA(block->stream_id, lpa);
            if (domains[block->stream_id]->cmt->Exists(block->stream_id, lpa))
            {
                ppa = domains[block->stream_id]->cmt->RetrievePPA(block->stream_id, lpa);
            }
            if (ppa != ConvertAddresstoPPA(addr))
            {
                PRINT_ERROR("Inconsistent mapping table between FTL and NAND driver!")
            }
            SetBarrierForLPA(block->stream_id, lpa);
        }
    }
}

void AddressMappingPageLevel::SetBarrierForLPA(const uint64_t stream_id, const uint64_t lpa)
{
    auto it = domains[stream_id]->locked_lpa.find(lpa);
    if (it != domains[stream_id]->locked_lpa.end())
    {
        PRINT_ERROR("LPA already locked!");
    }
    domains[stream_id]->locked_lpa.insert(lpa);
}

void AddressMappingPageLevel::RemoveBarrierForLPA(const uint64_t stream_id, const uint64_t lpa)
{
    auto it = domains[stream_id]->locked_lpa.find(lpa);
    if (it == domains[stream_id]->locked_lpa.end())
    {
        PRINT_ERROR("LPA not locked!");
    }
    domains[stream_id]->locked_lpa.erase(it);
    // TODO...
    // read behind barrier

    // program behind barrier
}

void AddressMappingPageLevel::AllocatePlaneForUserWrite(TransactionWritePtr tr)
{
    uint64_t lpa = tr->lpa;
    PhysicalPageAddressPtr targetAddress = tr->physical_address;
    // 实现 LPA 在所有 plane 间的均匀分布。
    targetAddress->channel_id = domains[tr->stream_id]->channel_ids[lpa % domains[tr->stream_id]->channel_no];
    targetAddress->chip_id = domains[tr->stream_id]->chip_ids[(lpa / domains[tr->stream_id]->channel_no) % domains[tr->stream_id]->chip_no];
    targetAddress->die_id = domains[tr->stream_id]->die_ids[(lpa / (domains[tr->stream_id]->channel_no * domains[tr->stream_id]->chip_no)) % domains[tr->stream_id]->die_no];
    targetAddress->plane_id = domains[tr->stream_id]->plane_ids[(lpa / (domains[tr->stream_id]->channel_no * domains[tr->stream_id]->chip_no * domains[tr->stream_id]->die_no)) % domains[tr->stream_id]->plane_no];
}

void AddressMappingPageLevel::AllocatePageInPlaneForUserWrite(TransactionWritePtr tr)
{
    auto domain = domains[tr->stream_id];
    uint64_t old_ppa = domain->GetPPA(tr->stream_id, tr->lpa);

    uint64_t prev_page_bitmap = domain->GetPageStatus(tr->stream_id, tr->lpa);
    uint64_t status_intersection = prev_page_bitmap & tr->write_sectors_bitmap;
    if (status_intersection == prev_page_bitmap)
    { // 新写入完全覆盖先前写入的扇区，直接把原page标记为无效
        PhysicalPageAddressPtr old_address = ConvertPPAtoAddress(old_ppa);
        block_manager->InvalidatePageInBlock(tr->stream_id, old_address);
    }
    else // 新写入未完全覆盖先前的扇区，需要读取旧数据
    {
        uint64_t read_page_bitmap = status_intersection ^ prev_page_bitmap; // 新写入没有覆盖到的扇区
        read_page_bitmap &= 1;                                              // delete
        auto update_read_tr = std::make_shared<TransactionRead>(/*TODO */);
        ConvertPPAtoAddress(old_ppa, update_read_tr->physical_address);
        block_manager->ReadTransactionStartedOnBlock(update_read_tr->physical_address);
        block_manager->InvalidatePageInBlock(tr->stream_id, update_read_tr->physical_address);
        tr->related_read = update_read_tr;
    }
    block_manager->AllocateBlockAndPageInPlaneForUserWrite(tr->stream_id, tr->physical_address);
    tr->ppa = ConvertAddresstoPPA(tr->physical_address);
    domain->UpdateMappingInfo(tr->stream_id, tr->lpa, tr->ppa, tr->write_sectors_bitmap | domain->GetPageStatus(tr->stream_id, tr->lpa));
}

void AddressMappingPageLevel::AllocatePageInPlaneForGCWrite(TransactionWritePtr tr)
{
    auto domain = domains[tr->stream_id];
    uint64_t old_ppa = domain->GetPPA(tr->stream_id, tr->lpa);
    if (old_ppa == NO_VALUE)
    {
        PRINT_ERROR("Unexpected mapping table status for GC write!");
    }
    else
    {
        PhysicalPageAddressPtr old_address = ConvertPPAtoAddress(old_ppa);
        block_manager->InvalidatePageInBlock(tr->stream_id, old_address);
        uint64_t prev_page_bitmap = domain->GetPageStatus(tr->stream_id, tr->lpa);
        if ((prev_page_bitmap & tr->write_sectors_bitmap) != prev_page_bitmap)
        {
            PRINT_ERROR("Unexpected mapping table status for GC write!");
        }
    }
    block_manager->AllocateBlockAndPageInPlaneForGcWrite(tr->stream_id, tr->physical_address);
    tr->ppa = ConvertAddresstoPPA(tr->physical_address);
    domain->UpdateMappingInfo(tr->stream_id, tr->lpa, tr->ppa, tr->write_sectors_bitmap | domain->GetPageStatus(tr->stream_id, tr->lpa));
}

bool AddressMappingPageLevel::TranslateLpaToPpa(uint64_t stream_id, TransactionPtr tr)
{
    auto domain = domains[stream_id];
    auto ppa = domain->GetPPA(stream_id, tr->lpa);
    if (tr->type == TransactionType::READ)
    {
        if (ppa == NO_VALUE)
        {
            ppa = OnlineCreateEntryForRead(stream_id, tr->lpa, tr->physical_address, static_cast<TransactionRead *>(tr.get())->read_sectors_bitmap);
        }
        tr->ppa = ppa;
        ConvertPPAtoAddress(ppa, tr->physical_address);
        block_manager->ReadTransactionStartedOnBlock(tr->physical_address);
        tr->physical_address_determined = true;
        return true;
    }
    else
    {
        AllocatePlaneForUserWrite(std::dynamic_pointer_cast<TransactionWrite>(tr));
        if (ftl->gcwl_unit->StopServicingWrites(tr->physical_address))
        {
            return false;
        }
        AllocatePageInPlaneForUserWrite(std::dynamic_pointer_cast<TransactionWrite>(tr));
        tr->physical_address_determined = true;
        return true;
    }
}

bool AddressMappingPageLevel::QueryCMT(TransactionPtr tr)
{
    uint64_t stream_id = tr->stream_id;
    if (domains[stream_id]->Mapping_entry_accessible(stream_id, tr->lpa))
    {
        if (TranslateLpaToPpa(stream_id, tr))
        {
            return true;
        }
        else
        {
            ManageUnsuccessfulTransaction(tr);
            return false;
        }
    }
    return false;
}

uint64_t AddressMappingPageLevel::OnlineCreateEntryForRead(uint64_t stream_id, uint64_t lpa, PhysicalPageAddressPtr addr, uint64_t read_sectors_bitmap)
{
    auto domain = domains[stream_id];
    addr->channel_id = domain->channel_ids[lpa % domain->channel_no];
    addr->chip_id = domain->chip_ids[(lpa / domain->channel_no) % domain->chip_no];
    addr->die_id = domain->die_ids[(lpa / (domain->channel_no * domain->chip_no)) % domain->die_no];
    addr->plane_id = domain->plane_ids[(lpa / (domain->channel_no * domain->chip_no * domain->die_no)) % domain->plane_no];

    block_manager->AllocateBlockAndPageInPlaneForUserWrite(stream_id, addr);
    uint64_t ppa = ConvertAddresstoPPA(addr);
    domain->UpdateMappingInfo(stream_id, lpa, ppa, read_sectors_bitmap);
    return ppa;
}

void AddressMappingPageLevel::ManageUnsuccessfulTransaction(TransactionPtr tr)
{
    Write_transactions_for_overfull_planes[tr->physical_address->channel_id][tr->physical_address->chip_id][tr->physical_address->die_id]->insert(std::dynamic_pointer_cast<TransactionWrite>(tr));
}

void AddressMappingPageLevel::ManageUserTransactionFacingBarrier(TransactionPtr tr)
{
    auto domain = domains[tr->stream_id];
    if (tr->type == TransactionType::READ)
    {
        domain->read_transactions_behind_LPA_barrier.insert({tr->lpa, tr});
    }
    else
    {
        domain->program_transactions_behind_LPA_barrier.insert({tr->lpa, tr});
    }
}

bool AddressMappingPageLevel::IsLPALockedForGC(const uint64_t stream_id, const uint64_t lpa)
{
    return domains[stream_id]->locked_lpa.find(lpa) != domains[stream_id]->locked_lpa.end();
}
