/*
 * Copyright 2023 Your Name
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ILAYER_H
#define ILAYER_H

#include <vector>
#include <functional>
#include <cstdint>
#include "log.h"



class ILayer
{
public:
    virtual ~ILayer() = default;

    // Set the downstream layer (the layer below this one)
    virtual void SetDownstreamLayer(ILayer *downstream)
    {
        downstream_layer_ = downstream;
    }

    // Set the upstream layer (the layer above this one)
    virtual void SetUpstreamLayer(ILayer *upstream)
    {
        upstream_layer_ = upstream;
    }

    // Pass data downstream (to the lower layer)
    virtual void SendDownstream(const std::vector<uint8_t> &data)
    {
        if (downstream_layer_)
        {
            downstream_layer_->ReceiveFromUpstream(data);
        }else
        {
            LOGE("No downstream layer set");
        }
    }

    // Pass data upstream (to the higher layer)
    virtual void SendUpstream(const std::vector<uint8_t> &data)
    {
        if (upstream_layer_)
        {
            upstream_layer_->ReceiveFromDownstream(data);
        }else
        {
            LOGE("No upstream layer set");
        }
    }

    // Receive data from the lower layer
    virtual void ReceiveFromDownstream(const std::vector<uint8_t> &data) = 0;

    // Receive data from the higher layer
    virtual void ReceiveFromUpstream(const std::vector<uint8_t> &data) = 0;

    // Layer enable setter
    void SetLayerEnable(bool enable)
    {
        layer_enabled_ = enable;
    }
private:
    // Layer enable
    bool layer_enabled_ = true;

    // The downstream layer (the layer below this one)
    ILayer *downstream_layer_ = nullptr;

    // The upstream layer (the layer above this one)
    ILayer *upstream_layer_ = nullptr;
};

#endif // ILAYER_H