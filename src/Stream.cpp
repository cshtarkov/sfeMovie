
/*
 *  Stream.cpp
 *  sfeMovie project
 *
 *  Copyright (C) 2010-2015 Lucas Soltic
 *  lucas.soltic@orange.fr
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "Stream.hpp"
#include "Utilities.hpp"
#include "TimerPriorities.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace sfe
{
    std::string Stream::AVStreamDescription(AVStream* stream)
    {
        return std::string("'" + std::string(av_get_media_type_string(stream->codecpar->codec_type))
                           + "/" + avcodec_get_name(stream->codecpar->codec_id) + "' stream @ " + s(stream));
    }
    
    Stream::Stream(AVFormatContext*& formatCtx, AVStream*& stream, DataSource& dataSource, std::shared_ptr<Timer> timer) :
    m_formatCtx(formatCtx),
    m_stream(stream),
    m_dataSource(dataSource),
    m_timer(timer),
    m_codec(nullptr),
    m_context(nullptr),
    m_streamID(-1),
    m_packetList(),
    m_status(Stopped),
    m_readerMutex()
    {
        CHECK(stream, "Stream::Stream() - invalid stream argument");
        CHECK(timer, "Inconcistency error: null timer");
        int err = 0;
        
        m_stream = stream;
        m_streamID = stream->index;
        CHECK(m_stream, "Inconcistency error: null stream")
        CHECK(m_streamID >= 0, "Inconcistency error: invalid stream id");
        
        // Get the decoder
        m_codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
        CHECK(m_codec, "Stream() - no decoder for " + std::string(avcodec_get_name(m_stream->codecpar->codec_id)) + " codec");
        
        // Load the codec
        m_context = avcodec_alloc_context3(m_codec); // TODO: will leak if later steps throw
        CHECK(m_context, "Stream() - unable to allocate codec context for codec " + std::string(avcodec_get_name(m_stream->codecpar->codec_id)));
        err = avcodec_parameters_to_context(m_context, m_stream->codecpar);
        CHECK0(err, "Stream() - unable to copy codec parameters to context for codec " + std::string(avcodec_get_name(m_stream->codecpar->codec_id)));
        err = avcodec_open2(m_context, m_codec, nullptr);
        CHECK0(err, "Stream() - unable to load decoder for codec " + std::string(avcodec_get_name(m_stream->codecpar->codec_id)));
        
        AVDictionaryEntry* entry = av_dict_get(m_stream->metadata, "language", nullptr, 0);
        if (entry)
        {
            m_language = entry->value;
        }
    }
    
    Stream::~Stream()
    {
        disconnect();
        Stream::flushBuffers();
        
        if (m_formatCtx && m_stream && m_context)
        {
            avcodec_close(m_context);
        }
        else
        {
            sfeLogWarning("Stream lost connection to its codec, leaking");
        }

        avcodec_free_context(&m_context);
    }
    
    void Stream::connect()
    {
        if (isPassive())
            m_timer->addObserver(*this, PassiveStreamTimerPriority);
        else
            m_timer->addObserver(*this, ActiveStreamTimerPriority);
    }
    
    void Stream::disconnect()
    {
        m_timer->removeObserver(*this);
    }
    
    void Stream::pushEncodedData(AVPacket* packet)
    {
        CHECK(packet, "invalid argument");
        sf::Lock l(m_readerMutex);
        m_packetList.push_back(packet);
    }
    
    void Stream::prependEncodedData(AVPacket* packet)
    {
        CHECK(packet, "invalid argument");
        sf::Lock l(m_readerMutex);
        m_packetList.push_front(packet);
    }
    
    AVPacket* Stream::popEncodedData()
    {
        AVPacket* result = nullptr;
        sf::Lock l(m_readerMutex);
        
        if (m_packetList.empty() && !isPassive())
        {
            m_dataSource.requestMoreData(*this);
        }
        
        if (!m_packetList.empty())
        {
            result = m_packetList.front();
            m_packetList.pop_front();
        }
        else
        {
            if (m_codec->capabilities & AV_CODEC_CAP_DELAY)
            {
                AVPacket* flushPacket = (AVPacket*)av_malloc(sizeof(*flushPacket));
                av_init_packet(flushPacket);
                flushPacket->data = nullptr;
                flushPacket->size = 0;
                result = flushPacket;
                
                sfeLogDebug("Sending flush packet: " + mediaTypeToString(getStreamKind()));
            }
        }
        
        return result;
    }
    
    void Stream::flushBuffers()
    {
        sf::Lock l(m_readerMutex);
        if (getStatus() == Playing)
        {
            sfeLogWarning("packets flushed while the stream is still playing");
        }
        
        if (m_formatCtx && m_stream)
          avcodec_flush_buffers(m_context);
        
        AVPacket* pkt = nullptr;
        
        while (!m_packetList.empty())
        {
            pkt = m_packetList.front();
            m_packetList.pop_front();
            
            av_packet_unref(pkt);
        }
    }
    
    bool Stream::needsMoreData() const
    {
        return m_packetList.size() < 10;
    }
    
    MediaType Stream::getStreamKind() const
    {
        return Unknown;
    }
    
    Status Stream::getStatus() const
    {
        return m_status;
    }
    
    std::string Stream::getLanguage() const
    {
        return m_language;
    }
    
    bool Stream::computeEncodedPosition(sf::Time& position)
    {
        if (m_packetList.empty() && !isPassive())
        {
            m_dataSource.requestMoreData(*this);
        }
        
        if (! m_packetList.empty())
        {
            sf::Lock l(m_readerMutex);
            AVPacket* packet = m_packetList.front();
            CHECK(packet, "internal inconcistency");
            
            int64_t timestamp = -424242;
            
            if (packet->dts != AV_NOPTS_VALUE)
            {
                timestamp = packet->dts;
            }
            else if (packet->pts != AV_NOPTS_VALUE)
            {
                int64_t startTime = m_stream->start_time != AV_NOPTS_VALUE ? m_stream->start_time : 0;
                timestamp = packet->pts - startTime;
            }
            
            AVRational seconds = av_mul_q(av_make_q(timestamp, 1), m_stream->time_base);
            position = sf::milliseconds(1000 * av_q2d(seconds));
            return true;
        }
        
        return false;
    }
    
    sf::Time Stream::packetDuration(const AVPacket* packet) const
    {
        CHECK(packet, "inconcistency error: null packet");
        CHECK(packet->stream_index == m_streamID, "Asking for duration of a packet for a different stream!");
        
        if (packet->duration != 0)
        {
            AVRational seconds = av_mul_q(av_make_q(packet->duration, 1), m_stream->time_base);
            return sf::seconds(av_q2d(seconds));
        }
        else
        {
            return sf::seconds(1. / av_q2d(av_guess_frame_rate(m_formatCtx, m_stream, nullptr)));
        }
    }
    
    std::string Stream::description() const
    {
        return AVStreamDescription(m_stream);
    }
    
    bool Stream::canUsePacket(AVPacket* packet) const
    {
        CHECK(packet, "inconcistency error: null argument");
        
        return packet->stream_index == m_stream->index;
    }
    
    bool Stream::isPassive() const
    {
        return false;
    }
    
    void Stream::setStatus(Status status)
    {
        m_status = status;
    }
    
    void Stream::didPlay(const Timer& timer, Status previousStatus)
    {
        setStatus(Playing);
    }
    
    void Stream::didPause(const Timer& timer, Status previousStatus)
    {
        setStatus(Paused);
    }
    
    void Stream::didStop(const Timer& timer, Status previousStatus)
    {
        setStatus(Stopped);
    }
    
    bool Stream::didSeek(const Timer& timer, sf::Time oldPosition)
    {
        if (timer.getOffset() != sf::Time::Zero)
            return fastForward(timer.getOffset());
        
        return true;
    }
    
    bool Stream::hasPackets()
    {
        return !m_packetList.empty();
    }
}
