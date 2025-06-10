#include "cxl_mem_device.h"
#include "factory.h" // For REGISTER_COMPONENT macro
#include "ddr4_spec.h" // Include for ddr4_spec if it's used as backend type
#include <iostream>
#include <stdexcept>

namespace vans
{

CXLMemDevice::CXLMemDevice(const std::string &name, const config &cfg) :
    base_component(name, cfg), // Call base_component constructor
    name_(name),               // Initialize name_ explicitly
    cfg_(cfg),                 // Initialize cfg_ explicitly
    cxl_mem_access_latency_(0),
    read_latency_(0),
    write_latency_(0),
    size_gb_(0),
    read_queue_(cfg_.get_ulong(name_ + ".read_queue_size", 32)),  // Default queue sizes
    write_queue_(cfg_.get_ulong(name_ + ".write_queue_size", 32))
{
    // Parse configuration parameters
    cxl_mem_access_latency_ = cfg_.get_ulong(name_ + ".cxl_mem_access_latency", 10); // Default CXL.mem latency 10ns
    read_latency_ = cfg_.get_ulong(name_ + ".read_latency", 120); // Default 120ns (backend DRAM read latency)
    write_latency_ = cfg_.get_ulong(name_ + ".write_latency", 100); // Default 100ns (backend DRAM write latency)
    size_gb_ = cfg_.get_ulong(name_ + ".size_gb", 128); // Default 128GB

    // Initialize the dram_memory backend.
    try {
        memory_backend_ = std::make_shared<dram::dram_memory<dram::ddr4_spec>>(name_ + ".backend", cfg_);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: CXLMemDevice '" << name_ << "' failed to create DRAMMemory backend: " << e.what() << std::endl;
        throw; // Re-throw to indicate critical construction failure
    }
}

CXLMemDevice::~CXLMemDevice()
{
    // shared_ptr handles deletion automatically if no other references exist.
}

void CXLMemDevice::tick_current(clk_t curr_clk)
{
    // Process read requests
    if (!read_queue_.empty()) {
        base_request &req = read_queue_.queue.front();

        // If the request is ready to be sent to the backend
        if (req.arrive <= curr_clk) {
            read_queue_.queue.pop_front();
            base_response resp = memory_backend_->issue_request(req); // Forward to backend

            if (!std::get<0>(resp)) { // Check the issued status (first element of tuple)
                // Backend NACKed, re-enqueue
                std::cerr << "WARN: CXLMemDevice '" << name_ << "' backend NACKed read request 0x"
                          << std::hex << req.addr << std::dec << ". Re-enqueuing.\n";
                read_queue_.enqueue(req);
            }
        }
    }

    // Process write requests
    if (!write_queue_.empty()) {
        base_request &req = write_queue_.queue.front();

        // If the request is ready to be sent to the backend
        if (req.arrive <= curr_clk) {
            write_queue_.queue.pop_front();
            base_response resp = memory_backend_->issue_request(req); // Forward to backend

            if (!std::get<0>(resp)) { // Check the issued status (first element of tuple)
                // Backend NACKed, re-enqueue
                std::cerr << "WARN: CXLMemDevice '" << name_ << "' backend NACKed write request 0x"
                          << std::hex << req.addr << std::dec << ". Re-enqueuing.\n";
                write_queue_.enqueue(req);
            }
        }
    }

    // Tick the internal memory backend
    if (memory_backend_) {
        memory_backend_->tick_current(curr_clk); // Tick the backend explicitly
    }
}

base_response CXLMemDevice::issue_request(base_request &req)
{
    // First, add the CXL.mem protocol access latency
    req.arrive = req.arrive + cxl_mem_access_latency_;

    // Then, add latency specific to the read/write operation on the backend memory
    if (req.type == base_request_type::read || req.type == base_request_type::cxl_mem_read) {
        req.arrive = req.arrive + read_latency_;
    } else if (req.type == base_request_type::write || req.type == base_request_type::cxl_mem_write) {
        req.arrive = req.arrive + write_latency_;
    } else {
        std::cerr << "ERROR: CXLMemDevice '" << name_ << "' received unknown/unsupported request type for request 0x"
                  << std::hex << req.addr << std::dec << std::endl;
        return {false, true, req.arrive}; // NACK response
    }

    // Determine which queue to use based on request type
    base_request_queue& queue = get_queue_for_request(req);

    // Enqueue the request
    if (!queue.enqueue(req)) {
        std::cerr << "WARN: CXLMemDevice '" << name_ << "' request queue is full. Request 0x"
                  << std::hex << req.addr << std::dec << " dropped or stalled.\n";
        return {false, true, req.arrive}; // NACK response for full queue
    }
    return {true, true, req.arrive}; // ACK response
}

void CXLMemDevice::connect_next(const std::shared_ptr<base_component> &nc)
{
    // CXLMemDevice is a leaf node in the VANS hierarchy from the perspective of the main model.
    // It should not connect to other components directly via 'next'.
    // Its internal 'memory_backend_' manages its own downstream.
    std::cerr << "WARN: CXLMemDevice '" << name_ << "' does not typically connect to 'next' components. Ignoring.\n";
}

void CXLMemDevice::connect_dumper(std::shared_ptr<dumper> dumper)
{
    this->stat_dumper = dumper;
    // Also connect dumper to the internal memory backend if it supports it
    if (memory_backend_) {
        memory_backend_->connect_dumper(dumper);
    }
}

void CXLMemDevice::print_counters()
{
    // Print CXLMemDevice specific counters if any are added.
    // Also, print counters for the internal memory backend.
    if (memory_backend_) {
        memory_backend_->print_counters();
    }
}

bool CXLMemDevice::full()
{
    // Consider both queues and backend fullness
    return read_queue_.full() || write_queue_.full() || (memory_backend_ ? memory_backend_->full() : false);
}

bool CXLMemDevice::pending()
{
    // Consider both queues and backend pending status
    return read_queue_.pending() || write_queue_.pending() || (memory_backend_ ? memory_backend_->pending() : false);
}

void CXLMemDevice::drain()
{
    read_queue_.drain();
    write_queue_.drain();
    if (memory_backend_) {
        memory_backend_->drain();
    }
}

base_request_queue& CXLMemDevice::get_queue_for_request(base_request &req)
{
    if (req.type == base_request_type::read || req.type == base_request_type::cxl_mem_read) {
        return read_queue_;
    } else if (req.type == base_request_type::write || req.type == base_request_type::cxl_mem_write) {
        return write_queue_;
    } else {
        std::cerr << "ERROR: Unsupported request type for CXLMemDevice '" << name_ << "'. Throwing error.\n";
        throw std::runtime_error("Unsupported request type in CXLMemDevice."); // Critical error
    }
}

uint64_t CXLMemDevice::calculate_physical_address(uint64_t address) const {
    // This method would implement the mapping from logical address to physical address within the CXL memory device.
    // For now, a simple pass-through or basic offset might be sufficient.
    // More complex CXL interleaving or address translation would go here.
    return address;
}

// Register the CXLMemDevice with the factory
REGISTER_COMPONENT(CXLMemDevice, "cxl_mem_device");

} // namespace vans
