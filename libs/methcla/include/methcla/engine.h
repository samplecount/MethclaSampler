/*
    Copyright 2012-2013 Samplecount S.L.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef METHCLA_ENGINE_H_INCLUDED
#define METHCLA_ENGINE_H_INCLUDED

#include <methcla/common.h>
#include <methcla/file.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

//* An integral type for uniquely identifying requests sent to the engine.
typedef int32_t Methcla_RequestId;

enum
{
    //* Request id reserved for asynchronous notifications.
    //  Clients should not use this id when sending requests to the engine.
    kMethcla_Notification = 0
};

//* Callback function type for handling OSC packets coming from the engine.
//  Packets can be either responses to previously issued requests, or, if request_id is equal to kMethcla_Notification, an asynchronous notification.
typedef void (*Methcla_PacketHandler)(void* handler_data, Methcla_RequestId request_id, const void* packet, size_t size);

//* Abstract type for the sound engine.
typedef struct Methcla_Engine Methcla_Engine;

//* Create a new engine with the given packet handling closure and options.
METHCLA_EXPORT Methcla_Error methcla_engine_new(
    Methcla_PacketHandler handler,
    void* handler_data,
    const Methcla_OSCPacket* options,
    Methcla_Engine** engine
    );

//* Free the resources associated with engine.
//
//  Dereferencing engine after this function returns results in undefined behavior.
METHCLA_EXPORT void methcla_engine_free(Methcla_Engine* engine);

//* Return the last error code.
// METHCLA_EXPORT Methcla_Error methcla_engine_error(const Methcla_Engine* engine);

//* Start the engine.
METHCLA_EXPORT Methcla_Error methcla_engine_start(Methcla_Engine* engine);

//* Stop the engine.
METHCLA_EXPORT Methcla_Error methcla_engine_stop(Methcla_Engine* engine);

//* Send an OSC packet to the engine.
METHCLA_EXPORT Methcla_Error methcla_engine_send(Methcla_Engine* engine, const void* packet, size_t size);

METHCLA_EXPORT Methcla_Error methcla_engine_register_soundfile_api(Methcla_Engine* engine, const char* mimeType, const Methcla_SoundFileAPI* api);

#if defined(__cplusplus)
}
#endif

#endif /* METHCLA_ENGINE_H_INCLUDED */
