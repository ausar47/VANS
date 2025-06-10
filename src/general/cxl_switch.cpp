#include "cxl_switch.h"
#include "factory.h"
#include "trace.h" // For trace logging if needed

namespace vans
{

CXLSwitch::CXLSwitch(const std::string &name, Config *config) :
    Component(name, config),
    latency_(0),
    bandwidth_gb_s_(0.0),
    transfer_time_per_byte_(0),
    request_queue_(config->GetInt(name + ".queue_size", 32)), // Default queue size 32
    child_(nullptr)
{
    // Parse configuration parameters
    latency_ = static_cast<Tick_t>(config->GetInt(name + ".latency", 50)); // Default 50ns
    bandwidth_gb_s_ = config->GetFloat(name + ".bandwidth_gb_s", 64.0); // Default 64 GB/s

    // Calculate transfer time per byte: 1 / (bandwidth_gb_s * 1024 * 1024 * 1024 / 1e9) ns/byte
    // Simplified: 1e9 ns/s / (bandwidth_gb_s * 1024^3 bytes/GB)
    // 1 ns/byte = 1/((1GB/s)*1e9)
    // bandwidth_gb_s is already in GB/s
    // transfer_time_per_byte_ = (1.0 / (bandwidth_gb_s_ * 1024 * 1024 * 1024)) * 1e9; // ns per byte
    // Assuming 1 Tick = 1 ns for now. If not, need conversion factor.
    // Bandwidth is typically Bytes/ns
    // 64 GB/s = 64 * 1024 MB/s = 64 * 1024 * 1024 KB/s = 64 * 1024 * 1024 * 1024 Bytes/s
    // 64 * 1024 * 1024 * 1024 Bytes / (1e9 ns) = 68.7 Bytes/ns (approx)
    // So, time per byte is 1 / (Bytes/ns) = ns/byte
    if (bandwidth_gb_s_ > 0) {
        transfer_time_per_byte_ = static_cast<Tick_t>(1.0 / (bandwidth_gb_s_ * (1024.0 * 1024.0 * 1024.0) / 1e9));
    } else {
        transfer_time_per_byte_ = 0; // Infinite bandwidth if 0
    }
}

CXLSwitch::~CXLSwitch()
{
}

void CXLSwitch::Setup(Component *parent, std::vector<Component *> &children)
{
    parent_ = parent;
    if (children.size() > 1) {
        ERROR("CXLSwitch '%s' expects only one child component.", name_.c_str());
        // For now, only support one child for simplicity.
        // A real switch would handle multiple ports/children.
    }
    if (!children.empty()) {
        child_ = children[0];
    }
}

void CXLSwitch::Tick()
{
    // Process requests in the queue
    if (!request_queue_.empty()) {
        Request *req = request_queue_.front();

        // Check if the request is ready to be forwarded
        if (req->arrive_time_ <= GetGlobalTick()) {
            request_queue_.pop_front();

            // Forward the request to the child component
            if (child_) {
                child_->ReceiveRequest(req);
            } else {
                // If no child, complete the request (e.g., error or loopback)
                req->finish_time_ = GetGlobalTick();
                parent_->ReceiveRequest(req); // Send back to parent as completed
            }
        }
    }
}

void CXLSwitch::ReceiveRequest(Request *req)
{
    // Add CXL switch latency to the request's arrival time
    req->arrive_time_ = GetGlobalTick() + latency_;

    // If bandwidth is limited, calculate additional transfer time
    // Assuming 'req->size_bytes_' is available on the Request object
    if (transfer_time_per_byte_ > 0 && req->size_bytes_ > 0) {
        req->arrive_time_ += CalculateTransferTime(req);
    }

    // Enqueue the request
    if (!request_queue_.Push(req)) {
        ERROR("CXLSwitch '%s' request queue is full. Request dropped or stalled.", name_.c_str());
        // In a real simulator, requests might stall here,
        // or a backpressure mechanism would be implemented.
        // For now, simply indicating an error.
    }
}

Tick_t CXLSwitch::CalculateTransferTime(Request *req) const {
    // Assuming cacheline size is 64 bytes for common memory accesses
    // This could be made configurable or determined by request type
    const int CACHELINE_SIZE = 64;
    size_t num_cachelines = (req->size_bytes_ + CACHELINE_SIZE - 1) / CACHELINE_SIZE;
    return static_cast<Tick_t>(num_cachelines * CACHELINE_SIZE * transfer_time_per_byte_);
}


// Register the CXLSwitch with the factory
REGISTER_COMPONENT(CXLSwitch, "cxl_switch");

} // namespace vans