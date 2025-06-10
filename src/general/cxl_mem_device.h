#ifndef VANS_CXL_MEM_DEVICE_H
#define VANS_CXL_MEM_DEVICE_H

#include "common.h"     // Must come first for Tick_t, Request, GetGlobalTick
#include "config.h"     // For Config class
#include "component.h"  // For Component base class
#include "request_queue.h" // For RequestQueue class
#include "dram_memory.h" // For DRAMMemory backend
#include "ddr4_spec.h" // For ddr4_spec template argument

#include <string>
#include <vector> // This might be removed or used differently with base_component
#include <memory> // For std::shared_ptr

namespace vans
{

// Forward declaration for dumper as it's used in connect_dumper.
class dumper;

class CXLMemDevice : public base_component
{
  public:
    // Constructor will now take name and config
    CXLMemDevice(const std::string &name, const config &cfg);
    ~CXLMemDevice() override; // Mark as override

    // Override base_component methods
    void tick_current(clk_t curr_clk) override;
    base_response issue_request(base_request &req) override;
    void connect_next(const std::shared_ptr<base_component> &nc) override;
    void connect_dumper(std::shared_ptr<dumper> dumper) override;
    void print_counters() override;
    bool full() override;
    bool pending() override;
    void drain() override;

  private:
    std::string name_;       // Store name
    const config &cfg_;      // Store config

    clk_t cxl_mem_access_latency_; // Latency for CXL.mem protocol access before hitting backend
    clk_t read_latency_;        // Read latency of the internal DRAM backend
    clk_t write_latency_;       // Write latency of the internal DRAM backend
    uint64_t size_gb_;           // Capacity of the CXL memory device in GB

    // Internal VANS memory model (e.g., DRAMMemory)
    std::shared_ptr<dram::dram_memory<dram::ddr4_spec>> memory_backend_;

    base_request_queue read_queue_;    // Queue for read requests
    base_request_queue write_queue_;   // Queue for write requests

    // Helper to determine which queue to push to
    base_request_queue& get_queue_for_request(base_request &req);

    // Helper to calculate physical address (if CXL introduces memory mapping)
    uint64_t calculate_physical_address(uint64_t address) const;
};

} // namespace vans

#endif // VANS_CXL_MEM_DEVICE_H
