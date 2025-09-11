#pragma once
#include "param.h"
#include "nand_drive.h"

class NandRuntime{
    std::vector<std::vector<NandDrivePtr>> nand_chips; // [channel][chip_per_channel]
    
};
