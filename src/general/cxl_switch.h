#ifndef VANS_CXL_SWITCH_H
#define VANS_CXL_SWITCH_H

#include "component.h"     // For vans::base_component
#include "request_queue.h" // For vans::base_request_queue
#include "config.h"        // For vans::config
#include "common.h"        // For clk_t and base_request

#include <string>
#include <vector>
#include <memory>    // For std::shared_ptr
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cerr

namespace vans
{

class CXLSwitch : public base_component
{
  public:
    // Constructor will now take name and config
    CXLSwitch(const std::string &name, const config &cfg);
    ~CXLSwitch() override = default;

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
    std::string name_;       // Store name as it's not in base_component
    const config &cfg_;      // Store config as it's not in base_component

    clk_t latency_;                // Latency introduced by the CXL switch
    double bandwidth_gb_s_;         // Bandwidth of the CXL link in GB/s
    clk_t transfer_time_per_byte_; // Pre-calculated transfer time per byte based on bandwidth
    base_request_queue request_queue_;    // Queue for incoming requests (using base_request_queue)

    // Helper to calculate transfer time for a request based on bandwidth
    clk_t calculate_transfer_time(base_request &req) const;
};

} // namespace vans

#endif // VANS_CXL_SWITCH_H
