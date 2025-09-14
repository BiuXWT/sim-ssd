#pragma once
#include "param.h"

class FTL
{
public:
    void ProcessUserRequest(UserRequestPtr req);
    TransactionPtr CreateTransactionFromUserRequest(UserRequestPtr req);
};