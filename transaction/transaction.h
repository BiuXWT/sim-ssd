#pragma once
#include "param.h"

class PhysicalPageAddress
{
public:
    uint64_t channel_id;
    uint64_t chip_id;
    uint64_t die_id;
    uint64_t plane_id;
    uint64_t block_id;
    uint64_t page_id;
    PhysicalPageAddress(const uint64_t channel_id = 0, const uint64_t chip_id = 0, const uint64_t die_id = 0,
                        const uint64_t plane_id = 0, const uint64_t block_id = 0, const uint64_t page_id = 0)
        : channel_id(channel_id), chip_id(chip_id), die_id(die_id), plane_id(plane_id), block_id(block_id), page_id(page_id) {}
    PhysicalPageAddress(const PhysicalPageAddress &other) = default;
};

class Transaction
{
public:
    Transaction(/*TODO... */) {}

    uint64_t transaction_id;
    uint64_t stream_id;
    TransactionSourceType source;
    TransactionType type;
    Priority priority;
    PhysicalPageAddressPtr physical_address;
    bool physical_address_determined; // 物理地址是否已确定
    UserRequestType req_type;
    uint64_t lpa;             // 事务的起始逻辑块地址
    uint64_t ppa;             // 事务的起始物理页地址
    uint64_t size_in_bytes;   // 事务的大小，单位为字节
    uint64_t size_in_sectors; // 事务的大小，单位为扇区
};
class TransactionRead : public Transaction
{
public:
    TransactionRead() {}
    std::vector<uint8_t> content;
    TransactionWritePtr related_write;
    uint64_t read_sectors_bitmap;
    uint64_t timestamp;
};

enum class WriteExecutionModeType
{
    SIMPLE,
    COPYBACK
};
class TransactionWrite : public Transaction
{
public:
    TransactionWrite() {}
    std::vector<uint8_t> content;
    TransactionReadPtr related_read;
    TransactionErasePtr related_erase;
    uint64_t write_sectors_bitmap;
    uint64_t timestamp;
    WriteExecutionModeType execution_mode;
};

class TransactionErase : public Transaction
{
public:
    TransactionErase() {}
    std::vector<TransactionWritePtr> page_movement_actions;
};