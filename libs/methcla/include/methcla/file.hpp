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

#ifndef METHCLA_FILE_HPP_INCLUDED
#define METHCLA_FILE_HPP_INCLUDED

#include <methcla/engine.hpp>
#include <methcla/file.h>

namespace Methcla
{
    class SoundFile
    {
        Methcla_SoundFile* m_file;
        Methcla_SoundFileInfo m_info;

    public:
        SoundFile()
            : m_file(nullptr)
        {
            memset(&m_info, 0, sizeof(m_info));
        }

        SoundFile(Methcla_SoundFile* file, const Methcla_SoundFileInfo& info)
            : m_file(file)
            , m_info(info)
        { }

        SoundFile(const Engine& engine, const std::string& path)
        {
            detail::checkReturnCode(
                methcla_engine_soundfile_open(engine, path.c_str(), kMethcla_FileModeRead, &m_file, &m_info)
            );
        }

        SoundFile(const Engine& engine, const std::string& path, const Methcla_SoundFileInfo& info)
            : m_info(info)
        {
            detail::checkReturnCode(
                methcla_engine_soundfile_open(engine, path.c_str(), kMethcla_FileModeWrite, &m_file, &m_info)
            );
        }

        SoundFile(const SoundFile&) = delete;
        SoundFile& operator=(const SoundFile&) = delete;

        ~SoundFile()
        {
            detail::checkReturnCode(methcla_soundfile_close(m_file));
        }

        const Methcla_SoundFileInfo& info() const
        {
            return m_info;
        }

        void seek(int64_t numFrames)
        {
            detail::checkReturnCode(methcla_soundfile_seek(m_file, numFrames));
        }

        int64_t tell()
        {
            int64_t numFrames;
            detail::checkReturnCode(methcla_soundfile_tell(m_file, &numFrames));
            return numFrames;
        }

        size_t read(float* buffer, size_t numFrames)
        {
            size_t outNumFrames;
            detail::checkReturnCode(methcla_soundfile_read_float(m_file, buffer, numFrames, &outNumFrames));
            return outNumFrames;
        }

        size_t write(const float* buffer, size_t numFrames)
        {
            size_t outNumFrames;
            detail::checkReturnCode(methcla_soundfile_write_float(m_file, buffer, numFrames, &outNumFrames));
            return outNumFrames;
        }
    };
}

#endif // METHCLA_FILE_HPP_INCLUDED