#ifndef TUNNEL_INTERFACE_H
#define TUNNEL_INTERFACE_H

#include "mesh_radio_interface.h"
#include <unistd.h>
#include <cstddef>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <atomic>
#include <functional>
#include "ILayer.h"

namespace nerfnet {



class TunnelInterface : public ILayer {
public:
    TunnelInterface(int tunnel_fd);
    ~TunnelInterface();

    void Start();
    void Run();

    // Reads data from the tunnel and sends it to the mesh

    // Writes data from the upstream buffer to the tunnel
    void WriteToTunnel();
private:
    void TunnelThread();

    // The file descriptor for the tunnel
    int tunnel_fd_;

    // The thread for the tunnel that reads data from the tunnel and puts it in the downstream buffer
    std::thread tunnel_thread_;

    // The buffer for data coming from the downstream that needs to be written to the tunnel
    std::deque<std::vector<uint8_t>> upstream_buffer_;
    // The buffer for data coming from the tunnel that needs to be sent downstream
    std::deque<std::vector<uint8_t>> downstream_buffer_;
    // The mutex for the upstream buffer
    std::mutex upstream_buffer_mutex_;
    // The mutex for the downstream buffer
    std::mutex downstream_buffer_mutex_;
    // The condition variable for the upstream buffer
    std::atomic<bool> running_;

    void ReceiveFromDownstream(const std::vector<uint8_t>& data) override;
    void ReceiveFromUpstream(const std::vector<uint8_t>& data) override {}
};

} // namespace nerfnet

#endif // TUNNEL_INTERFACE_H
