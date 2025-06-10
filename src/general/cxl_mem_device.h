#ifndef VANS_CXL_MEM_DEVICE_H
#define VANS_CXL_MEM_DEVICE_H

#include "component.h"
#include "config.h"
#include "request_queue.h"
#include "dram_memory.h" // Or static_memory, depending on the desired backing store
#include "common.h"      // For Tick_t

namespace vans
{

class CXLMemDevice : public Component
{
  public:
    CXLMemDevice(const std::string &name, Config *config);
    ~CXLMemDevice();

    void Setup(Component *parent, std::vector<Component *> &children) override;
    void Tick() override;
    void ReceiveRequest(Request *req) override;

  private:
    Tick_t read_latency_;        // Read latency of the CXL memory device
    Tick_t write_latency_;       // Write latency of the CXL memory device
    uint64_t size_gb_;           // Capacity of the CXL memory device in GB
    DRAMMemory *memory_backend_; // Internal VANS memory model (e.g., DRAMMemory)

    RequestQueue read_queue_;  // Queue for read requests
    RequestQueue write_queue_; // Queue for write requests

    // Helper to calculate the physical address within the CXL memory
    uint64_t CalculatePhysicalAddress(uint64_t address) const;
};

} // namespace vans

#endif // VANS_CXL_MEM_DEVICE_H