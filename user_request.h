#include "param.h"
enum class UserRequestType { READ, WRITE, TRIM };

class UserRequest {
public:
    uint64_t id;
    UserRequestType req_type;
    uint64_t start_lpa;
    uint64_t size_in_sectors;
    uint64_t size_in_bytes;
    uint64_t stream_id;
    std::vector<uint8_t> data;
    uint64_t sectors_from_cache;
};
using UserRequestPtr = std::shared_ptr<UserRequest>;

UserRequestPtr generate_user_requests_ramdomly();