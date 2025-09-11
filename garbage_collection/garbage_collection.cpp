#include "garbage_collection.h"

int GarbageCollection::GetRandomBlockId()
{
    return dist(rng);
}