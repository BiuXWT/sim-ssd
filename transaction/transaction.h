#pragma once
#include "param.h"

class Transaction;
class TransactionRead;
class TransactionWrite;
class TransactionErase;

using TransactionPtr = std::shared_ptr<Transaction>;
using TransactionReadPtr = std::shared_ptr<TransactionRead>;
using TransactionWritePtr = std::shared_ptr<TransactionWrite>;
using TransactionErasePtr = std::shared_ptr<TransactionErase>;


class PhysicalPageAddress{
public:
    uint64_t channel_id;
    uint64_t chip_id;
    uint64_t die_id;
    uint64_t plane_id;
    uint64_t block_id;
    uint64_t page_id;
    PhysicalPageAddress(const uint64_t channel_id=0, const uint64_t chip_id=0, const uint64_t die_id=0,
                        const uint64_t plane_id=0, const uint64_t block_id=0, const uint64_t page_id=0)
        : channel_id(channel_id), chip_id(chip_id), die_id(die_id), plane_id(plane_id), block_id(block_id), page_id(page_id) {}
    PhysicalPageAddress(const PhysicalPageAddress& other) = default;
};
class Transaction {
public:
    uint64_t transaction_id;
    uint64_t stream_id;
    TransactionType type;
    TransactionSourceType source;
    Priority priority;
    PhysicalPageAddress physical_address;
    bool physical_address_determined;// 物理地址是否已确定
    UserRequestType req_type;
    uint64_t lpa; // 事务的起始逻辑块地址
    uint64_t ppa; // 事务的起始物理页地址
    uint64_t size_in_bytes; // 事务的大小，单位为字节
    uint64_t size_in_sectors; // 事务的大小，单位为扇区
    std::vector<uint8_t> data;
};
class TransactionRead : public Transaction {
    TransactionWritePtr related_write;
    uint64_t read_sectors_bitmap;
    uint64_t timestamp;
};

enum class WriteExecutionModeType { SIMPLE, COPYBACK };
class TransactionWrite : public Transaction {
    TransactionReadPtr related_read;
    TransactionErasePtr related_erase;
    uint64_t write_sectors_bitmap;
    uint64_t timestamp;
    WriteExecutionModeType execution_mode;
};

class TransactionErase : public Transaction {
    std::vector<TransactionWritePtr> page_movement_actions;
};