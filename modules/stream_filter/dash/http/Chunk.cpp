/*
 * Chunk.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Chunk.h"

using namespace dash::http;

Chunk::Chunk        () :
       startByte    (0),
       endByte      (0),
       bitrate      (1),
       port         (0),
       isHostname   (false),
       length       (0),
       bytesRead    (0),
       bytesToRead  (0),
       connection   (NULL)
{
}

size_t              Chunk::getEndByte           () const
{
    return endByte;
}
size_t              Chunk::getStartByte         () const
{
    return startByte;
}
const std::string&  Chunk::getUrl               () const
{
    return url;
}
void                Chunk::setEndByte           (size_t endByte)
{
    this->endByte = endByte;
    if (endByte > startByte)
        bytesToRead = endByte - startByte;
}
void                Chunk::setStartByte         (size_t startByte)
{
    this->startByte = startByte;
    if (endByte > startByte)
        bytesToRead = endByte - startByte;
}
void                Chunk::setUrl               (const std::string& url )
{
    this->url = url;

    if(this->url.compare(0, 4, "http"))
    {
        this->isHostname = false;
        return;
    }

    vlc_url_t url_components;
    vlc_UrlParse(&url_components, url.c_str(), 0);

    this->path          = url_components.psz_path;
    this->port          = url_components.i_port ? url_components.i_port : 80;
    this->hostname      = url_components.psz_host;
    this->isHostname    = true;

    vlc_UrlClean(&url_components);
}
void                Chunk::addOptionalUrl       (const std::string& url)
{
    this->optionalUrls.push_back(url);
}
bool                Chunk::usesByteRange        () const
{
    return (startByte != endByte);
}

void                Chunk::setBitrate           (uint64_t bitrate)
{
    this->bitrate = bitrate;
}
int                 Chunk::getBitrate           ()
{
    return this->bitrate;
}
bool                Chunk::hasHostname          () const
{
    return this->isHostname;
}
const std::string&  Chunk::getHostname          () const
{
    return this->hostname;
}
const std::string&  Chunk::getPath              () const
{
    return this->path;
}
int                 Chunk::getPort              () const
{
    return this->port;
}
uint64_t            Chunk::getLength            () const
{
    return this->length;
}
void                Chunk::setLength            (uint64_t length)
{
    this->length = length;
}
uint64_t            Chunk::getBytesRead         () const
{
    return this->bytesRead;
}
void                Chunk::setBytesRead         (uint64_t bytes)
{
    this->bytesRead = bytes;
}

void                Chunk::setBytesToRead         (uint64_t bytes)
{
    bytesToRead = bytes;
}

uint64_t            Chunk::getBytesToRead       () const
{
        return length - bytesRead;
}

size_t              Chunk::getPercentDownloaded () const
{
    return (size_t)(((float)this->bytesRead / this->length) * 100);
}
HTTPConnection*     Chunk::getConnection           () const
{
    return this->connection;
}
void                Chunk::setConnection   (HTTPConnection *connection)
{
    this->connection = connection;
}
