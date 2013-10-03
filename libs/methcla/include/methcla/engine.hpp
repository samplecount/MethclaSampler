// Copyright 2012-2013 Samplecount S.L.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef METHCLA_ENGINE_HPP_INCLUDED
#define METHCLA_ENGINE_HPP_INCLUDED

#include <methcla/engine.h>
#include <methcla/plugin.h>

#include <exception>
#include <future>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <oscpp/client.hpp>
#include <oscpp/server.hpp>
#include <oscpp/print.hpp>

namespace Methcla
{
    inline static void dumpRequest(std::ostream& out, const OSCPP::Client::Packet& packet)
    {
        out << "Request (send): " << packet << std::endl;
    }

#if 0
    inline static std::exception_ptr responseToException(const OSCPP::Server::Packet& packet)
    {
        if (packet.isMessage()) {
            OSCPP::Server::Message msg(packet);
            if (msg == "/error") {
                auto args(msg.args());
                args.drop(); // request id
                const char* error = args.string();
                return std::make_exception_ptr(std::runtime_error(error));
            } else {
                return std::exception_ptr();
            }
        } else {
            return std::make_exception_ptr(std::invalid_argument("Response is not a message"));
        }
    }
#endif

    namespace detail
    {
        template <class D, typename T> class Id
        {
        public:
            explicit Id(T id)
                : m_id(id)
            { }
            Id(const D& other)
                : m_id(other.m_id)
            { }

            T id() const
            {
                return m_id;
            }

            bool operator==(const D& other) const
            {
                return m_id == other.m_id;
            }

            bool operator!=(const D& other) const
            {
                return m_id != other.m_id;
            }

        private:
            T m_id;
        };
    };

    class NodeId : public detail::Id<NodeId,int32_t>
    {
    public:
        NodeId(int32_t id)
            : Id<NodeId,int32_t>(id)
        { }
        NodeId()
            : NodeId(-1)
        { }
    };

    class GroupId : public NodeId
    {
    public:
        // Inheriting constructors not supported by clang 3.2
        // using NodeId::NodeId;
        GroupId(int32_t id)
            : NodeId(id)
        { }
        GroupId()
            : NodeId()
        { }
    };

    class SynthId : public NodeId
    {
    public:
        SynthId(int32_t id)
            : NodeId(id)
        { }
        SynthId()
            : NodeId()
        { }
    };

    class AudioBusId : public detail::Id<AudioBusId,int32_t>
    {
    public:
        AudioBusId(int32_t id)
            : Id<AudioBusId,int32_t>(id)
        { }
        AudioBusId()
            : AudioBusId(0)
        { }
    };

    enum BusMappingFlags
    {
        kBusMappingInternal = kMethcla_BusMappingInternal,
        kBusMappingExternal = kMethcla_BusMappingExternal,
        kBusMappingFeedback = kMethcla_BusMappingFeedback,
        kBusMappingReplace  = kMethcla_BusMappingReplace
    };

    template <typename T> class ResourceIdAllocator
    {
    public:
        ResourceIdAllocator(T minValue, size_t n)
            : m_offset(minValue)
            , m_bits(n)
            , m_pos(0)
        { }

        T alloc()
        {
            for (size_t i=m_pos; i < m_bits.size(); i++) {
                if (!m_bits[i]) {
                    m_bits[i] = true;
                    m_pos = (i+1) == m_bits.size() ? 0 : i+1;
                    return T(m_offset + i);
                }
            }
            for (size_t i=0; i < m_pos; i++) {
                if (!m_bits[i]) {
                    m_bits[i] = true;
                    m_pos = i+1;
                    return T(m_offset + i);
                }
            }
            throw std::runtime_error("No free ids");
        }

        void free(T id)
        {
            T i = id - m_offset;
            if ((i >= 0) && (i < (T)m_bits.size())) {
                m_bits[i] = false;
            } else {
                throw std::runtime_error("Invalid id");
            }
        }

    private:
        T                 m_offset;
        std::vector<bool> m_bits;
        size_t            m_pos;
    };

    class PacketPool
    {
    public:
        PacketPool(const PacketPool&) = delete;
        PacketPool& operator=(const PacketPool&) = delete;

        PacketPool(size_t packetSize)
            : m_packetSize(packetSize)
        { }
        ~PacketPool()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (!m_freeList.empty()) {
                void* ptr = m_freeList.front();
                delete [] (char*)ptr;
                m_freeList.pop_front();
            }
        }

        size_t packetSize() const
        {
            return m_packetSize;
        }

        void* alloc()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_freeList.empty())
                return new char[m_packetSize];
            void* result = m_freeList.back();
            m_freeList.pop_back();
            return result;
        }

        void free(void* ptr)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_freeList.push_back(ptr);
        }

    private:
        size_t m_packetSize;
        // TODO: Use boost::lockfree::queue for free list
        std::list<void*> m_freeList;
        std::mutex m_mutex;
    };

    class Packet
    {
    public:
        Packet(PacketPool& pool)
            : m_pool(pool)
            , m_packet(pool.alloc(), pool.packetSize())
        { }
        ~Packet()
        {
            m_pool.free(m_packet.data());
        }

        Packet(const Packet&) = delete;
        Packet& operator=(const Packet&) = delete;

        const OSCPP::Client::Packet& packet() const
        {
            return m_packet;
        }

        OSCPP::Client::Packet& packet()
        {
            return m_packet;
        }

    private:
        PacketPool&             m_pool;
        OSCPP::Client::Packet   m_packet;
    };

#if 0
    namespace detail
    {
        struct Result
        {
            Result()
                : m_cond(false)
            { }
            Result(const Result&) = delete;
            Result& operator=(const Result&) = delete;

            inline void notify()
            {
                m_cond = true;
                m_cond_var.notify_one();
            }

            inline void wait()
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                while (!m_cond) {
                    m_cond_var.wait(lock);
                }
                if (m_exc) {
                    std::rethrow_exception(m_exc);
                }
            }

            void set_exception(std::exception_ptr exc)
            {
                BOOST_ASSERT(!m_cond);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_exc = exc;
                notify();
            }

            std::mutex m_mutex;
            std::condition_variable m_cond_var;
            bool m_cond;
            std::exception_ptr m_exc;
        };
    };

    template <class T> class Result : detail::Result
    {
    public:
        void set(std::exception_ptr exc)
        {
            set_exception(exc);
        }

        void set(const T& value)
        {
            BOOST_ASSERT(!m_cond);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_value = value;
            notify();
        }

        const T& get()
        {
            wait();
            return m_value;
        }

    private:
        T m_value;
    };

    template <> class Result<void> : detail::Result
    {
    public:
        void set(std::exception_ptr exc)
        {
            set_exception(exc);
        }

        void set()
        {
            BOOST_ASSERT(!m_cond);
            std::lock_guard<std::mutex> lock(m_mutex);
            notify();
        }

        void get()
        {
            wait();
        }
    };

    template <typename T> bool checkResponse(const OSCPP::Server::Packet& response, Result<T>& result)
    {
        auto error = responseToException(response);
        if (error) {
            result.set(error);
            return false;
        }
        return true;
    }
#endif

    class Value
    {
    public:
        enum Type
        {
            kInt,
            kFloat,
            kString
        };

        explicit Value(int x)
            : m_type(kInt)
            , m_int(x)
        { }
        explicit Value(float x)
            : m_type(kFloat)
            , m_float(x)
        { }
        Value(const std::string& x)
            : m_type(kString)
            , m_string(x)
        { }

        explicit Value(bool x)
            : m_type(kInt)
            , m_int(x)
        { }

        void put(OSCPP::Client::Packet& packet) const
        {
            switch (m_type) {
                case kInt:
                    packet.int32(m_int);
                    break;
                case kFloat:
                    packet.float32(m_float);
                    break;
                case kString:
                    packet.string(m_string.c_str());
                    break;
            }
        }

    private:
        Type m_type;
        int m_int;
        float m_float;
        std::string m_string;
    };

    class Option
    {
    public:
        virtual ~Option() { }
        virtual void put(OSCPP::Client::Packet& packet) const = 0;

        inline static std::shared_ptr<Option> pluginLibrary(Methcla_LibraryFunction f);
        inline static std::shared_ptr<Option> driverBufferSize(int32_t bufferSize);
    };

    class ValueOption : public Option
    {
    public:
        ValueOption(const char* key, Value value)
            : m_key(key)
            , m_value(value)
        { }

        virtual void put(OSCPP::Client::Packet& packet) const override
        {
            packet.openMessage(m_key.c_str(), 1);
            m_value.put(packet);
            packet.closeMessage();
        }

    private:
        std::string m_key;
        Value       m_value;
    };

    template <typename T> class BlobOption : public Option
    {
        static_assert(std::is_pod<T>::value, "Value type must be POD");

    public:
        BlobOption(const char* key, const T& value)
            : m_key(key)
            , m_value(value)
        { }

        virtual void put(OSCPP::Client::Packet& packet) const override
        {
            packet
                .openMessage(m_key.c_str(), 1)
                .blob(OSCPP::Blob(&m_value, sizeof(m_value)))
                .closeMessage();
        }

    private:
        std::string m_key;
        T m_value;
    };

    std::shared_ptr<Option> Option::pluginLibrary(Methcla_LibraryFunction f)
    {
        return std::make_shared<BlobOption<Methcla_LibraryFunction>>("/engine/option/plugin-library", f);
    }

    std::shared_ptr<Option> Option::driverBufferSize(int32_t bufferSize)
    {
        return std::make_shared<ValueOption>("/engine/option/driver/buffer-size", Value(bufferSize));
    }

    typedef std::vector<std::shared_ptr<Option>> Options;

    static const Methcla_Time immediately = 0.;

    class Engine
    {
    public:
        Engine(const Options& options)
            : m_nodeIds(1, 1023)
            , m_requestId(kMethcla_Notification+1)
            , m_packets(8192)
        {
            OSCPP::Client::DynamicPacket bundle(8192);
            bundle.openBundle(1);
            for (auto option : options) {
                option->put(bundle);
            }
            bundle.closeBundle();
            const Methcla_OSCPacket packet = { .data = bundle.data(), .size = bundle.size() };
            checkReturnCode(methcla_engine_new(handlePacket, this, &packet, &m_engine));
        }
        ~Engine()
        {
            methcla_engine_free(m_engine);
        }

        operator const Methcla_Engine* () const
        {
            return m_engine;
        }

        operator Methcla_Engine* ()
        {
            return m_engine;
        }

        void start()
        {
            checkReturnCode(methcla_engine_start(m_engine));
        }

        void stop()
        {
            checkReturnCode(methcla_engine_stop(m_engine));
        }

        Methcla_Time currentTime() const
        {
            return methcla_engine_current_time(m_engine);
        }

        GroupId root() const
        {
            return GroupId(0);
        }

        friend class Request;

        class Request
        {
            Engine&                 m_engine;
            std::shared_ptr<Packet> m_request;

        protected:
            Engine& engine()
            {
                return m_engine;
            }

            std::shared_ptr<Packet> packet()
            {
                return m_request;
            }

            OSCPP::Client::Packet& oscPacket()
            {
                return packet()->packet();
            }

        public:
            Request(Engine& engine, std::shared_ptr<Packet> request)
                : m_engine(engine)
                , m_request(request)
            { }
            Request(Engine& engine)
                : Request(engine, std::make_shared<Packet>(engine.packets()))
            { }
            Request(Engine* engine)
                : Request(*engine)
            { }

            //* Send request to the engine.
            virtual void send()
            {
                m_engine.send(oscPacket());
            }

            GroupId group(GroupId parent)
            {
                const int32_t nodeId = m_engine.m_nodeIds.alloc();

                oscPacket()
                    .openMessage("/group/new", 3)
                        .int32(nodeId)
                        .int32(parent.id())
                        .int32(0) // add action
                    .closeMessage();

                return GroupId(nodeId);
            }

            SynthId synth(const char* synthDef, GroupId parent, const std::vector<float>& controls, const std::list<Value>& options=std::list<Value>())
            {
                const int32_t nodeId = m_engine.m_nodeIds.alloc();

                oscPacket()
                    .openMessage("/synth/new", 4 + OSCPP::Tags::array(controls.size()) + OSCPP::Tags::array(options.size()))
                        .string(synthDef)
                        .int32(nodeId)
                        .int32(parent.id())
                        .int32(0)
                        .putArray(controls.begin(), controls.end());

                        oscPacket().openArray();
                            for (const auto& x : options) {
                                x.put(oscPacket());
                            }
                        oscPacket().closeArray();

                    oscPacket().closeMessage();

                return SynthId(nodeId);
            }

            void activate(SynthId synth)
            {
                oscPacket()
                    .openMessage("/synth/activate", 1)
                    .int32(synth.id())
                    .closeMessage();
            }

            void mapInput(SynthId synth, size_t index, AudioBusId bus, BusMappingFlags flags=kBusMappingInternal)
            {
                oscPacket()
                    .openMessage("/synth/map/input", 4)
                        .int32(synth.id())
                        .int32(index)
                        .int32(bus.id())
                        .int32(flags)
                    .closeMessage();
            }

            void mapOutput(SynthId synth, size_t index, AudioBusId bus, BusMappingFlags flags=kBusMappingInternal)
            {
                oscPacket()
                    .openMessage("/synth/map/output", 4)
                        .int32(synth.id())
                        .int32(index)
                        .int32(bus.id())
                        .int32(flags)
                    .closeMessage();
            }

            void set(NodeId node, size_t index, double value)
            {
                oscPacket()
                    .openMessage("/node/set", 3)
                        .int32(node.id())
                        .int32(index)
                        .float32(value)
                    .closeMessage();
            }

            void free(NodeId node)
            {
                oscPacket()
                    .openMessage("/node/free", 1)
                    .int32(node.id())
                    .closeMessage();
                m_engine.m_nodeIds.free(node.id());
            }
        };

        class Bundle : public Request
        {
            struct Flags
            {
                bool isFinished : 1;
                bool isInner  : 1;
            };

            Flags m_flags;

        public:
            Bundle(Engine& engine, Methcla_Time time=immediately)
                : Request(engine)
            {
                m_flags.isFinished = false;
                m_flags.isInner = false;
                oscPacket().openBundle(methcla_time_to_uint64(time));
            }
            Bundle(Engine* engine, Methcla_Time time=immediately)
                : Bundle(*engine, time)
            { }

            Bundle(Bundle& other, Methcla_Time time=immediately)
                : Request(other.engine(), other.packet())
            {
                m_flags.isFinished = false;
                m_flags.isInner = true;
                oscPacket().openBundle(methcla_time_to_uint64(time));
            }

            void close()
            {
                if (!m_flags.isFinished)
                {
                    oscPacket().closeBundle();
                    m_flags.isFinished = true;
                }
            }

            void bundle(Methcla_Time time, std::function<void(Bundle&)> func)
            {
                Bundle bundle(*this, time);
                func(bundle);
                close();
            }

            //* Finalize request and send resulting bundle to the engine.
            void send() override
            {
                if (m_flags.isInner)
                    throw std::runtime_error("Cannot send nested bundle");
                close();
                Request::send();
            }
        };

        void bundle(Methcla_Time time, std::function<void(Bundle&)> func)
        {
            Bundle bundle(*this, time);
            func(bundle);
            bundle.send();
        }

        GroupId group(GroupId parent)
        {
            Request request(this);
            GroupId result = request.group(parent);
            request.send();
            return result;
        }

        SynthId synth(const char* synthDef, GroupId parent, const std::vector<float>& controls, const std::list<Value>& options=std::list<Value>())
        {
            Request request(this);
            SynthId result = request.synth(synthDef, parent, controls, options);
            request.send();
            return result;
        }

        void activate(SynthId synth)
        {
            Request request(this);
            request.activate(synth);
            request.send();
        }

        void mapInput(SynthId synth, size_t index, AudioBusId bus, BusMappingFlags flags=kBusMappingInternal)
        {
            Request request(this);
            request.mapInput(synth, index, bus, flags);
            request.send();
        }

        void mapOutput(SynthId synth, size_t index, AudioBusId bus, BusMappingFlags flags=kBusMappingInternal)
        {
            Request request(this);
            request.mapOutput(synth, index, bus, flags);
            request.send();
        }

        void set(NodeId node, size_t index, double value)
        {
            Request request(this);
            request.set(node, index, value);
            request.send();
        }

        void free(NodeId node)
        {
            Request request(this);
            request.free(node);
            request.send();
        }

        PacketPool& packets()
        {
            return m_packets;
        }

    private:
        static void checkReturnCode(Methcla_Error err)
        {
            if (err != kMethcla_NoError) {
                const char* msg = methcla_error_message(err);
                if (err == kMethcla_ArgumentError) {
                    throw std::invalid_argument(msg);
                } else if (err == kMethcla_LogicError) {
                    throw std::logic_error(msg);
                } else if (err == kMethcla_MemoryError) {
                    throw std::bad_alloc();
                } else {
                    throw std::runtime_error(msg);
                }
            }
        }

        static void handlePacket(void* data, Methcla_RequestId requestId, const void* packet, size_t size)
        {
            if (requestId == kMethcla_Notification)
                static_cast<Engine*>(data)->handleNotification(packet, size);
            else
                static_cast<Engine*>(data)->handleReply(requestId, packet, size);
        }

        void handleNotification(const void* packet, size_t size)
        {
        }

        void handleReply(Methcla_RequestId requestId, const void* packet, size_t size)
        {
            std::lock_guard<std::mutex> lock(m_callbacksMutex);
            // look up request id and invoke callback
            auto it = m_callbacks.find(requestId);
            if (it != m_callbacks.end()) {
                try {
                    it->second(requestId, packet, size);
                    m_callbacks.erase(it);
                } catch (...) {
                    m_callbacks.erase(it);
                    throw;
                }
            }
        }

        void send(const void* packet, size_t size)
        {
            checkReturnCode(methcla_engine_send(m_engine, packet, size));
        }

        void send(const OSCPP::Client::Packet& packet)
        {
            // dumpRequest(std::cout, packet);
            send(packet.data(), packet.size());
        }

        void send(const Packet& packet)
        {
            send(packet.packet());
        }

        Methcla_RequestId getRequestId()
        {
            std::lock_guard<std::mutex> lock(m_requestIdMutex);
            Methcla_RequestId result = m_requestId;
            if (result == kMethcla_Notification) {
                result++;
            }
            m_requestId = result + 1;
            return result;
        }

        void registerResponse(Methcla_RequestId requestId, std::function<void (Methcla_RequestId, const void*, size_t)> callback)
        {
            std::lock_guard<std::mutex> lock(m_callbacksMutex);
            if (m_callbacks.find(requestId) != m_callbacks.end()) {
                throw std::logic_error("Duplicate request id");
            }
            m_callbacks[requestId] = callback;
        }

#if 0
        void withRequest(Methcla_RequestId requestId, const OSCPP::Client::Packet& request, std::function<void (Methcla_RequestId, const void*, size_t)> callback)
        {
            registerResponse(requestId, callback);
            send(request);
        }

        void execRequest(Methcla_RequestId requestId, const OSCPP::Client::Packet& request)
        {
            Result<void> result;
            withRequest(requestId, request, [&result](Methcla_RequestId, const void* buffer, size_t size){
                if (checkResponse(OSCPP::Server::Packet(buffer, size), result)) {
                    result.set();
                }
            });
            result.get();
        }
#endif

    private:
        Methcla_Engine*     m_engine;
        ResourceIdAllocator<int32_t> m_nodeIds;
        Methcla_RequestId   m_requestId;
        std::mutex          m_requestIdMutex;
        std::unordered_map<Methcla_RequestId,std::function<void (Methcla_RequestId, const void*, size_t)>> m_callbacks;
        std::mutex          m_callbacksMutex;
        PacketPool          m_packets;
    };
};

#endif // METHCLA_ENGINE_HPP_INCLUDED
