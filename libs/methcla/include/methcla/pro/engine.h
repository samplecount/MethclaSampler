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

#ifndef METHCLA_PRO_ENGINE_H_INCLUDED
#define METHCLA_PRO_ENGINE_H_INCLUDED

#include <methcla/common.h>
#include <methcla/file.h>

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

//* Offline processing.
METHCLA_EXPORT Methcla_Error methcla_engine_render(
    const Methcla_OSCPacket* options,
    const char* inputFile,
    const char* outputFile,
    Methcla_SoundFileType outputFileType,
    Methcla_SoundFileFormat outputFileFormat,
    const char* commandFile
    );

#if defined(__cplusplus)
}
#endif

#endif /* METHCLA_PRO_ENGINE_H_INCLUDED */
