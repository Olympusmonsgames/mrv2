// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#ifdef TLRENDER_FFMPEG
#    include <tlIO/FFmpeg.h>
#endif

#ifdef TLRENDER_EXR
#    include <tlIO/OpenEXR.h>
#endif

namespace mrv
{
    enum class SaveResolution { kSameSize, kHalfSize, kQuarterSize };

    struct SaveOptions
    {
        bool annotations = false;

        SaveResolution resolution = SaveResolution::kSameSize;

#ifdef TLRENDER_FFMPEG
        tl::ffmpeg::Profile ffmpegProfile = tl::ffmpeg::Profile::None;
        std::string ffmpegPreset;
        std::string ffmpegPixelFormat = "YUV420P";
        tl::ffmpeg::AudioCodec ffmpegAudioCodec = tl::ffmpeg::AudioCodec::AAC;
        bool ffmpegHardwareEncode = false;
        bool ffmpegOverride = false;
        std::string ffmpegColorRange;
        std::string ffmpegColorSpace;
        std::string ffmpegColorPrimaries;
        std::string ffmpegColorTRC;
#endif

#ifdef TLRENDER_EXR
        tl::exr::Compression exrCompression = tl::exr::Compression::ZIP;
        tl::image::PixelType exrPixelType = tl::image::PixelType::RGBA_F16;
#endif
        int zipCompressionLevel = 4;
        float dwaCompressionLevel = 45.0F;

        bool noRename = false;
    };
} // namespace mrv
