#pragma once
#include "param.h"
#include "transaction.h"
#include "user_request.h"
class FTL;
using FTLPtr = std::shared_ptr<FTL>;

class FTL {
public:
    void ProcessUserRequest(UserRequestPtr req);
    TransactionPtr CreateTransactionFromUserRequest(UserRequestPtr req);
};