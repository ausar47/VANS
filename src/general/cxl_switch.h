#ifndef VANS_CXL_SWITCH_H
#define VANS_CXL_SWITCH_H

#include "component.h"
#include "request_queue.h"
#include "config.h"
#include "tick.h"
#include "common.h" // For Tick_t

namespace vans
{

class CXLSwitch : public Component
{
  public:
    CXLSwitch(const std::string &name, Config *config);
    ~CXLSwitch();

    void Setup(Component *parent, std::vector<Component *> &children) override;
    void Tick() override;
    void ReceiveRequest(Request *req) override;

  private:
    Tick_t latency_;                // Latency introduced by the CXL switch
    double bandwidth_gb_s_;         // Bandwidth of the CXL link in GB/s
    Tick_t transfer_time_per_byte_; // Pre-calculated transfer time per byte based on bandwidth
    RequestQueue request_queue_;    // Queue for incoming requests
    Component *child_;              // The component connected to the CXL switch (e.g., CXL memory)

    // Helper to calculate transfer time for a request based on bandwidth
    Tick_t CalculateTransferTime(Request *req) const;
};

} // namespace vans

#endif // VANS_CXL_SWITCH_H