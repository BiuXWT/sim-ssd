#include "garbage_collection.h"

int GarbageCollection::GetRandomBlockId()
{
    return dist(rng);
}

bool GarbageCollection::GcIsUrgentMode(NandChipPtr nand_chip)
{
    if(!preemptible_gc_enabled)
        return true;

    PhysicalPageAddress addr;
    addr.channel_id = nand_chip->GetChannelId();

    return false;
}
