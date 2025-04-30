#ifndef TUNNEL_INTERFACE_H
#define TUNNEL_INTERFACE_H

#include "mesh_radio_interface.h"
#include <unistd.h>
#include <cstddef>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <functional>

namespace nerfnet {



class TunnelInterface {
public:
    TunnelInterface(MeshRadioInterface& mesh_radio_interface, int tunnel_fd);
    ~TunnelInterface();

    void Run();

    // Reads data from the tunnel and sends it to the mesh
    void ReadFromTunnel();

    // Writes data to the tunnel
    void WriteToTunnel();

    // Sets a callback to handle data from the mesh
    void SetDataCallback(DataCallback callback);

private:
    void TunnelThread();
    void InitializeCallback();

    MeshRadioInterface& mesh_radio_interface_;
    int tunnel_fd_;
    std::thread tunnel_thread_;
    std::mutex buffer_mutex_;
    std::vector<std::vector<uint8_t>> buffer_;
    std::atomic<bool> running_;
    bool logs_enabled_;
    DataCallback data_callback_;
};

} // namespace nerfnet

#endif // TUNNEL_INTERFACE_H
