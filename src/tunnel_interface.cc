#include "tunnel_interface.h"
#include "log.h"
#include <cstring>
#include <errno.h>
#include "nrftime.h"
namespace nerfnet {

TunnelInterface::TunnelInterface(MeshRadioInterface& mesh_radio_interface, int tunnel_fd)
    : mesh_radio_interface_(mesh_radio_interface), tunnel_fd_(tunnel_fd), running_(true), logs_enabled_(false) {
}

TunnelInterface::~TunnelInterface() {
    running_ = false;
    if (tunnel_thread_.joinable()) {
        tunnel_thread_.join();
    }
}

void TunnelInterface::Run() {
    InitializeCallback();
    tunnel_thread_ = std::thread(&TunnelInterface::TunnelThread, this);
    while(1)
    {
        mesh_radio_interface_.Run();
    }
}

void TunnelInterface::ReadFromTunnel() {
    char buffer[1024];
    ssize_t bytes_read = read(tunnel_fd_, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        LOGI("Read %ld bytes from tunnel", bytes_read);
        // Process the data read from the tunnel
        if (data_callback_) {
            data_callback_(std::vector<uint8_t>(buffer, buffer + bytes_read));
        }
    } else if (bytes_read < 0) {
        LOGE("Error reading from tunnel");
    }
}

void TunnelInterface::WriteToTunnel() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (!buffer_.empty()) {
        auto& data = buffer_.front();
        ssize_t bytes_written = write(tunnel_fd_, data.data(), data.size());
        if (logs_enabled_) {
            LOGI("Wrote %ld bytes to tunnel", bytes_written);
        }
        buffer_.erase(buffer_.begin());
    }
}

void TunnelInterface::SetDataCallback(DataCallback callback) {
    data_callback_ = std::move(callback);
}

void TunnelInterface::TunnelThread() {
    constexpr size_t kMaxBufferedFrames = 1024;
    uint8_t buffer[3200];

    while (running_) {
        int bytes_read = read(tunnel_fd_, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            LOGE("Failed to read: %s (%d)", strerror(errno), errno);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_.emplace_back(&buffer[0], &buffer[bytes_read]);
            if (logs_enabled_) {
                LOGI("Read %zu bytes from the tunnel", buffer_.back().size());
            }
        }

        while (buffer_.size() > kMaxBufferedFrames && running_) {
            SleepUs(1000);
        }
    }
}

void TunnelInterface::InitializeCallback() {
    mesh_radio_interface_.SetTunnelCallback([this](const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.emplace_back(data);
        
        // Print the received data
        LOGI("Received %zu bytes from mesh", data.size());
    });
}

} // namespace nerfnet
