#include "nand_chip.h"
#include "transaction.h"
#include <cstring>
#include <cstdio>

NandChip::NandChip(uint64_t channel_id, uint64_t chip_id, uint64_t dies_per_chip, uint64_t planes_per_die,
                   uint64_t blocks_per_plane, uint64_t pages_per_block, uint64_t page_size)
    : channel_id(channel_id), chip_id(chip_id), dies_per_chip(dies_per_chip), planes_per_die(planes_per_die),
      blocks_per_plane(blocks_per_plane), pages_per_block(pages_per_block), page_size(page_size)
{
    dies.resize(dies_per_chip);
    for (uint64_t i = 0; i < dies_per_chip; ++i)
    {
        dies[i].plane_no = planes_per_die;
        dies[i].status = Die::DieStatus::IDLE;
        dies[i].planes.reserve(planes_per_die);
        for (uint64_t j = 0; j < planes_per_die; ++j)
        {
            dies[i].planes.emplace_back(blocks_per_plane, pages_per_block, page_size);
        }
    }

    stop_flag = false;
    worker = std::thread(&NandChip::worker_loop, this);
}

NandChip::~NandChip()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop_flag = true;
    }
    cv.notify_all();
    if (worker.joinable())
        worker.join();
}

std::vector<uint8_t> NandChip::GetMetaData(uint64_t die, uint64_t plane, uint64_t block, uint64_t page)
{
    if (die >= dies.size() || plane >= dies[die].planes.size() ||
        block >= dies[die].planes[plane].blocks.size() ||
        page >= dies[die].planes[plane].pages_per_block)
    {
        return std::vector<uint8_t>(); // 返回空向量表示错误
    }

    // 返回元数据，这里可以根据需要自定义
    std::vector<uint8_t> metadata(16, 0x00); // 假设元数据长度为16字节
    return metadata;
}

std::future<NandResult> NandChip::push_command(NandCmd cmd, const PhysicalPageAddressPtr addr, const std::vector<uint8_t> &data)
{
    auto promise = std::make_shared<std::promise<NandResult>>();
    std::future<NandResult> fut = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(mtx);
        command_queue.push(NandTask{cmd, addr, data, promise});
    }
    cv.notify_one();
    return fut;
}

int NandChip::erase_block(const PhysicalPageAddressPtr addr)
{
    if (!addr)
        return -1;
    if (addr->die_id >= dies.size() || addr->plane_id >= dies[addr->die_id].planes.size() ||
        addr->block_id >= dies[addr->die_id].planes[addr->plane_id].blocks.size())
    {
        return -1;
    }

    // 擦除整个块，将所有页重置为0xFF
    Block &block = dies[addr->die_id].planes[addr->plane_id].blocks[addr->block_id];
    for (auto &page : block.pages)
    {
        std::fill(page.data.begin(), page.data.end(), 0xFF);
    }
    return 0;
}

int NandChip::write_page(const PhysicalPageAddressPtr addr, const uint8_t *data)
{
    if (!addr || !data)
        return -1;
    if (addr->die_id >= dies.size() || addr->plane_id >= dies[addr->die_id].planes.size() ||
        addr->block_id >= dies[addr->die_id].planes[addr->plane_id].blocks.size() ||
        addr->page_id >= dies[addr->die_id].planes[addr->plane_id].blocks[addr->block_id].pages.size())
    {
        return -1;
    }

    Page &page = dies[addr->die_id].planes[addr->plane_id].blocks[addr->block_id].pages[addr->page_id];
    std::memcpy(page.data.data(), data, page.data.size());
    return 0;
}

int NandChip::read_page(const PhysicalPageAddressPtr addr, uint8_t *data)
{
    if (!addr || !data)
        return -1;
    if (addr->die_id >= dies.size() || addr->plane_id >= dies[addr->die_id].planes.size() ||
        addr->block_id >= dies[addr->die_id].planes[addr->plane_id].blocks.size() ||
        addr->page_id >= dies[addr->die_id].planes[addr->plane_id].blocks[addr->block_id].pages.size())
    {
        return -1;
    }

    const Page &page = dies[addr->die_id].planes[addr->plane_id].blocks[addr->block_id].pages[addr->page_id];
    std::memcpy(data, page.data.data(), page.data.size());
    return 0;
}

void NandChip::worker_loop()
{
    while (!stop_flag)
    {
        NandTask task;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]
                    { return stop_flag || !command_queue.empty(); });
            if (stop_flag && command_queue.empty())
                return;
            task = std::move(command_queue.front());
            command_queue.pop();
        }
        NandResult result;
        result.cmd = task.cmd;
        switch (task.cmd)
        {
        case NandCmd::READ:
        {
            std::vector<uint8_t> buf(page_size, 0xFF);
            int status = read_page(task.addr, buf.data());
            result.status = status;
            result.data = std::move(buf);
            break;
        }
        case NandCmd::PROGRAM:
        {
            int status = write_page(task.addr, task.data.data());
            result.status = status;
            break;
        }
        case NandCmd::ERASE:
        {
            int status = erase_block(task.addr);
            result.status = status;
            break;
        }
        default:
            result.status = -1;
        }
        task.promise->set_value(std::move(result));
    }
    std::cout << "NandChip worker thread exiting." << std::endl;
}
