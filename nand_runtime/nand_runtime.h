#pragma once
#include "param.h"
#include "nand_drive.h"

class NandRuntime;
using NandRuntimePtr = std::shared_ptr<NandRuntime>;

class NandRuntime{
    std::vector<std::vector<NandDrivePtr>> nand_chips; // [channel][chip_per_channel]
    
};
