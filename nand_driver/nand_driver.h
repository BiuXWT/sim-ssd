#pragma once
#include "param.h"
#include "nand_chip.h"

class NandDriver;
using NandDriverPtr = std::shared_ptr<NandDriver>;

class NandDriver{
    std::vector<std::vector<NandChipPtr>> nand_chips; // [channel][chip_per_channel]

};
