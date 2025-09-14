#pragma once
#include "param.h"
#include "nand_driver.h"

enum class CMTEntryStatus
{
    FREE,
    WAITING,
    VALID
};

class CMTSlot
{
public:
    uint64_t ppa;
    uint64_t write_state_bitmap; // 记录Page中哪些sector已写入
    bool dirty;                  // 是否为“脏页”（即是否需要写回 Flash）
    CMTEntryStatus status;
    std::list<std::pair<uint64_t, CMTSlotPtr>>::iterator pos;
    uint64_t stream_id;
};

class CachedMappingTable
{
public:
    CachedMappingTable(uint64_t capacity) : capacity_in_entries(capacity) {}
    ~CachedMappingTable();
    bool Exists(const uint64_t stream_id, const uint64_t lpa);
    // 获取 LPA 对应的 PPA;Retrieve:检索
    uint64_t RetrievePPA(const uint64_t stream_id, const uint64_t lpa);
    void Update(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap);
    void Insert(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap);
    uint64_t GetBitMap(const uint64_t stream_id, const uint64_t lpa);
    bool IsSlotReservedForLpnAndWaiting(const uint64_t stream_id, const uint64_t lpa);
    bool CheckFreeSlotAvailability();
    void ReserveSlotForLpn(const uint64_t stream_id, const uint64_t lpa);
    CMTSlotPtr EvictOne(uint64_t &lpa);
    bool IsDirty(const uint64_t stream_id, const uint64_t lpa);
    void MakeClean(const uint64_t stream_id, const uint64_t lpa);

private:
    std::unordered_map<uint64_t, CMTSlotPtr> address_map; // key: LPN, value: slot_ptr
    std::list<std::pair<uint64_t, CMTSlotPtr>> lru_list;  // LRU链表，存储 (LPN, slot) pair
    size_t capacity_in_entries;                           // 缓存容量，以映射表项数为
};

class AddressMappingDomain
{
public:
    AddressMappingDomain(CachedMappingTablePtr cmt_ptr, uint64_t *channel_ids, uint64_t channel_no,
                         uint64_t *chip_ids, uint64_t chip_no, uint64_t *die_ids, uint64_t die_no,
                         uint64_t *plane_ids, uint64_t plane_no, uint64_t total_physical_sector_no,
                         uint64_t total_logical_sector_no, uint64_t sectors_per_page);
    ~AddressMappingDomain() = default;
    void UpdateMappingInfo(const uint64_t stream_id, const uint64_t lpa, const uint64_t ppa, const uint64_t write_state_bitmap);
    uint64_t GetPageStatus(const uint64_t stream_id, const uint64_t lpa);
    uint64_t GetPPA(const uint64_t stream_id, const uint64_t lpa);
    bool Mapping_entry_accessible(const uint64_t stream_id, const uint64_t lpa);

    uint64_t CMT_entry_size;
    CachedMappingTablePtr cmt;
    std::multimap<uint64_t, TransactionPtr> waiting_unmapped_read_transactions;    // key: LPA, value: tr_ptr
    std::multimap<uint64_t, TransactionPtr> waiting_unmapped_program_transactions; // key: LPA, value: tr_ptr
    std::set<uint64_t> locked_lpa;
    std::multimap<uint64_t, TransactionPtr> read_transactions_behind_LPA_barrier;    // key: LPA, value: tr_ptr
    std::multimap<uint64_t, TransactionPtr> program_transactions_behind_LPA_barrier; // key: LPA, value: tr_ptr

    std::shared_ptr<uint64_t[]> channel_ids;
    uint64_t channel_no;
    std::shared_ptr<uint64_t[]> chip_ids;
    uint64_t chip_no;
    std::shared_ptr<uint64_t[]> die_ids;
    uint64_t die_no;
    std::shared_ptr<uint64_t[]> plane_ids;
    uint64_t plane_no;

    uint64_t max_logical_sector_address;
    uint64_t total_logical_page_no;
    uint64_t total_physical_page_no;
};

class AddressMappingPageLevel
{
public:
    AddressMappingPageLevel();
    ~AddressMappingPageLevel() = default;

private:
    FTLPtr ftl;
    BlockManagerPtr block_manager;
    CMTSharingMode sharing_mode;
    uint64_t total_stream_count;
    uint64_t max_logical_sector_address;

    uint64_t channel_no;
    uint64_t chips_per_channel;
    uint64_t dies_per_chip;
    uint64_t planes_per_die;
    uint64_t blocks_per_plane;
    uint64_t pages_per_block;
    uint64_t sectors_per_page;
    uint64_t page_size_in_bytes;

    uint64_t total_physical_pages_no;
    uint64_t total_logical_pages_no;

    uint64_t pages_per_channel;
    uint64_t pages_per_chip;
    uint64_t pages_per_die;
    uint64_t pages_per_plane;

    double overprovisioning_ratio;
};