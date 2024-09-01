// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <list>
#include <vector>
#include <string>
#include <mutex>
#include <thread>

#include <nlohmann/json.hpp>

#include <tlCore/Vector.h>

#include <tlTimeline/Player.h>

#include <opentime/rationalTime.h>
#include <opentime/timeRange.h>
namespace otime = opentime::OPENTIME_VERSION;

#ifdef MRV2_NETWORK
#    include <Poco/Net/StreamSocket.h>
#endif

#include "mrvNetwork/mrvMessage.h"

namespace mrv
{

    std::string ipToHostname(const std::string& ip);

    class TCP
    {

    public:
        TCP();
        TCP(const std::string& host, int16_t port);
        virtual ~TCP();

        bool running() const { return m_running; }

        virtual void stop();

        inline bool hasSend() const { return !m_send.empty(); }
        virtual bool hasReceive() const { return !m_receive.empty(); }

        virtual void pushMessage(const Message& message);

        void pushMessage(const std::string& command, bool value);
        void pushMessage(const std::string& command, int8_t value);
        void pushMessage(const std::string& command, int16_t value);
        void pushMessage(const std::string& command, int32_t value);
        void pushMessage(const std::string& command, int64_t value);
        void pushMessage(const std::string& command, float value);
        void pushMessage(const std::string& command, double value);
        void pushMessage(const std::string& command, const std::string& value);
        void pushMessage(
            const std::string& command, const tl::math::Vector2i& value);
        void pushMessage(
            const std::string& command, const otime::RationalTime& value);
        void
        pushMessage(const std::string& command, const otime::TimeRange& value);

        void lock() { m_lock = true; }
        void unlock() { m_lock = false; }
        bool isLocked() { return m_lock == true; }

        virtual Message popMessage();

        void close();

        inline size_t numReceive() const { return m_receive.size(); };

    protected:
        virtual void sendMessages() = 0;
        virtual void receiveMessages() = 0;

        Message receiveMessage();

    protected:
#ifdef MRV2_NETWORK
        Poco::Net::StreamSocket m_socket;
        Poco::Net::SocketAddress m_address; // Example server/client address
#endif
        volatile bool m_running = false;

        bool m_lock = false;

        bool m_isClient = false;

        std::vector< std::thread* > m_threads;
        std::mutex m_sendMutex;
        std::list< Message > m_send;

        static std::mutex m_receiveMutex;
        static std::list< Message > m_receive;

        std::vector< uint8_t > m_buffer;
    };

    extern TCP* tcp;
} // namespace mrv
