// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <string>
#include <vector>

#include <mrvCore/mrvString.h>

namespace mrv
{

    extern const int64_t kMinFrame;
    extern const int64_t kMaxFrame;

    struct Sequence
    {
        std::string root;
        std::string number;
        std::string view;
        std::string ext;
    };

    typedef std::vector< Sequence > SequenceList;

    struct SequenceSort
    {
        // true if a < b, else false
        bool operator()(const Sequence& a, const Sequence& b) const
        {
            if (a.root < b.root)
                return true;
            else if (a.root > b.root)
                return false;

            if (a.ext < b.ext)
                return true;
            else if (a.ext > b.ext)
                return false;

            if (a.view < b.view)
                return true;
            else if (a.view > b.view)
                return false;

            // size_t as = a.number.size();
            // size_t bs = b.number.size();
            // if ( as < bs ) return true;
            // else if ( as > bs ) return false;

            if (atoi(a.number.c_str()) < atoi(b.number.c_str()))
                return true;
            return false;
        }
    };

    /**
     * Given a filename with %hex characters, return string in ascii.
     */
    std::string hex_to_char_filename(std::string& f);

    /**
     * Given a filename of a possible sequence, split it into
     * root name, frame string, and extension
     *
     * @param root    root name of file sequence (root.)
     * @param frame   frame part of file name    (%d)
     * @param view    left or right for stereo images.  Empty if not stereo.
     * @param ext     extension of file sequence (.ext)
     * @param file    original filename, potentially part of a sequence.
     * @param change_view whether to change view for %V or %v.
     *
     * @return true if a sequence, false if not.
     */
    bool split_sequence(
        std::string& root, std::string& frame, std::string& view,
        std::string& ext, const std::string& file,
        const bool change_view = true, const bool change_frame = true);

    /**
     * Obtain the frame range of a sequence by scanning the directory where it
     * resides.
     *
     * @param firstFrame   first frame of sequence
     * @param lastFrame    last  frame of sequence
     * @param file         fileroot of sequence ( Example: mray.%04d.exr )
     * @param error        log errors on the log window
     *
     * @return true if sequence limits found, false if not.
     */
    bool get_sequence_limits(
        int64_t& firstFrame, int64_t& lastFrame, std::string& file,
        const bool error = true);

    /**
     * Given a filename extension, return whether the extension is
     * from a movie format.
     *
     * @param ext Filename extension
     *
     * @return true if a possible movie, false if not.
     */
    bool is_valid_movie(const char* ext);

    /**
     * Given a filename extension, return whether the extension is
     * from an audio format.
     *
     * @param ext Filename extension
     *
     * @return true if a possible audio file, false if not.
     */
    bool is_valid_audio(const char* ext);

    /**
     * Given a filename extension, return whether the extension is
     * from a subtitle format.
     *
     * @param ext Filename extension
     *
     * @return true if a possible subtitle file, false if not.
     */
    bool is_valid_subtitle(const char* ext);

    std::string get_short_view(bool left);
    std::string get_long_view(bool left);

    //! Parse a directory and return all movies, sequences and audios found
    //! there
    void parse_directory(
        const std::string& directory, stringArray& movies,
        stringArray& sequences, stringArray& audios);

    //! Parse a %v or %V fileroot and return the appropiate view name.
    std::string parse_view(const std::string& fileroot, bool left = true);

} // namespace mrv
