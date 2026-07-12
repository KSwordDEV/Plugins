#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mstcpip.h>
#include <windows.h>

#include "ndpi_classifier.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view kProtocol = "ksword-plugin/1";
    constexpr std::string_view kPluginId = "ndpi-network-inspector";
    constexpr unsigned int kDefaultDurationSeconds = 60;
    constexpr unsigned int kMaximumDurationSeconds = 3600;

    struct Options
    {
        std::string command;
        std::string targetKind;
        std::string interfaceAddress;
        unsigned int durationSeconds = kDefaultDurationSeconds;
    };

    struct PacketEndpoints
    {
        std::string transport;
        std::string source;
        std::string destination;
    };

    std::string jsonEscape(const std::string_view input)
    {
        std::ostringstream output;
        for (const unsigned char character : input)
        {
            switch (character)
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    static constexpr char kHex[] = "0123456789ABCDEF";
                    output << "\\u00" << kHex[character >> 4] << kHex[character & 0x0F];
                }
                else
                {
                    output << static_cast<char>(character);
                }
                break;
            }
        }
        return output.str();
    }

    void emitEvent(const std::string& body)
    {
        std::cout << "{\"protocol\":\"" << kProtocol
            << "\",\"plugin_id\":\"" << kPluginId << "\"," << body << "}\n";
        std::cout.flush();
    }

    int emitArgumentError(const std::string& message)
    {
        emitEvent("\"event\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"" +
            jsonEscape(message) + "\"");
        return 64;
    }

    bool parseUnsigned(const std::string_view text, unsigned int& valueOut)
    {
        unsigned int value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc() || result.ptr != text.data() + text.size())
        {
            return false;
        }
        valueOut = value;
        return true;
    }

    std::optional<Options> parseArguments(const int argc, char** argv, std::string& errorOut)
    {
        Options options;
        int separatorIndex = -1;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);
            if (argument == "--ksword-plugin")
            {
                if (index + 1 >= argc)
                {
                    errorOut = "--ksword-plugin requires a command";
                    return std::nullopt;
                }
                options.command = argv[++index];
            }
            else if (argument == "--")
            {
                separatorIndex = index;
                break;
            }
        }

        if (options.command.empty())
        {
            errorOut = "missing --ksword-plugin command";
            return std::nullopt;
        }

        for (int index = separatorIndex >= 0 ? separatorIndex + 1 : argc; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);
            const auto requireValue = [&](std::string& target) -> bool
                {
                    if (index + 1 >= argc)
                    {
                        errorOut = std::string(argument) + " requires a value";
                        return false;
                    }
                    target = argv[++index];
                    return true;
                };

            if (argument == "--target-kind")
            {
                if (!requireValue(options.targetKind)) return std::nullopt;
            }
            else if (argument == "--interface")
            {
                if (!requireValue(options.interfaceAddress)) return std::nullopt;
            }
            else if (argument == "--duration")
            {
                std::string durationText;
                if (!requireValue(durationText)) return std::nullopt;
                if (!parseUnsigned(durationText, options.durationSeconds) ||
                    options.durationSeconds == 0 || options.durationSeconds > kMaximumDurationSeconds)
                {
                    errorOut = "--duration must be between 1 and 3600 seconds";
                    return std::nullopt;
                }
            }
            else
            {
                errorOut = "unsupported argument: " + std::string(argument);
                return std::nullopt;
            }
        }
        return options;
    }

    std::optional<IN_ADDR> parseIpv4Address(const std::string& addressText)
    {
        IN_ADDR address{};
        if (InetPtonA(AF_INET, addressText.c_str(), &address) != 1)
        {
            return std::nullopt;
        }
        return address;
    }

    std::optional<IN_ADDR> findCaptureAddress()
    {
        ULONG bufferSize = 0;
        constexpr ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW)
        {
            return std::nullopt;
        }

        std::vector<std::uint8_t> buffer(bufferSize);
        auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapter, &bufferSize) != NO_ERROR)
        {
            return std::nullopt;
        }

        for (auto* current = adapter; current != nullptr; current = current->Next)
        {
            if (current->OperStatus != IfOperStatusUp || current->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            {
                continue;
            }
            for (auto* unicast = current->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                if (unicast->Address.lpSockaddr == nullptr || unicast->Address.lpSockaddr->sa_family != AF_INET)
                {
                    continue;
                }
                const auto* socketAddress = reinterpret_cast<const SOCKADDR_IN*>(unicast->Address.lpSockaddr);
                const std::uint32_t hostAddress = ntohl(socketAddress->sin_addr.s_addr);
                if ((hostAddress >> 24) == 127 || hostAddress == 0)
                {
                    continue;
                }
                return socketAddress->sin_addr;
            }
        }
        return std::nullopt;
    }

    std::string addressToText(const IN_ADDR& address)
    {
        std::array<char, INET_ADDRSTRLEN> text{};
        return InetNtopA(AF_INET, &address, text.data(), static_cast<DWORD>(text.size())) != nullptr
            ? std::string(text.data())
            : std::string("0.0.0.0");
    }

    std::uint16_t readNetworkUInt16(const std::uint8_t* bytes)
    {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[0]) << 8) |
            static_cast<std::uint16_t>(bytes[1]));
    }

    PacketEndpoints describeIpv4Packet(const std::uint8_t* packet, const std::size_t packetLength)
    {
        PacketEndpoints endpoints;
        if (packet == nullptr || packetLength < 20 || (packet[0] >> 4) != 4)
        {
            return endpoints;
        }

        const std::size_t headerLength = static_cast<std::size_t>(packet[0] & 0x0F) * 4;
        if (headerLength < 20 || headerLength + 4 > packetLength)
        {
            return endpoints;
        }

        IN_ADDR sourceAddress{};
        IN_ADDR destinationAddress{};
        std::memcpy(&sourceAddress, packet + 12, sizeof(sourceAddress));
        std::memcpy(&destinationAddress, packet + 16, sizeof(destinationAddress));
        const std::uint16_t sourcePort = readNetworkUInt16(packet + headerLength);
        const std::uint16_t destinationPort = readNetworkUInt16(packet + headerLength + 2);
        endpoints.transport = packet[9] == IPPROTO_TCP ? "TCP" : packet[9] == IPPROTO_UDP ? "UDP" : "IP";
        endpoints.source = addressToText(sourceAddress) + ":" + std::to_string(sourcePort);
        endpoints.destination = addressToText(destinationAddress) + ":" + std::to_string(destinationPort);
        return endpoints;
    }

    int runSelfTest()
    {
        // Minimal IPv4/UDP DNS query for example.com. Checksums are intentionally
        // zero because nDPI classifies packet contents and does not validate them.
        const std::array<std::uint8_t, 57> dnsPacket = {
            0x45, 0x00, 0x00, 0x39, 0x00, 0x01, 0x00, 0x00,
            0x40, 0x11, 0x00, 0x00, 0xC0, 0x00, 0x02, 0x01,
            0x08, 0x08, 0x08, 0x08,
            0xCF, 0x08, 0x00, 0x35, 0x00, 0x25, 0x00, 0x00,
            0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
            0x03, 'c', 'o', 'm', 0x00, 0x00, 0x01, 0x00, 0x01
        };

        ks::network::NdpiPacketClassifier classifier;
        if (!classifier.IsAvailable())
        {
            emitEvent("\"event\":\"error\",\"code\":\"ndpi_init_failed\",\"message\":\"nDPI initialization failed\"");
            return 2;
        }
        const auto classification = classifier.ClassifyPacket(
            dnsPacket.data(), dnsPacket.size(), GetTickCount64());
        const bool passed = classification.classified &&
            classification.protocolName.find("DNS") != std::string::npos;
        emitEvent("\"event\":\"selftest_complete\",\"passed\":" + std::string(passed ? "true" : "false") +
            ",\"application\":\"" + jsonEscape(classification.protocolName) +
            "\",\"category\":\"" + jsonEscape(classification.categoryName) +
            "\",\"state\":\"" + jsonEscape(classification.stateName) + "\"");
        return passed ? 0 : 3;
    }

    int runCapture(const Options& options)
    {
        const std::optional<IN_ADDR> captureAddress = options.interfaceAddress.empty()
            ? findCaptureAddress()
            : parseIpv4Address(options.interfaceAddress);
        if (!captureAddress.has_value())
        {
            emitEvent("\"event\":\"error\",\"code\":\"interface_not_found\",\"message\":\"No usable IPv4 capture interface was found\"");
            return 2;
        }

        ks::network::NdpiPacketClassifier classifier;
        if (!classifier.IsAvailable())
        {
            emitEvent("\"event\":\"error\",\"code\":\"ndpi_init_failed\",\"message\":\"nDPI initialization failed\"");
            return 2;
        }

        const SOCKET captureSocket = socket(AF_INET, SOCK_RAW, IPPROTO_IP);
        if (captureSocket == INVALID_SOCKET)
        {
            emitEvent("\"event\":\"error\",\"code\":\"socket_failed\",\"message\":\"Raw socket creation failed; run KSword as administrator\",\"wsa_error\":" +
                std::to_string(WSAGetLastError()));
            return 2;
        }

        SOCKADDR_IN bindAddress{};
        bindAddress.sin_family = AF_INET;
        bindAddress.sin_addr = *captureAddress;
        if (bind(captureSocket, reinterpret_cast<const SOCKADDR*>(&bindAddress), sizeof(bindAddress)) == SOCKET_ERROR)
        {
            const int errorCode = WSAGetLastError();
            closesocket(captureSocket);
            emitEvent("\"event\":\"error\",\"code\":\"bind_failed\",\"message\":\"Unable to bind the raw capture socket\",\"wsa_error\":" +
                std::to_string(errorCode));
            return 2;
        }

        DWORD receiveAll = RCVALL_ON;
        DWORD returnedBytes = 0;
        if (WSAIoctl(captureSocket, SIO_RCVALL, &receiveAll, sizeof(receiveAll), nullptr, 0,
            &returnedBytes, nullptr, nullptr) == SOCKET_ERROR)
        {
            const int errorCode = WSAGetLastError();
            closesocket(captureSocket);
            emitEvent("\"event\":\"error\",\"code\":\"capture_enable_failed\",\"message\":\"SIO_RCVALL failed; administrator privileges are required\",\"wsa_error\":" +
                std::to_string(errorCode));
            return 2;
        }

        const DWORD receiveTimeoutMs = 1000;
        setsockopt(captureSocket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));

        const std::string interfaceText = addressToText(*captureAddress);
        emitEvent("\"event\":\"capture_started\",\"total_flows\":0,\"interface\":\"" +
            jsonEscape(interfaceText) + "\",\"duration_seconds\":" + std::to_string(options.durationSeconds));

        std::array<std::uint8_t, 65536> packetBuffer{};
        std::uint64_t packetCount = 0;
        std::uint64_t classifiedFlowCount = 0;
        std::uint64_t receiveErrorCount = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.durationSeconds);
        while (std::chrono::steady_clock::now() < deadline)
        {
            const int receivedLength = recv(captureSocket,
                reinterpret_cast<char*>(packetBuffer.data()), static_cast<int>(packetBuffer.size()), 0);
            if (receivedLength == SOCKET_ERROR)
            {
                const int errorCode = WSAGetLastError();
                if (errorCode == WSAETIMEDOUT || errorCode == WSAEWOULDBLOCK)
                {
                    continue;
                }
                ++receiveErrorCount;
                if (receiveErrorCount >= 5)
                {
                    break;
                }
                continue;
            }
            if (receivedLength <= 0)
            {
                continue;
            }

            ++packetCount;
            const auto classification = classifier.ClassifyPacket(
                packetBuffer.data(), static_cast<std::size_t>(receivedLength), GetTickCount64());
            if (!classification.classificationChanged)
            {
                continue;
            }

            ++classifiedFlowCount;
            const PacketEndpoints endpoints = describeIpv4Packet(
                packetBuffer.data(), static_cast<std::size_t>(receivedLength));
            const std::string protocolIds = std::to_string(classification.masterProtocolId) + "/" +
                std::to_string(classification.applicationProtocolId);
            emitEvent("\"event\":\"flow_result\",\"status\":\"classified\",\"application\":\"" +
                jsonEscape(classification.protocolName) + "\",\"category\":\"" +
                jsonEscape(classification.categoryName) + "\",\"transport\":\"" +
                jsonEscape(endpoints.transport) + "\",\"source\":\"" + jsonEscape(endpoints.source) +
                "\",\"destination\":\"" + jsonEscape(endpoints.destination) + "\",\"breed\":\"" +
                jsonEscape(classification.breedName) + "\",\"state\":\"" +
                jsonEscape(classification.stateName) + "\",\"protocol_ids\":\"" + protocolIds + "\"");
        }

        receiveAll = RCVALL_OFF;
        WSAIoctl(captureSocket, SIO_RCVALL, &receiveAll, sizeof(receiveAll), nullptr, 0,
            &returnedBytes, nullptr, nullptr);
        closesocket(captureSocket);

        emitEvent("\"event\":\"capture_complete\",\"packets\":" + std::to_string(packetCount) +
            ",\"flows\":" + std::to_string(classifiedFlowCount) +
            ",\"errors\":" + std::to_string(receiveErrorCount));
        return receiveErrorCount >= 5 ? 2 : 0;
    }
}

int main(const int argc, char** argv)
{
    std::string argumentError;
    const std::optional<Options> options = parseArguments(argc, argv, argumentError);
    if (!options.has_value())
    {
        return emitArgumentError(argumentError);
    }

    WSADATA winsockData{};
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0)
    {
        emitEvent("\"event\":\"error\",\"code\":\"winsock_init_failed\",\"message\":\"WSAStartup failed\"");
        return 2;
    }

    emitEvent("\"event\":\"ready\",\"version\":\"1.0.0\",\"commands\":[\"capture\",\"info\",\"selftest\"],\"targets\":[\"network\"]");
    if (options->command == "info")
    {
        emitEvent("\"event\":\"plugin_info\",\"name\":\"nDPI Network Inspector\",\"ndpi_version\":\"5.0\"");
        WSACleanup();
        return 0;
    }
    if (options->command == "selftest")
    {
        const int exitCode = runSelfTest();
        WSACleanup();
        return exitCode;
    }
    if (options->command != "capture")
    {
        WSACleanup();
        return emitArgumentError("unsupported command: " + options->command);
    }
    if (options->targetKind != "network")
    {
        WSACleanup();
        return emitArgumentError("capture requires --target-kind network");
    }

    const int exitCode = runCapture(*options);
    WSACleanup();
    return exitCode;
}
