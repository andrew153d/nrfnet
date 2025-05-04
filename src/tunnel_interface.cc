#include "tunnel_interface.h"
#include "log.h"
#include <cstring>
#include <errno.h>
#include "nrftime.h"
#include <queue>

namespace nerfnet
{

    TunnelInterface::TunnelInterface(int tunnel_fd)
        : tunnel_fd_(tunnel_fd), running_(true)
    {
    }

    TunnelInterface::~TunnelInterface()
    {
        running_ = false;
        if (tunnel_thread_.joinable())
        {
            tunnel_thread_.join();
        }
    }

    void TunnelInterface::Start()
    {
        tunnel_thread_ = std::thread(&TunnelInterface::TunnelThread, this);
    }

    void TunnelInterface::Run()
    {
        std::lock_guard<std::mutex> lock(downstream_buffer_mutex_);
        if (!downstream_buffer_.empty())
        {
            auto &data = downstream_buffer_.front();
            LOGI("Sending %zu bytes to downstream", data.size());
            SendDownstream(data);
            downstream_buffer_.pop_front();
        }

        WriteToTunnel();
    }

    void TunnelInterface::WriteToTunnel()
    {
        std::lock_guard<std::mutex> lock(upstream_buffer_mutex_);
        if (!upstream_buffer_.empty())
        {
            auto &data = upstream_buffer_.front();
            //ssize_t bytes_written = data.size();
            //LOGW("Faking write %ld bytes to tunnel", bytes_written);
            ssize_t bytes_written = write(tunnel_fd_, data.data(), data.size());
            upstream_buffer_.pop_front();
        }
    }

    void TunnelInterface::TunnelThread()
    {
        constexpr size_t kMaxBufferedFrames = 1024;
        uint8_t buffer[3200];

        while (running_)
        {
            int bytes_read = read(tunnel_fd_, buffer, sizeof(buffer));
            if (bytes_read < 0)
            {
                LOGE("Failed to read: %s (%d)", strerror(errno), errno);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(downstream_buffer_mutex_);
                downstream_buffer_.push_back(std::vector<uint8_t>(&buffer[0], &buffer[bytes_read]));

                LOGI("Read %zu bytes from the tunnel", downstream_buffer_.back().size());
            }

            while (downstream_buffer_.size() > kMaxBufferedFrames && running_)
            {
                SleepUs(1000);
            }
        }
    }

    void TunnelInterface::ReceiveFromDownstream(const std::vector<uint8_t> &data)
    {
        LOGI("Tunnel Layer Received %zu bytes from downstream", data.size());
        std::lock_guard<std::mutex> lock(upstream_buffer_mutex_);
        upstream_buffer_.push_back(data);
    }
} // namespace nerfnet
