/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "artnet_protocol.hpp"
#include "receiver_state.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace flux::artnet {

/// Shared sender socket for every artnet.send object in this module.
///
/// Sending is stateless, so one UDP socket can serve all sender objects. The
/// socket is non-blocking: if the kernel buffer is full a packet is dropped
/// rather than blocking Max's scheduler thread.
class SenderNetworkService
{
  public:
    static SenderNetworkService& instance()
    {
        static SenderNetworkService service;
        return service;
    }

    bool send(const std::vector<std::uint8_t>& packet, const std::string& host, std::uint16_t port, std::string& error)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        error.clear();

        if (!ensure_socket_locked(error))
            return false;

        std::vector<sockaddr_in> destinations;
        if (host.empty()) {
            for (const auto& address : broadcast_addresses_locked()) {
                sockaddr_in destination{};
                destination.sin_family = AF_INET;
                destination.sin_port = htons(port);
                if (inet_pton(AF_INET, address.c_str(), &destination.sin_addr) == 1)
                    destinations.push_back(destination);
            }
            if (destinations.empty()) {
                error = "no usable broadcast address was found";
                return false;
            }
        }
        else {
            destinations = resolve_locked(host, port, error);
            if (destinations.empty()) {
                if (error.empty())
                    error = "could not resolve host '" + host + "'";
                return false;
            }
        }

        std::string first_error;
        bool sent = false;
        for (const auto& destination : destinations) {
            ssize_t result = -1;
            do {
                result = ::sendto(m_fd, packet.data(), packet.size(), 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
            } while (result < 0 && errno == EINTR);

            if (result >= 0 && static_cast<std::size_t>(result) == packet.size()) {
                sent = true;
            }
            else if (first_error.empty()) {
                first_error = std::strerror(errno);
            }
        }

        if (!sent) {
            error = "UDP send failed: " + first_error;
            return false;
        }
        return true;
    }

    /// Exposed for diagnostics and tests.
    std::vector<std::string> broadcast_addresses()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return broadcast_addresses_locked();
    }

  private:
    using steady_clock = std::chrono::steady_clock;

    SenderNetworkService() = default;

    ~SenderNetworkService()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    bool ensure_socket_locked(std::string& error)
    {
        if (m_fd >= 0)
            return true;

        m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_fd < 0) {
            error = "could not create UDP socket: ";
            error += std::strerror(errno);
            return false;
        }

        const int flags = ::fcntl(m_fd, F_GETFL, 0);
        if (flags >= 0)
            ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

        const int enabled = 1;
        if (::setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) != 0) {
            error = "could not enable UDP broadcast: ";
            error += std::strerror(errno);
            ::close(m_fd);
            m_fd = -1;
            return false;
        }
        return true;
    }

    std::vector<std::string> broadcast_addresses_locked()
    {
        const auto now = steady_clock::now();
        if (m_broadcast_cache_valid && (now - m_broadcast_cache_time) < std::chrono::seconds(2))
            return m_broadcast_cache;

        std::set<std::string> unique;
        ifaddrs* interfaces = nullptr;
        if (::getifaddrs(&interfaces) == 0) {
            for (ifaddrs* entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
                if (entry->ifa_addr == nullptr || entry->ifa_broadaddr == nullptr)
                    continue;
                if (entry->ifa_addr->sa_family != AF_INET)
                    continue;
                if ((entry->ifa_flags & IFF_UP) == 0 || (entry->ifa_flags & IFF_BROADCAST) == 0 || (entry->ifa_flags & IFF_LOOPBACK) != 0)
                    continue;

                const auto* broadcast = reinterpret_cast<const sockaddr_in*>(entry->ifa_broadaddr);
                char text[INET_ADDRSTRLEN] = {};
                if (::inet_ntop(AF_INET, &broadcast->sin_addr, text, sizeof(text)) != nullptr) {
                    const std::string address{text};
                    if (address != "0.0.0.0")
                        unique.insert(address);
                }
            }
            ::freeifaddrs(interfaces);
        }

        m_broadcast_cache.assign(unique.begin(), unique.end());
        if (m_broadcast_cache.empty())
            m_broadcast_cache.emplace_back("255.255.255.255");
        m_broadcast_cache_time = now;
        m_broadcast_cache_valid = true;
        return m_broadcast_cache;
    }

    std::vector<sockaddr_in> resolve_locked(const std::string& host, std::uint16_t port, std::string& error)
    {
        auto cached = m_resolved_hosts.find(host);
        if (cached == m_resolved_hosts.end()) {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            addrinfo* results = nullptr;
            const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &results);
            if (rc != 0) {
                error = "could not resolve host '" + host + "': " + ::gai_strerror(rc);
                return {};
            }

            std::set<std::uint32_t> unique;
            std::vector<in_addr> addresses;
            for (addrinfo* result = results; result != nullptr; result = result->ai_next) {
                if (result->ai_family != AF_INET || result->ai_addr == nullptr)
                    continue;
                const auto* address = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
                if (!unique.insert(address->sin_addr.s_addr).second)
                    continue;
                addresses.push_back(address->sin_addr);
            }
            ::freeaddrinfo(results);

            if (addresses.empty()) {
                error = "host '" + host + "' did not resolve to an IPv4 address";
                return {};
            }
            cached = m_resolved_hosts.emplace(host, std::move(addresses)).first;
        }

        std::vector<sockaddr_in> destinations;
        destinations.reserve(cached->second.size());
        for (const auto& address : cached->second) {
            sockaddr_in destination{};
            destination.sin_family = AF_INET;
            destination.sin_port = htons(port);
            destination.sin_addr = address;
            destinations.push_back(destination);
        }
        return destinations;
    }

    std::mutex m_mutex;
    int m_fd{-1};
    bool m_broadcast_cache_valid{false};
    steady_clock::time_point m_broadcast_cache_time{};
    std::vector<std::string> m_broadcast_cache;
    std::map<std::string, std::vector<in_addr>> m_resolved_hosts;
};

class ReceiverSubscription;

/// The registration handle returned by ReceiverNetworkService::subscribe().
class ReceiverSubscription
{
  public:
    ReceiverSubscription(std::shared_ptr<ReceiverState> a_state, std::uint16_t a_port)
        : state(std::move(a_state))
        , port(a_port)
    {
    }

    std::shared_ptr<ReceiverState> state;
    std::uint16_t port{0};
};

/// One UDP listener per local port, shared by every artnet.receive object that
/// uses that port. The listener never calls into Max; it only updates the
/// thread-safe ReceiverState owned by each subscriber.
class ReceiverPortListener
{
  public:
    explicit ReceiverPortListener(std::uint16_t port)
        : m_port(port)
    {
    }

    ~ReceiverPortListener()
    {
        stop();
    }

    ReceiverPortListener(const ReceiverPortListener&) = delete;
    ReceiverPortListener& operator=(const ReceiverPortListener&) = delete;

    bool start(std::string& error)
    {
        error.clear();
        m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_fd < 0) {
            error = "could not create UDP socket: ";
            error += std::strerror(errno);
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(m_port);

        if (::bind(m_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            error = "could not bind UDP port " + std::to_string(m_port) + ": " + std::strerror(errno);
            ::close(m_fd);
            m_fd = -1;
            return false;
        }

        const int flags = ::fcntl(m_fd, F_GETFL, 0);
        if (flags >= 0)
            ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

        m_running.store(true);
        try {
            m_thread = std::thread(&ReceiverPortListener::run, this);
        }
        catch (const std::exception& exception) {
            m_running.store(false);
            ::close(m_fd);
            m_fd = -1;
            error = "could not start UDP receive thread: ";
            error += exception.what();
            return false;
        }
        return true;
    }

    void stop()
    {
        m_running.store(false);
        if (m_thread.joinable())
            m_thread.join();
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    void add_subscription(const std::shared_ptr<ReceiverSubscription>& subscription)
    {
        std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
        m_subscriptions.push_back(subscription);
    }

    void remove_subscription(const std::shared_ptr<ReceiverSubscription>& subscription)
    {
        std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
        m_subscriptions.erase(std::remove(m_subscriptions.begin(), m_subscriptions.end(), subscription), m_subscriptions.end());
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
        return m_subscriptions.empty();
    }

    std::uint64_t datagram_count() const
    {
        return m_datagrams.load();
    }

    std::uint64_t malformed_count() const
    {
        return m_malformed.load();
    }

  private:
    void run()
    {
        while (m_running.load()) {
            pollfd descriptor{};
            descriptor.fd = m_fd;
            descriptor.events = POLLIN;

            const int result = ::poll(&descriptor, 1, 100);
            if (result < 0) {
                if (errno == EINTR)
                    continue;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (result == 0)
                continue;

            if ((descriptor.revents & POLLNVAL) != 0)
                break;

            // A UDP socket can report POLLERR after an ICMP unreachable
            // response. Clear the pending error with SO_ERROR so that poll
            // does not spin; this does not consume a queued datagram.
            if ((descriptor.revents & POLLERR) != 0) {
                int socket_error = 0;
                socklen_t error_length = sizeof(socket_error);
                ::getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length);
            }

            if ((descriptor.revents & POLLIN) == 0)
                continue;

            for (;;) {
                std::uint8_t buffer[2048];
                sockaddr_storage source{};
                socklen_t source_length = sizeof(source);
                const ssize_t received = ::recvfrom(m_fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&source), &source_length);

                if (received < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    break;
                }
                if (received == 0)
                    continue;

                m_datagrams.fetch_add(1);

                ArtDmxPacket packet;
                if (decode_artdmx(buffer, static_cast<std::size_t>(received), packet) != DecodeError::none) {
                    m_malformed.fetch_add(1);
                    continue;
                }

                std::vector<std::shared_ptr<ReceiverSubscription>> subscriptions;
                {
                    std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
                    subscriptions = m_subscriptions;
                }

                for (const auto& subscription : subscriptions) {
                    if (subscription && subscription->state && subscription->state->accepts(packet.port_address))
                        subscription->state->apply(packet);
                }
            }
        }
    }

    std::uint16_t m_port{0};
    int m_fd{-1};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::mutex m_subscriptions_mutex;
    std::vector<std::shared_ptr<ReceiverSubscription>> m_subscriptions;
    std::atomic<std::uint64_t> m_datagrams{0};
    std::atomic<std::uint64_t> m_malformed{0};
};

/// Registry of UDP listeners, keyed by local port, for this module.
class ReceiverNetworkService
{
  public:
    static ReceiverNetworkService& instance()
    {
        static ReceiverNetworkService service;
        return service;
    }

    std::shared_ptr<ReceiverSubscription> subscribe(std::uint16_t port, std::shared_ptr<ReceiverState> state, std::string& error)
    {
        error.clear();
        if (!state) {
            error = "receiver state is null";
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        std::shared_ptr<ReceiverPortListener> listener;
        const auto existing = m_listeners.find(port);
        if (existing == m_listeners.end()) {
            listener = std::make_shared<ReceiverPortListener>(port);
            std::string listener_error;
            if (!listener->start(listener_error)) {
                error = listener_error;
                return nullptr;
            }
            m_listeners[port] = listener;
        }
        else {
            listener = existing->second;
        }

        auto subscription = std::make_shared<ReceiverSubscription>(std::move(state), port);
        listener->add_subscription(subscription);
        return subscription;
    }

    void unsubscribe(std::uint16_t port, const std::shared_ptr<ReceiverSubscription>& subscription)
    {
        std::shared_ptr<ReceiverPortListener> listener;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto existing = m_listeners.find(port);
            if (existing == m_listeners.end())
                return;

            existing->second->remove_subscription(subscription);
            if (existing->second->empty()) {
                listener = existing->second;
                m_listeners.erase(existing);
            }
        }

        if (listener)
            listener->stop();
    }

    /// Exposed for tests.
    std::size_t listener_count()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_listeners.size();
    }

    std::shared_ptr<ReceiverPortListener> listener_for_port(std::uint16_t port)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto existing = m_listeners.find(port);
        return existing == m_listeners.end() ? nullptr : existing->second;
    }

  private:
    ReceiverNetworkService() = default;

    ~ReceiverNetworkService()
    {
        std::vector<std::shared_ptr<ReceiverPortListener>> listeners;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& entry : m_listeners)
                listeners.push_back(entry.second);
            m_listeners.clear();
        }
        for (auto& listener : listeners)
            listener->stop();
    }

    std::mutex m_mutex;
    std::map<std::uint16_t, std::shared_ptr<ReceiverPortListener>> m_listeners;
};

} // namespace flux::artnet
