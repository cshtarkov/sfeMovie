
/*
 *  Demuxer.hpp
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

#ifndef SFEMOVIE_DEMUXER_HPP
#define SFEMOVIE_DEMUXER_HPP

#include <SFML/System.hpp>
#include "Stream.hpp"
#include "AudioStream.hpp"
#include "VideoStream.hpp"
#include "Timer.hpp"
#include <map>
#include <string>
#include <set>
#include <list>
#include <utility>
#include <memory>

namespace sfe
{
    class Demuxer : public Stream::DataSource, public Timer::Observer
    {
    public:
        /** Describes a demuxer
         *
         * Ie. an audio/video container format parser such as avi, mov, mkv, ogv... parsers
         */
        struct DemuxerInfo
        {
            std::string name;
            std::string description;
        };
        
        /** Describes a decoder
         *
         * Ie. an audio/video stream decoder for h.264, theora, vp9, mp3, pcm, srt... streams
         */
        struct DecoderInfo
        {
            std::string name;
            std::string description;
            MediaType type;
        };
        
        /** Return a list containing the names of all the demuxers (ie. container parsers) included
         * in this sfeMovie build
         */
        static const std::list<DemuxerInfo>& getAvailableDemuxers();
        
        /** Return a list containing the names of all the decoders included
         * in this sfeMovie build
         */
        static const std::list<DecoderInfo>& getAvailableDecoders();
        
        /** Default constructor
         *
         * Open a media file and find its streams
         *
         * @param sourceFile the path of the media to open and play
         * @param timer the timer with which the media streams will be synchronized
         * @param videoDelegate the delegate that will handle the images produced by the VideoStreams
         */
        Demuxer(const std::string& sourceFile, std::shared_ptr<Timer> timer, VideoStream::Delegate& videoDelegate);
        
        /** Default destructor
         */
        virtual ~Demuxer();
        
        /** Return a list of the streams found in the media
         * The map key is the index of the stream
         *
         * @return the list of streams
         */
        const std::map<int, std::shared_ptr<Stream> >& getStreams() const;
        
        /** Return a set containing all the streams found in the media that match the given type
         *
         * @param the media type against which the returned streams should be filtered
         * @return the audio streams
         */
        std::set< std::shared_ptr<Stream> > getStreamsOfType(MediaType type) const;
        
        /** Gather the required stream metadata from each stream of the given type
         *
         * @param type the type of the streams that are to be described
         * @return the stream entries computed from the gathered metadata
         */
        Streams computeStreamDescriptors(MediaType type) const;
        
        /** Enable the given audio stream and connect it to the reference timer
         *
         * If another stream of the same kind is already enabled, it is first disabled and disconnected
         * so that only one stream of the same kind can be enabled at the same time.
         *
         * @param stream the audio stream to enable and connect for playing, or nullptr to disable audio
         */
        void selectAudioStream(std::shared_ptr<AudioStream> stream);
        
        /** Enable the first found audio stream, if it exists
         *
         * @see selectAudioStream
         */
        void selectFirstAudioStream();
        
        /** Get the currently selected audio stream, if there's one
         *
         * @return the currently selected audio stream, or nullptr if there's none
         */
        std::shared_ptr<AudioStream> getSelectedAudioStream() const;
        
        /** Enable the given video stream and connect it to the reference timer
         *
         * If another stream of the same kind is already enabled, it is first disabled and disconnected
         * so that only one stream of the same kind can be enabled at the same time.
         *
         * @param stream the video stream to enable and connect for playing, or nullptr to disable video
         */
        void selectVideoStream(std::shared_ptr<VideoStream> stream);
        
        /** Enable the first found video stream, if it exists
         *
         * @see selectAudioStream
         */
        void selectFirstVideoStream();
        
        /** Get the currently selected video stream, if there's one
         *
         * @return the currently selected video stream, or nullptr if there's none
         */
        std::shared_ptr<VideoStream> getSelectedVideoStream() const;
        
        /** Read encoded data from the media and makes sure that the given stream
         * has enough data
         *
         * @param stream The stream to feed
         */
        void feedStream(Stream& stream);
        
        /** @return a list of all the active streams
         */
        std::set<std::shared_ptr<Stream>> getSelectedStreams() const;
        
        /** Update the media status and eventually decode frames
         */
        void update();
        
        /** Tell whether the demuxer has reached the end of the file and can no more feed the streams
         *
         * @return whether the end of the media file has been reached
         */
        bool didReachEndOfFile() const;
        
        /** Give the media duration
         *
         * @return the media duration
         */
        sf::Time getDuration() const;
        
    private:
        /** Read a encoded packet from the media file
         *
         * You're responsible for freeing the returned packet
         *
         * @return the read packet, or nullptr if the end of file has been reached
         */
        AVPacket* readPacket();
        
        /** Empty the temporarily encoded data queue
         */
        void flushBuffers();
        
        /** Queue a packet that has been read and is to be used by an active stream in near future
         *
         * @param packet the packet to temporarily store
         */
        void queueEncodedData(AVPacket* packet);
        
        /** Check whether data that should be distributed to the given stream is currently pending
         * in the demuxer's temporary queue
         *
         * @param stream the stream for which pending data availability is to be checked
         * @param whether pending data exists for the given stream
         */
        bool hasPendingDataForStream(const Stream& stream) const;
        
        /** Look for a queued packet for the given stream
         *
         * @param stream the stream for which to search a packet
         * @return if a packet for the given stream has been found, it is dequeued and returned
         * otherwise NULL is returned
         */
        AVPacket* gatherQueuedPacketForStream(Stream& stream);
        
        /** Distribute the given packet to the correct stream
         *
         * If the packet doesn't match any known stream, nothing is done
         *
         * @param packet the packet to distribute
         * @param stream the stream that requested data from the demuxer, if the packet is not for this stream
         * it must be queued
         * @return true if the packet could be distributed, false otherwise
         */
        bool distributePacket(AVPacket* packet, Stream& stream);
        
        /** Try to extract the media duration from the given stream
         */
        void extractDurationFromStream(const AVStream* stream);
        
        // Data source interface
        void requestMoreData(Stream& starvingStream) override;
        void resetEndOfFileStatus() override;
        
        // Timer interface
        bool didSeek(const Timer& timer, sf::Time oldPosition) override;
        
        AVFormatContext* m_formatCtx;
        bool m_eofReached;
        std::map<int, std::shared_ptr<Stream> > m_streams;
        std::map<int, std::string> m_ignoredStreams;
        mutable sf::Mutex m_synchronized;
        std::shared_ptr<Timer> m_timer;
        std::shared_ptr<Stream> m_connectedAudioStream;
        std::shared_ptr<Stream> m_connectedVideoStream;
        sf::Time m_duration;
        std::map<const Stream*, std::list<AVPacket*> > m_pendingDataForActiveStreams;
        
        static std::list<DemuxerInfo> g_availableDemuxers;
        static std::list<DecoderInfo> g_availableDecoders;
    };
}

#endif
