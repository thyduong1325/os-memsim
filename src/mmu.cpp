#include <iostream>
#include <algorithm>
#include <iomanip>
#include "mmu.h"

Mmu::Mmu(int memory_size)
{
    _next_pid = 1024;
    _max_size = memory_size;
}

Mmu::~Mmu()
{
    for (Process* p : _processes) {
        for (Variable* v : p->variables) {
            delete v;
        }
        delete p;
    }
}

uint32_t Mmu::createProcess()
{
    Process *proc = new Process();
    proc->pid = _next_pid;

    Variable *var = new Variable();
    var->name = "<FREE_SPACE>";
    var->type = DataType::FreeSpace;
    var->virtual_address = 0;
    var->size = _max_size;
    proc->variables.push_back(var);

    _processes.push_back(proc);

    _next_pid++;
    
    return proc->pid;
}

void Mmu::addVariableToProcess(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t address)
{
    std::vector<Process*>::iterator it = std::find_if(_processes.begin(), _processes.end(), [pid](Process* p)
    { 
        return p != nullptr && p->pid == pid; 
    });
    
    if (it != _processes.end())
    {
        Process *proc = *it;
        Variable *var = new Variable();
        var->name = var_name;
        var->type = type;
        var->virtual_address = address;
        var->size = size;
        proc->variables.push_back(var);
        
        // Sort variables by virtual address so First-Fit and Printing work flawlessly
        std::sort(proc->variables.begin(), proc->variables.end(), [](Variable* a, Variable* b) {
            return a->virtual_address < b->virtual_address;
        });
    }
}

Process* Mmu::getProcess(uint32_t pid) {
    auto it = std::find_if(_processes.begin(), _processes.end(), [pid](Process* p) { 
        return p != nullptr && p->pid == pid; 
    });
    if (it != _processes.end()) return *it;
    return nullptr;
}

void Mmu::removeProcess(uint32_t pid) {
    auto it = std::find_if(_processes.begin(), _processes.end(), [pid](Process* p) { 
        return p != nullptr && p->pid == pid; 
    });
    if (it != _processes.end()) {
        for (Variable* v : (*it)->variables) {
            delete v;
        }
        delete *it;
        _processes.erase(it);
    }
}

void Mmu::print()
{
    std::cout << " PID  | Variable Name | Virtual Addr | Size" << std::endl;
    std::cout << "------+---------------+--------------+------------" << std::endl;
    for (size_t i = 0; i < _processes.size(); i++)
    {
        for (size_t j = 0; j < _processes[i]->variables.size(); j++)
        {
            Variable* var = _processes[i]->variables[j];
            if (var->type != DataType::FreeSpace) {
                std::cout << " " << std::setw(4) << _processes[i]->pid
                          << " | " << std::left << std::setw(13) << var->name << std::right
                          << " |   0x" << std::hex << std::uppercase << std::setfill('0') 
                          << std::setw(8) << var->virtual_address
                          << std::dec << std::setfill(' ')
                          << " | " << std::setw(10) << var->size << std::endl;
            }
        }
    }
}