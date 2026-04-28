#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include "pagetable.h"

PageTable::PageTable(int page_size)
{
    _page_size = page_size;
    _total_frames = 67108864 / _page_size; // 64 MB / page_size
    _used_frames.resize(_total_frames, false);
}

PageTable::~PageTable()
{
}

std::vector<std::string> PageTable::sortedKeys()
{
    std::vector<std::string> keys;

    std::map<std::string, int>::iterator it;
    for (it = _table.begin(); it != _table.end(); it++)
    {
        keys.push_back(it->first);
    }

    std::sort(keys.begin(), keys.end(), PageTableKeyComparator());

    return keys;
}

void PageTable::addEntry(uint32_t pid, int page_number)
{
    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    
    // If it already exists, do nothing
    if (_table.count(entry) > 0) return;

    int frame = -1; 
    // Find free frame (First-Fit)
    for (int i = 0; i < _total_frames; i++) {
        if (!_used_frames[i]) {
            frame = i;
            break;
        }
    }

    // Map entry to frame if space is available; otherwise, report error
    if (frame != -1) {
        _table[entry] = frame;
        _used_frames[frame] = true;
    } else {
        std::cout << "error: out of physical memory\n";
    }
}

void PageTable::removeEntry(uint32_t pid, int page_number)
{
    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);

    // Free the frame if the entry exists
    if (_table.count(entry) > 0) {
        _used_frames[_table[entry]] = false;
        _table.erase(entry);
    }
}

void PageTable::removeAllEntries(uint32_t pid)
{
    // Iterate through table to remove all pages belonging to a specific PID
    std::string prefix = std::to_string(pid) + "|";
    auto it = _table.begin();
    while (it != _table.end()) {
        if (it->first.substr(0, prefix.size()) == prefix) {
            _used_frames[it->second] = false;
            it = _table.erase(it);
        } else {
            ++it;
        }
    }
}

int PageTable::getPhysicalAddress(uint32_t pid, uint32_t virtual_address)
{
    // Convert virtual address to page_number and page_offset
    int page_number = virtual_address / _page_size;
    int page_offset = virtual_address % _page_size;

    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    
    // If entry exists, look up frame number and convert virtual to physical address
    int address = -1;
    if (_table.count(entry) > 0)
    {
        int frame_number = _table[entry];
        address = (frame_number * _page_size) + page_offset;
    }

    return address;
}

void PageTable::print()
{
    int i;

    std::cout << " PID  | Page Number | Frame Number" << std::endl;
    std::cout << "------+-------------+--------------" << std::endl;

    std::vector<std::string> keys = sortedKeys();

    for (i = 0; i < keys.size(); i++)
    {
        std::string key = keys[i];
        size_t sep_pos = key.find("|");
        std::string pid_str = key.substr(0, sep_pos);
        std::string page_str = key.substr(sep_pos + 1);
        
        int frame = _table[key];

        std::cout << " " << std::setw(4) << pid_str 
                  << " | " << std::setw(11) << page_str 
                  << " | " << std::setw(12) << frame << std::endl;
    }
}