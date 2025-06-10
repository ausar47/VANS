#ifndef VANS_CXL_SWITCH_H
#define VANS_CXL_SWITCH_H

#include "component.h"     // For base_component base class
#include "request_queue.h" // For base_request_queue
#include "config.h"        // For config object
#include "common.h"        // For clk_t and base_request

#include <string>
#include <vector>
#include <memory> // For std::shared_ptr

namespace vans
{

// CXLSwitch will inherit from base_component, as it handles requests
// and forwards them to its children (CXL memory devices).
class CXLSwitch : public base_component
{
  public:
    // Constructor uses vans::config reference
    CXLSwitch(const std::string &name, const config &cfg);
    ~CXLSwitch() override = default; // Use default destructor

    // Override methods from base_component
    void tick_current(clk_t curr_clk) override;
    // tick_next is handled by base_component's default implementation
    void connect_next(const std::shared_ptr<base_component> &nc) override;
    void connect_dumper(std::shared_ptr<dumper> dumper) override;
    void print_counters() override;
    base_response issue_request(base_request &req) override; // Corrected return type and parameter
    bool full() override;
    bool pending() override;
    void drain() override;

  private:
    std::string name_; // To store the component's name for logging
    const config &cfg_; // Reference to the configuration object

    clk_t latency_;                // Latency introduced by the CXL switch
    double bandwidth_gb_s_;         // Bandwidth of the CXL link in GB/s
    clk_t transfer_time_per_byte_; // Pre-calculated transfer time per byte based on bandwidth
    base_request_queue request_queue_; // Queue for incoming requests (using base_request_queue)
    // child_ is already part of base_component's `next` vector.
    // For simplicity, we'll assume a single child for a direct CXL switch for now.

    // Helper to calculate transfer time for a request based on bandwidth
    clk_t calculate_transfer_time(base_request &req) const; // Corrected parameter type and name
};

} // namespace vans

#endif // VANS_CXL_SWITCH_H
