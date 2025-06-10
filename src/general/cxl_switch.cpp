#include "cxl_switch.h"
#include "factory.h" // For REGISTER_COMPONENT macro
#include <iostream>  // For std::cerr
#include <stdexcept> // For std::runtime_error

namespace vans
{

CXLSwitch::CXLSwitch(const std::string &name, const config &cfg) :
    name_(name), // Initialize name_ explicitly
    cfg_(cfg),   // Initialize cfg_ explicitly
    latency_(0),
    bandwidth_gb_s_(0.0),
    transfer_time_per_byte_(0),
    // RequestQueue expects a size, fetched from config
    request_queue_(base_request_queue(cfg_.get_ulong(name_ + ".queue_size", 32))) // Corrected instantiation
{
    // Parse configuration parameters using cfg_
    latency_ = cfg_.get_ulong(name_ + ".latency", 50); // Default 50ns
    // Bandwidth is float, use get_string and convert
    bandwidth_gb_s_ = std::stod(cfg_.get_string(name_ + ".bandwidth_gb_s", "64.0")); // Default 64 GB/s

    if (bandwidth_gb_s_ > 0) {
        // Calculate transfer time per byte in ticks (assuming 1 tick = 1 ns)
        // 1 GB/s = 10^9 Bytes/s
        // (bandwidth_gb_s * (1024^3) Bytes/GB) / (10^9 ns/s) = Bytes/ns
        // 1 / (Bytes/ns) = ns/Byte = ticks/Byte (if 1 tick = 1 ns)
        transfer_time_per_byte_ = static_cast<clk_t>(1.0 / (bandwidth_gb_s_ * (1024.0 * 1024.0 * 1024.0) / 1e9));
    } else {
        transfer_time_per_byte_ = 0; // Infinite bandwidth if 0
    }
}

void CXLSwitch::tick_current(clk_t curr_clk)
{
    // Process requests in the queue
    if (!request_queue_.empty()) {
        base_request &req = request_queue_.queue.front(); // Access deque directly

        // Check if the request is ready to be forwarded
        if (req.arrive <= curr_clk) {
            request_queue_.queue.pop_front();

            // Forward the request to the child component
            if (!next.empty()) { // 'next' is inherited from base_component
                // Assuming only one child for simplicity in this CXL switch model.
                // If there are multiple, routing logic (e.g., based on address) is needed here.
                // The request's arrive time should be updated for the next component.
                req.arrive = curr_clk; // Set arrival to next component as current tick
                base_response resp = next[0]->issue_request(req); // Call issue_request on child

                // Interpret base_response tuple: {issued, deterministic, next_clk}
                if (!std::get<0>(resp)) { // If not issued (NACK)
                    // Child NACKed, re-enqueue or handle stalling
                    std::cerr << "WARN: CXLSwitch '" << name_ << "' child NACKed request 0x"
                              << std::hex << req.addr << std::dec << ". Request stalled or re-enqueued.\n";
                    // For now, re-enqueue to simulate stalling, or implement backpressure.
                    request_queue_.enqueue(req); // This might cause infinite loop if child always NACKs
                } else {
                    // Successfully issued to child, nothing else to do for this tick for this request
                }

            } else {
                // If no child, this is an error or end of trace.
                std::cerr << "ERROR: CXLSwitch '" << name_ << "' has no child. Request 0x"
                          << std::hex << req.addr << std::dec << " departed without further processing.\n";
                req.depart = curr_clk; // Mark as departed from this component
                // In a real VANS setup, completed requests might be returned up the hierarchy
                // For now, we consider it completed here if no child.
            }
        }
    }
}

base_response CXLSwitch::issue_request(base_request &req)
{
    // Requests arriving at the CXLSwitch are processed and enqueued.
    // Their 'arrive' time is adjusted to reflect the time they *will arrive* at the *next* stage after this component's latency.
    req.arrive = req.arrive + latency_; // Add CXL switch latency

    // If bandwidth is limited, calculate additional transfer time
    // We assume 'cpu_cl_size' from common.h as the unit of transfer.
    if (transfer_time_per_byte_ > 0) {
        req.arrive += calculate_transfer_time(req);
    }

    // Enqueue the request
    if (!request_queue_.enqueue(req)) {
        std::cerr << "WARN: CXLSwitch '" << name_ << "' request queue is full. Request 0x"
                  << std::hex << req.addr << std::dec << " dropped or stalled.\n";
        // Return {false, false, clk_invalid} for NACK
        return {false, false, clk_invalid};
    }
    // Return {true, false, clk_invalid} for ACK (not deterministic in this model)
    return {true, false, clk_invalid};
}

void CXLSwitch::connect_next(const std::shared_ptr<base_component> &nc)
{
    // CXL Switch typically connects to one CXL memory device or another switch.
    // Enforce a single child for now, similar to previous design.
    if (!next.empty()) {
        std::cerr << "WARN: CXLSwitch '" << name_ << "' already has a child. Overwriting existing connection.\n";
        // In a more robust system, this might be an error or require specific multi-port logic.
        next.clear(); // Clear existing to allow new connection
    }
    base_component::connect_next(nc); // Use base class method to add to 'next' vector
}

void CXLSwitch::connect_dumper(std::shared_ptr<dumper> dumper)
{
    this->stat_dumper = dumper;
    // Pass dumper to all connected children
    for (auto &n : next) {
        n->connect_dumper(dumper);
    }
}

void CXLSwitch::print_counters()
{
    // Implement printing of CXLSwitch specific counters if any are added.
    // For now, just forward to children.
    for (auto &n : next) {
        n->print_counters();
    }
}

bool CXLSwitch::full()
{
    return request_queue_.full();
}

bool CXLSwitch::pending()
{
    return request_queue_.pending();
}

void CXLSwitch::drain()
{
    request_queue_.drain(); // Clear the request queue for draining
    // Also drain children
    for (auto &n : next) {
        n->drain();
    }
}

clk_t CXLSwitch::calculate_transfer_time(base_request &req) const {
    // Assuming 'cpu_cl_size' (64 bytes) is the minimum transfer granularity.
    // If requests have variable sizes, req.size should be used. This will require adding `size` to base_request.
    // For now, we calculate time per cacheline.
    return static_cast<clk_t>(cpu_cl_size * transfer_time_per_byte_);
}

// Register the CXLSwitch with the factory
REGISTER_COMPONENT(CXLSwitch, "cxl_switch");

} // namespace vans
