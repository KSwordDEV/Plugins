#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace ks::network
{
    struct NdpiClassification
    {
        bool engineAvailable = false;
        bool classified = false;
        bool classificationChanged = false;
        bool finalResult = false;
        std::uint16_t masterProtocolId = 0;
        std::uint16_t applicationProtocolId = 0;
        std::uint16_t categoryId = 0;
        std::string protocolName = "Unknown";
        std::string categoryName = "Unspecified";
        std::string breedName = "Unknown";
        std::string stateName = "Unavailable";
    };

    // Stateful nDPI classifier. One instance is owned by a capture thread so
    // nDPI's detection module and per-flow state never cross threads.
    class NdpiPacketClassifier final
    {
    public:
        NdpiPacketClassifier();
        ~NdpiPacketClassifier();

        NdpiPacketClassifier(const NdpiPacketClassifier&) = delete;
        NdpiPacketClassifier& operator=(const NdpiPacketClassifier&) = delete;

        [[nodiscard]] bool IsAvailable() const noexcept;
        [[nodiscard]] NdpiClassification ClassifyPacket(
            const std::uint8_t* layer3Packet,
            std::size_t packetLength,
            std::uint64_t timestampMs);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
