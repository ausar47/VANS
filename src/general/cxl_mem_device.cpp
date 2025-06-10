#include "cxl_mem_device.h"
#include "factory.h"
#include "trace.h" // For logging

namespace vans
{

CXLMemDevice::CXLMemDevice(const std::string &name, Config *config) :
    Component(name, config),
    read_latency_(0),
    write_latency_(0),
    size_gb_(0),
    memory_backend_(nullptr),
    read_queue_(config->GetInt(name + ".read_queue_size", 32)),  // Default queue sizes
    write_queue_(config->GetInt(name + ".write_queue_size", 32))
{
    // Parse configuration parameters
    read_latency_ = static_cast<Tick_t>(config->GetInt(name + ".read_latency", 120)); // Default 120ns
    write_latency_ = static_cast<Tick_t>(config->GetInt(name + ".write_latency", 100)); // Default 100ns
    size_gb_ = static_cast<uint64_t>(config->GetInt(name + ".size_gb", 128)); // Default 128GB

    // Create the internal memory backend (e.g., DRAMMemory or StaticMemory)
    // The type of backend can be configured in vans.cfg if needed.
    // For simplicity, we'll instantiate a basic DRAMMemory as the backing store.
    // This DRAMMemory doesn't represent physical DRAM, but rather the internal
    // VANS model for managing memory requests (e.g., address mapping, hit/miss).
    // The CXLMemDevice itself applies the CXL-specific latencies.
    memory_backend_ = new DRAMMemory(name + ".backend", config); // Pass config to backend as well

    // The backend memory's size needs to be configured.
    // We assume the size_gb_ parameter from CXLMemDevice config applies to the backend.
    // This might require adding a mechanism for the CXLMemDevice to set the backend's size.
    // For now, let's assume DRAMMemory can handle a large address space.
    // A more robust solution would be to pass the size_gb_ to the backend config explicitly.
}

CXLMemDevice::~CXLMemDevice()
{
    delete memory_backend_;
}

void CXLMemDevice::Setup(Component *parent, std::vector<Component *> &children)
{
    parent_ = parent;
    if (!children.empty()) {
        ERROR("CXLMemDevice '%s' does not expect child components.", name_.c_str());
    }
    // Setup the internal memory backend as well, potentially without children
    memory_backend_->Setup(this, {}); // This component is the parent of the backend
}

void CXLMemDevice::Tick()
{
    // Process read queue
    if (!read_queue_.empty()) {
        Request *req = read_queue_.front();
        if (req->arrive_time_ <= GetGlobalTick()) {
            read_queue_.pop_front();
            // Simulate memory access on backend (this is where actual data access happens)
            memory_backend_->ReceiveRequest(req); // Forward to backend
        }
    }

    // Process write queue
    if (!write_queue_.empty()) {
        Request *req = write_queue_.front();
        if (req->arrive_time_ <= GetGlobalTick()) {
            write_queue_.pop_front();
            // Simulate memory access on backend
            memory_backend_->ReceiveRequest(req); // Forward to backend
        }
    }

    // Allow the internal memory backend to tick
    memory_backend_->Tick();
}

void CXLMemDevice::ReceiveRequest(Request *req)
{
    // Apply CXL memory latency based on request type
    if (req->type_ == Request::Type::READ) {
        req->arrive_time_ = GetGlobalTick() + read_latency_;
        if (!read_queue_.Push(req)) {
            ERROR("CXLMemDevice '%s' read queue is full. Request dropped or stalled.", name_.c_str());
        }
    } else if (req->type_ == Request::Type::WRITE) {
        req->arrive_time_ = GetGlobalTick() + write_latency_;
        if (!write_queue_.Push(req)) {
            ERROR("CXLMemDevice '%s' write queue is full. Request dropped or stalled.", name_.c_str());
        }
    } else if (req->type_ == Request::Type::RETURN) {
        // This is a return from the internal memory backend
        req->finish_time_ = GetGlobalTick();
        parent_->ReceiveRequest(req); // Send back to the parent (CXLSwitch)
    } else {
        ERROR("CXLMemDevice '%s' received unsupported request type.", name_.c_str());
    }

    // For a real CXL memory, you might also consider
    // - address translation/mapping if CXL memory is part of a larger address space.
    // - handling different CXL memory types (e.g., Type 2 or Type 3).
    // - more complex queuing/scheduling within the CXL memory.
}

uint64_t CXLMemDevice::CalculatePhysicalAddress(uint64_t address) const {
    // This is a placeholder for actual address mapping if needed.
    // For now, assuming a direct mapping within the CXL memory device's address range.
    // This would be crucial if we want to model interleaving or multiple CXL devices.
    return address;
}

// Register the CXLMemDevice with the factory
REGISTER_COMPONENT(CXLMemDevice, "cxl_mem_device");

} // namespace vans