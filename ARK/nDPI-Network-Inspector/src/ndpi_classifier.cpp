#include "ndpi_classifier.h"

#include "../third_party/nDPI/ksword_bridge/ksword_ndpi_bridge.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace ks::network
{
    namespace
    {
        constexpr std::uint64_t kFlowIdleTimeoutMs = 120000;
        constexpr std::size_t kMaximumTrackedFlows = 8192;
        constexpr std::uint64_t kCleanupPacketInterval = 512;

        struct FlowEndpoint
        {
            std::array<std::uint8_t, 16> address{};
            std::uint16_t port = 0;
            std::uint8_t addressLength = 0;

            bool operator==(const FlowEndpoint&) const = default;
        };

        struct FlowKey
        {
            FlowEndpoint first;
            FlowEndpoint second;
            std::uint8_t ipVersion = 0;
            std::uint8_t transportProtocol = 0;

            bool operator==(const FlowKey&) const = default;
        };

        int compareEndpoints(const FlowEndpoint& left, const FlowEndpoint& right)
        {
            if (left.addressLength != right.addressLength)
            {
                return left.addressLength < right.addressLength ? -1 : 1;
            }
            const int addressCompare = std::memcmp(
                left.address.data(),
                right.address.data(),
                left.addressLength);
            if (addressCompare != 0)
            {
                return addressCompare;
            }
            if (left.port == right.port)
            {
                return 0;
            }
            return left.port < right.port ? -1 : 1;
        }

        std::uint16_t readNetworkUInt16(const std::uint8_t* bytes)
        {
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(bytes[0]) << 8) |
                static_cast<std::uint16_t>(bytes[1]));
        }

        bool locateIpv6Transport(
            const std::uint8_t* packet,
            const std::size_t packetLength,
            std::uint8_t& protocolOut,
            std::size_t& transportOffsetOut)
        {
            if (packetLength < 40)
            {
                return false;
            }

            std::uint8_t nextHeader = packet[6];
            std::size_t offset = 40;
            for (int extensionCount = 0; extensionCount < 12; ++extensionCount)
            {
                if (nextHeader == 6 || nextHeader == 17)
                {
                    protocolOut = nextHeader;
                    transportOffsetOut = offset;
                    return true;
                }
                if (nextHeader == 0 || nextHeader == 43 || nextHeader == 60)
                {
                    if (offset + 2 > packetLength)
                    {
                        return false;
                    }
                    const std::size_t extensionLength = (static_cast<std::size_t>(packet[offset + 1]) + 1) * 8;
                    nextHeader = packet[offset];
                    offset += extensionLength;
                }
                else if (nextHeader == 44)
                {
                    if (offset + 8 > packetLength)
                    {
                        return false;
                    }
                    const std::uint16_t fragmentField = readNetworkUInt16(packet + offset + 2);
                    if ((fragmentField & 0xFFF8U) != 0)
                    {
                        return false;
                    }
                    nextHeader = packet[offset];
                    offset += 8;
                }
                else if (nextHeader == 51)
                {
                    if (offset + 2 > packetLength)
                    {
                        return false;
                    }
                    const std::size_t extensionLength = (static_cast<std::size_t>(packet[offset + 1]) + 2) * 4;
                    nextHeader = packet[offset];
                    offset += extensionLength;
                }
                else
                {
                    return false;
                }

                if (offset > packetLength)
                {
                    return false;
                }
            }
            return false;
        }

        bool buildFlowKey(
            const std::uint8_t* packet,
            const std::size_t packetLength,
            FlowKey& keyOut)
        {
            if (packet == nullptr || packetLength < 1)
            {
                return false;
            }

            FlowEndpoint source;
            FlowEndpoint destination;
            std::size_t transportOffset = 0;
            const std::uint8_t version = static_cast<std::uint8_t>(packet[0] >> 4);
            std::uint8_t protocol = 0;

            if (version == 4)
            {
                if (packetLength < 20)
                {
                    return false;
                }
                const std::size_t headerLength = static_cast<std::size_t>(packet[0] & 0x0FU) * 4;
                if (headerLength < 20 || headerLength + 4 > packetLength ||
                    (readNetworkUInt16(packet + 6) & 0x1FFFU) != 0)
                {
                    return false;
                }
                protocol = packet[9];
                transportOffset = headerLength;
                source.addressLength = 4;
                destination.addressLength = 4;
                std::memcpy(source.address.data(), packet + 12, 4);
                std::memcpy(destination.address.data(), packet + 16, 4);
            }
            else if (version == 6)
            {
                if (packetLength < 40 || !locateIpv6Transport(packet, packetLength, protocol, transportOffset) ||
                    transportOffset + 4 > packetLength)
                {
                    return false;
                }
                source.addressLength = 16;
                destination.addressLength = 16;
                std::memcpy(source.address.data(), packet + 8, 16);
                std::memcpy(destination.address.data(), packet + 24, 16);
            }
            else
            {
                return false;
            }

            if ((protocol != 6 && protocol != 17) || transportOffset + 4 > packetLength)
            {
                return false;
            }
            source.port = readNetworkUInt16(packet + transportOffset);
            destination.port = readNetworkUInt16(packet + transportOffset + 2);

            keyOut.ipVersion = version;
            keyOut.transportProtocol = protocol;
            if (compareEndpoints(source, destination) <= 0)
            {
                keyOut.first = source;
                keyOut.second = destination;
            }
            else
            {
                keyOut.first = destination;
                keyOut.second = source;
            }
            return true;
        }

        struct FlowKeyHash
        {
            std::size_t operator()(const FlowKey& key) const noexcept
            {
                std::size_t hashValue = 1469598103934665603ULL;
                const auto mixByte = [&hashValue](const std::uint8_t byteValue)
                    {
                        hashValue ^= byteValue;
                        hashValue *= 1099511628211ULL;
                    };
                mixByte(key.ipVersion);
                mixByte(key.transportProtocol);
                for (const FlowEndpoint* endpoint : { &key.first, &key.second })
                {
                    mixByte(endpoint->addressLength);
                    for (std::size_t index = 0; index < endpoint->addressLength; ++index)
                    {
                        mixByte(endpoint->address[index]);
                    }
                    mixByte(static_cast<std::uint8_t>(endpoint->port >> 8));
                    mixByte(static_cast<std::uint8_t>(endpoint->port & 0xFFU));
                }
                return hashValue;
            }
        };

        std::string classificationStateName(const std::uint8_t state)
        {
            switch (state)
            {
            case 0: return "Inspecting";
            case 1: return "Partial";
            case 2: return "Monitoring";
            case 3: return "Classified";
            default: return "Unknown";
            }
        }

        std::string normalizeText(const char* text, const char* fallback)
        {
            return text != nullptr && text[0] != '\0' ? std::string(text) : std::string(fallback);
        }
    }

    struct NdpiPacketClassifier::Impl
    {
        struct FlowState
        {
            void* flowHandle = nullptr;
            std::uint64_t lastSeenMs = 0;
            std::uint16_t lastMasterProtocolId = 0;
            std::uint16_t lastApplicationProtocolId = 0;
            bool classificationReported = false;
        };

        void* engineHandle = nullptr;
        std::unordered_map<FlowKey, FlowState, FlowKeyHash> flowByKey;
        std::uint64_t packetCount = 0;

        Impl()
            : engineHandle(ksword_ndpi_engine_create())
        {
        }

        ~Impl()
        {
            for (auto& [key, flowState] : flowByKey)
            {
                (void)key;
                ksword_ndpi_flow_destroy(flowState.flowHandle);
            }
            ksword_ndpi_engine_destroy(engineHandle);
        }

        void cleanup(const std::uint64_t nowMs)
        {
            for (auto iterator = flowByKey.begin(); iterator != flowByKey.end();)
            {
                const bool timestampMovedForward = nowMs >= iterator->second.lastSeenMs;
                const bool expired = timestampMovedForward &&
                    nowMs - iterator->second.lastSeenMs > kFlowIdleTimeoutMs;
                if (expired)
                {
                    ksword_ndpi_flow_destroy(iterator->second.flowHandle);
                    iterator = flowByKey.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        void evictOldestFlow()
        {
            if (flowByKey.empty())
            {
                return;
            }
            auto oldest = flowByKey.begin();
            for (auto iterator = std::next(flowByKey.begin()); iterator != flowByKey.end(); ++iterator)
            {
                if (iterator->second.lastSeenMs < oldest->second.lastSeenMs)
                {
                    oldest = iterator;
                }
            }
            ksword_ndpi_flow_destroy(oldest->second.flowHandle);
            flowByKey.erase(oldest);
        }
    };

    NdpiPacketClassifier::NdpiPacketClassifier()
        : m_impl(std::make_unique<Impl>())
    {
    }

    NdpiPacketClassifier::~NdpiPacketClassifier() = default;

    bool NdpiPacketClassifier::IsAvailable() const noexcept
    {
        return m_impl != nullptr && m_impl->engineHandle != nullptr;
    }

    NdpiClassification NdpiPacketClassifier::ClassifyPacket(
        const std::uint8_t* layer3Packet,
        const std::size_t packetLength,
        const std::uint64_t timestampMs)
    {
        NdpiClassification classification;
        classification.engineAvailable = IsAvailable();
        if (!classification.engineAvailable || packetLength > std::numeric_limits<std::uint16_t>::max())
        {
            return classification;
        }

        FlowKey flowKey;
        if (!buildFlowKey(layer3Packet, packetLength, flowKey))
        {
            classification.stateName = "Unsupported";
            return classification;
        }

        ++m_impl->packetCount;
        if (m_impl->packetCount % kCleanupPacketInterval == 0)
        {
            m_impl->cleanup(timestampMs);
        }

        auto iterator = m_impl->flowByKey.find(flowKey);
        if (iterator == m_impl->flowByKey.end())
        {
            if (m_impl->flowByKey.size() >= kMaximumTrackedFlows)
            {
                m_impl->evictOldestFlow();
            }
            Impl::FlowState newState;
            newState.flowHandle = ksword_ndpi_flow_create();
            newState.lastSeenMs = timestampMs;
            if (newState.flowHandle == nullptr)
            {
                classification.stateName = "AllocationFailed";
                return classification;
            }
            iterator = m_impl->flowByKey.emplace(flowKey, newState).first;
        }
        iterator->second.lastSeenMs = timestampMs;

        KswordNdpiResult bridgeResult{};
        if (ksword_ndpi_process_packet(
            m_impl->engineHandle,
            iterator->second.flowHandle,
            layer3Packet,
            packetLength,
            timestampMs,
            &bridgeResult) == 0)
        {
            classification.stateName = "ProcessingFailed";
            return classification;
        }

        classification.classified = bridgeResult.classified != 0;
        classification.finalResult = bridgeResult.classification_state == 3;
        classification.masterProtocolId = bridgeResult.master_protocol_id;
        classification.applicationProtocolId = bridgeResult.application_protocol_id;
        classification.categoryId = bridgeResult.category_id;
        classification.protocolName = normalizeText(bridgeResult.protocol_name, "Unknown");
        classification.categoryName = normalizeText(bridgeResult.category_name, "Unspecified");
        classification.breedName = normalizeText(bridgeResult.breed_name, "Unknown");
        classification.stateName = classificationStateName(bridgeResult.classification_state);
        classification.classificationChanged = classification.classified &&
            (!iterator->second.classificationReported ||
                iterator->second.lastMasterProtocolId != classification.masterProtocolId ||
                iterator->second.lastApplicationProtocolId != classification.applicationProtocolId);
        if (classification.classificationChanged)
        {
            iterator->second.classificationReported = true;
            iterator->second.lastMasterProtocolId = classification.masterProtocolId;
            iterator->second.lastApplicationProtocolId = classification.applicationProtocolId;
        }
        return classification;
    }
}
