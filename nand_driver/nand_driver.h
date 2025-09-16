#pragma once
#include "param.h"
#include "nand_chip.h"
#include "transaction.h"

class NandDriver;
using NandDriverPtr = std::shared_ptr<NandDriver>;

class NandDriver
{

public:
    uint64_t GetLPA(PhysicalPageAddressPtr addr) { return addr->page_id; /* TODO... */ };

private:
    std::vector<std::vector<NandChipPtr>> nand_chips; // [channel][chip_per_channel]
};
