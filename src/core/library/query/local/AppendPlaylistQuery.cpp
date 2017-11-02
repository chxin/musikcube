//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include "AppendPlaylistQuery.h"

#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;

static std::string INSERT_PLAYLIST_TRACK_QUERY =
    "INSERT INTO playlist_tracks (track_external_id, source_id, playlist_id, sort_order) "
    "VALUES (?, ?, ?, ?)";

static std::string UPDATE_OFFSET_QUERY =
    "UPDATE playlist_tracks SET offset = offset + ? WHERE playlist_id = ? AND offset >= ?";

static std::string GET_MAX_SORT_ORDER_QUERY =
    "SELECT MAX(sort_order) from playlist_tracks where playlist_id = ?";

AppendPlaylistQuery::AppendPlaylistQuery(
    const int64_t playlistId,
    std::shared_ptr<musik::core::TrackList> tracks,
    const int offset)
: tracks(tracks)
, playlistId(playlistId)
, offset(offset) {

}

bool AppendPlaylistQuery::OnRun(musik::core::db::Connection &db) {
    if (!tracks->Count() || playlistId == 0) {
        return true;
    }

    ScopedTransaction transaction(db);

    int offset = this->offset;

    if (this->offset < 0) {
        /* get the max offset, we're appending to the end and don't want
        to screw up our sort order! */
        Statement queryMax(GET_MAX_SORT_ORDER_QUERY.c_str(), db);
        queryMax.BindInt64(0, playlistId);
        if (queryMax.Step() == db::Row) {
            offset = queryMax.ColumnInt32(0);
        }
    }

    {
        Statement updateOffsets(UPDATE_OFFSET_QUERY.c_str(), db);
        updateOffsets.BindInt32(0, offset);
        updateOffsets.BindInt64(1, playlistId);
        updateOffsets.BindInt32(2, offset);

        if (updateOffsets.Step() == db::Error) {
            return false;
        }
    }

    Statement insertTrack(INSERT_PLAYLIST_TRACK_QUERY.c_str(), db);

    for (size_t i = 0; i < this->tracks->Count(); i++) {
        auto track = this->tracks->Get(i);
        insertTrack.Reset();
        insertTrack.BindText(0, track->GetString("external_id"));
        insertTrack.BindText(1, track->GetString("source_id"));
        insertTrack.BindInt64(2, playlistId);
        insertTrack.BindInt32(3, offset++);

        if (insertTrack.Step() == db::Error) {
            return false;
        }
    }

    transaction.CommitAndRestart();

    return false;
}