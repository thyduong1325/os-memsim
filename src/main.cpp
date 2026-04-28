#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include "mmu.h"
#include "pagetable.h"

// 64 MB (64 * 1024 * 1024)
#define PHYSICAL_MEMORY 67108864

void printStartMessage(int page_size);
void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table);
void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table, int page_size);
void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, uint8_t *memory);
void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table);
void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table);
uint32_t getDataTypeSize(DataType type);
DataType stringToDataType(const std::string& type_str);

int main(int argc, char **argv)
{
    // Ensure user specified page size as a command line parameter
    if (argc < 2)
    {
        std::cerr << "error: you must specify the page size" << std:: endl;
        return 1;
    }

    // Print opening instuction message
    int page_size = std::stoi(argv[1]);
    if (page_size < 1024 || page_size > 32768 || (page_size & (page_size - 1)) != 0) 
    {
        std::cout << "error: page size must be a power of 2 between 1024 and 32768" << std:: endl;
        return 1;
    }

    printStartMessage(page_size);

    // Create physical 'memory' (raw array of bytes)
    uint8_t *memory = new uint8_t[PHYSICAL_MEMORY];

    // Create MMU and Page Table
    Mmu *mmu = new Mmu(PHYSICAL_MEMORY);
    PageTable *page_table = new PageTable(page_size);

    // Prompt loop
    std::string command_line;
    while (true)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, command_line)) break;
        if (command_line.empty()) continue;

        std::istringstream iss(command_line);
        std::string command;
        iss >> command;

        if (command == "exit") break;

        if (command == "create") 
        {
            uint32_t t_size, d_size;
            if (iss >> t_size >> d_size) 
            {
                createProcess(t_size, d_size, mmu, page_table);
            }
        } 
        else if (command == "allocate") 
        {
            uint32_t pid, num_elements;
            std::string var_name, type_str;
            if (iss >> pid >> var_name >> type_str >> num_elements) 
            {
                try {
                    DataType type = stringToDataType(type_str);
                    allocateVariable(pid, var_name, type, num_elements, mmu, page_table, page_size);
                } 
                catch (...) 
                {
                    std::cout << "error: invalid data type" << std:: endl;
                }
            }
        } 
        else if (command == "set") 
        {
            uint32_t pid, offset;
            std::string var_name;
            if (iss >> pid >> var_name >> offset) 
            {
                Process* p = mmu->getProcess(pid);
                if (!p) 
                { 
                    std::cout << "error: process not found" << std:: endl; 
                    continue; 
                }
                
                Variable* target_var = nullptr;
                for (auto* v : p->variables) 
                {
                    if (v->name == var_name) 
                    { 
                        target_var = v; 
                        break; 
                    }
                }
                
                if (!target_var) 
                { 
                    std::cout << "error: variable not found" << std:: endl; 
                    continue; 
                }

                std::string val_str;
                int current_offset = offset;
                while (iss >> val_str) {
                    // Extract values and cast based on type
                    if (target_var->type == DataType::Int) {
                        int val = std::stoi(val_str);
                        setVariable(pid, var_name, current_offset, &val, mmu, page_table, memory);
                    } else if (target_var->type == DataType::Double) {
                        double val = std::stod(val_str);
                        setVariable(pid, var_name, current_offset, &val, mmu, page_table, memory);
                    } else if (target_var->type == DataType::Char) {
                        char val = val_str[0];
                        setVariable(pid, var_name, current_offset, &val, mmu, page_table, memory);
                    } else if (target_var->type == DataType::Long) {
                        long val = std::stoll(val_str);
                        setVariable(pid, var_name, current_offset, &val, mmu, page_table, memory);
                    }
                    current_offset++;
                }
            }
        } 
        else if (command == "free") 
        {
            uint32_t pid; std::string var_name;
            if (iss >> pid >> var_name) 
            {
                freeVariable(pid, var_name, mmu, page_table);
            }
        } 
        else if (command == "terminate") 
        {
            uint32_t pid;
            if (iss >> pid) terminateProcess(pid, mmu, page_table);
        } 
        else if (command == "print") 
        {
            std::string obj;
            if (iss >> obj) {
                if (obj == "mmu") mmu->print();
                else if (obj == "page") page_table->print();
                else if (obj == "processes") 
                {
                    for (auto* p : mmu->getProcesses()) std::cout << p->pid << std:: endl;
                } 
                else 
                {
                    // Handle variable printing: <PID>:<var_name>
                    size_t colon = obj.find(':');
                    if (colon != std::string::npos) 
                    {
                        uint32_t pid = std::stoi(obj.substr(0, colon));
                        std::string vname = obj.substr(colon + 1);
                        
                        Process* p = mmu->getProcess(pid);
                        if (p) 
                        {
                            Variable* target = nullptr;
                            for (auto* v : p->variables) 
                            {
                                if (v->name == vname) 
                                {
                                    target = v;
                                    break;
                                }
                            }
                            
                            if (target) 
                            {
                                uint32_t element_size = getDataTypeSize(target->type);
                                uint32_t num_elements = target->size / element_size;
                                uint32_t print_limit = std::min((uint32_t)4, num_elements);

                                for (uint32_t i = 0; i < print_limit; i++) 
                                {
                                    if (i > 0) std::cout << ", ";

                                    // Calculate virtual address for this specific element
                                    uint32_t v_addr = target->virtual_address + (i * element_size);
                                    
                                    // Look up physical address in RAM
                                    int p_addr = page_table->getPhysicalAddress(pid, v_addr);
                                    
                                    if (p_addr != -1) 
                                    {
                                        // Extract bytes from physical memory and cast to correct type
                                        if (target->type == DataType::Char) 
                                        {
                                            char val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(char));
                                            std::cout << val;
                                        } 
                                        else if (target->type == DataType::Short) 
                                        {
                                            short val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(short));
                                            std::cout << val;
                                        } 
                                        else if (target->type == DataType::Int) 
                                        {
                                            int val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(int));
                                            std::cout << val;
                                        } 
                                        else if (target->type == DataType::Float) 
                                        {
                                            float val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(float));
                                            std::cout << val;
                                        } 
                                        else if (target->type == DataType::Long) 
                                        {
                                            long val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(long));
                                            std::cout << val;
                                        } 
                                        else if (target->type == DataType::Double) 
                                        {
                                            double val;
                                            std::memcpy(&val, &memory[p_addr], sizeof(double));
                                            std::cout << val;
                                        }
                                    }
                                }

                                // Append the suffix if there are more than 4 items
                                if (num_elements > 4) 
                                {
                                    std::cout << ", ... [" << num_elements << " items]";
                                }
                                std::cout << std:: endl;
                            } 
                            else 
                            {
                                std::cout << "error: variable not found" << std:: endl;
                            }
                        } 
                        else 
                        {
                            std::cout << "error: process not found" << std:: endl;
                        }
                    }
                }
            }
        } 
        else 
        {
            std::cout << "error: command not recognized" << std:: endl;
        }
    }

    delete[] memory;
    delete mmu;
    delete page_table;
    return 0;
}

void printStartMessage(int page_size)
{
    std::cout << "Welcome to the Memory Allocation Simulator! Using a page size of " << page_size << " bytes." << std:: endl;
    std::cout << "Commands:" << std:: endl;
    std::cout << "  * create <text_size> <data_size> (initializes a new process)" << std:: endl;
    std::cout << "  * allocate <PID> <var_name> <data_type> <number_of_elements> (allocated memory on the heap)" << std:: endl;
    std::cout << "  * set <PID> <var_name> <offset> <value_0> <value_1> <value_2> ... <value_N> (set the value for a variable)" << std:: endl;
    std::cout << "  * free <PID> <var_name> (deallocate memory on the heap that is associated with <var_name>)" << std:: endl;
    std::cout << "  * terminate <PID> (kill the specified process)" << std:: endl;
    std::cout << "  * print <object> (prints data)" << std:: endl;
    std::cout << "    * If <object> is \"mmu\", print the MMU memory table" << std:: endl;
    std::cout << "    * if <object> is \"page\", print the page table" << std:: endl;
    std::cout << "    * if <object> is \"processes\", print a list of PIDs for processes that are still running" << std:: endl;
    std::cout << "    * if <object> is a \"<PID>:<var_name>\", print the value of the variable for that process" << std:: endl;
    std::cout << std::endl;
}

void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table)
{
    // Create new process in the MMU
    uint32_t pid = mmu->createProcess();
    
    // Allocate required starting variables by shrinking the initial 64MB FreeSpace
    Process* p = mmu->getProcess(pid);
    Variable* free_space = p->variables[0]; // The 64MB block
    
    uint32_t current_addr = 0;
    
    mmu->addVariableToProcess(pid, "<TEXT>", DataType::Char, text_size, current_addr);
    current_addr += text_size;
    
    mmu->addVariableToProcess(pid, "<GLOBALS>", DataType::Char, data_size, current_addr);
    current_addr += data_size;
    
    mmu->addVariableToProcess(pid, "<STACK>", DataType::Char, 65536, current_addr);
    current_addr += 65536;
    
    // Shrink the free space
    free_space->virtual_address = current_addr;
    free_space->size -= current_addr;
    
    // Map physical frames for the base variables
    int page_size = page_table->getPageSize();
    
    // Calculate how many pages the base process (Text + Globals + Stack) takes up
    int num_base_pages = (current_addr + page_size - 1) / page_size;
    
    // Add all these initial pages to the page table (Pages 0 up to num_base_pages)
    for (int i = 0; i < num_base_pages; i++) 
    {
        page_table->addEntry(pid, i);
    }
    
    // Print pid
    std::cout << pid << std:: endl;
}

void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table, int page_size)
{
    Process* p = mmu->getProcess(pid);
    if (!p) 
    { 
        std::cout << "error: process not found" << std:: endl;
        return; 
    }
    
    for (auto* v : p->variables) 
    {
        if (v->name == var_name) 
        { 
            std::cout << "error: variable already exists" << std:: endl;
            return;
        }
    }
    
    uint32_t req_size = getDataTypeSize(type) * num_elements;
    
    // First Fit using the FreeSpace block
    for (auto* v : p->variables) 
    {
        if (v->type == DataType::FreeSpace && v->size >= req_size) 
        {
            uint32_t alloc_addr = v->virtual_address;
            
            // Register pages needed
            uint32_t start_page = alloc_addr / page_size;
            uint32_t end_page = (alloc_addr + req_size - 1) / page_size;
            for (uint32_t i = start_page; i <= end_page; i++) 
            {
                page_table->addEntry(pid, i);
            }
            
            // Shrink FreeSpace
            v->virtual_address += req_size;
            v->size -= req_size;

            // Insert variable into MMU
            mmu->addVariableToProcess(pid, var_name, type, req_size, alloc_addr);

            // Print virtual memory address
            std::cout << alloc_addr << std:: endl;
            return;
        }
    }
    std::cout << "error: out of memory" << std:: endl;
}

void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, uint8_t *memory)
{
    Process* p = mmu->getProcess(pid);
    if (!p) return;
    
    Variable* target = nullptr;
    for (auto* v : p->variables) 
    {
        if (v->name == var_name) target = v;
    }
    if (!target) return;
    
    uint32_t element_size = getDataTypeSize(target->type);
    uint32_t byte_offset = offset * element_size;
    
    if (byte_offset >= target->size) 
    {
        std::cout << "error: index out of range" << std:: endl;
        return;
    }
    
    uint32_t virt_addr = target->virtual_address + byte_offset;
    int phys_addr = page_table->getPhysicalAddress(pid, virt_addr);
    
    if (phys_addr != -1) 
    {
        std::memcpy(&memory[phys_addr], value, element_size);
    }
}

void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table)
{
    Process* p = mmu->getProcess(pid);
    if (!p) 
    { 
        std::cout << "error: process not found" << std:: endl; 
        return; 
    }
    
    for (auto it = p->variables.begin(); it != p->variables.end(); ++it) 
    {
        if ((*it)->name == var_name) 
        {
            // Mark the current variable as free space
            (*it)->name = "<FREE_SPACE>";
            (*it)->type = DataType::FreeSpace;

            // Merge with the next block if it is also FreeSpace
            if (it + 1 != p->variables.end() && (*(it + 1))->type == DataType::FreeSpace) 
            {
                (*it)->size += (*(it + 1))->size; // Absorb the size
                delete *(it + 1);                 // Free the allocated Variable object
                p->variables.erase(it + 1);       // Remove it from the vector
            }

            // Merge with the previous block if it is also FreeSpace
            if (it != p->variables.begin() && (*(it - 1))->type == DataType::FreeSpace) 
            {
                (*(it - 1))->size += (*it)->size; // Previous block absorbs current block's new size
                delete *it;                       // Free the current Variable object
                p->variables.erase(it);           // Remove it from the vector
                --it;                             // Back up iterator to point to the newly merged block
            }

            // Physical page reclaiming
            // Get our newly unified free space block
            Variable* free_block = *it; 
            
            uint32_t page_size = page_table->getPageSize();
            uint32_t start_addr = free_block->virtual_address;
            uint32_t end_addr = start_addr + free_block->size;

            // Find the first page that is completely inside the free space
            uint32_t first_freeable_page = (start_addr + page_size - 1) / page_size;
            
            // Find the last page that is completely inside the free space
            int last_freeable_page = (end_addr / page_size) - 1; 

            // Free those pages from the physical RAM
            for (int page_num = first_freeable_page; page_num <= last_freeable_page; ++page_num) 
            {
                page_table->removeEntry(pid, page_num);
            }

            return;
        }
    }
    
    std::cout << "error: variable not found" << std:: endl;
}

void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table)
{
    if (!mmu->getProcess(pid)) 
    { 
        std::cout << "error: process not found" << std:: endl; 
        return; 
    }
    page_table->removeAllEntries(pid);
    mmu->removeProcess(pid);
}


uint32_t getDataTypeSize(DataType type) 
{
    switch (type) 
    {
        case DataType::Char: return 1;
        case DataType::Short: return 2;
        case DataType::Int: case DataType::Float: return 4;
        case DataType::Long: case DataType::Double: return 8;
        default: return 0;
    }
}

DataType stringToDataType(const std::string& type_str) 
{
    if (type_str == "char") return DataType::Char;
    if (type_str == "short") return DataType::Short;
    if (type_str == "int") return DataType::Int;
    if (type_str == "float") return DataType::Float;
    if (type_str == "long") return DataType::Long;
    if (type_str == "double") return DataType::Double;
    throw std::invalid_argument("Invalid data type");
}