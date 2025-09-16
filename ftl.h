#pragma once
#include "param.h"

class FTL
{
public:
    void ProcessUserRequest(UserRequestPtr req);
    TransactionPtr CreateTransactionFromUserRequest(UserRequestPtr req);

    AddressMappingPageLevelPtr address_mapping;
    BlockManagerPtr block_manager;
    NandDriverPtr nand_driver;
    GcWlUnitPtr gcwl_unit;
    CacheManagerPtr cache_manager;
};