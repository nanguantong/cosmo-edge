// RtmpCodecConfig — Rtmp Codec Config implementation.

#include "flow/stream/RtmpCodecConfig.h"

#include "util/Log.h"

namespace cosmo {

namespace {
    // Bitrate tiers by pixel count
    constexpr int kPixels1080p = 1920 * 1080;
    constexpr int kPixels720p  = 1280 * 720;

    constexpr int64_t kBitrate1080p = 4000000;  // 4 Mbps
    constexpr int64_t kBitrate720p  = 2000000;  // 2 Mbps
    constexpr int64_t kBitrate480p  = 1000000;  // 1 Mbps
}  // namespace

RtmpCodecConfig::RtmpCodecConfig(media::VideoCodecType codec_type, int width, int height, float fps)
    : codec_type_(codec_type), width_(width), height_(height), fps_(fps) {}

AVCodecID RtmpCodecConfig::ToAvCodecId(media::VideoCodecType type) {
    switch (type) {
        case media::VideoCodecType::kH264:
            return AV_CODEC_ID_H264;
        case media::VideoCodecType::kH265:
            return AV_CODEC_ID_H265;
        default:
            return AV_CODEC_ID_H264;
    }
}

int64_t RtmpCodecConfig::EstimateBitrate(int width, int height) {
    const int pixels = width * height;
    if (pixels >= kPixels1080p) {
        return kBitrate1080p;
    }
    if (pixels >= kPixels720p) {
        return kBitrate720p;
    }
    return kBitrate480p;
}

bool RtmpCodecConfig::ParseAndPrepare(const uint8_t* data, size_t size, const uint8_t*& out_data,
                                      size_t& out_size, media::HFrameType& main_type,
                                      media::HFrameType& lead_type) {
    // Extract VPS/SPS/PPS/SEI from NALU stream
    size_t offset            = 0;
    size_t sz                = 0;
    size_t last_frame_offset = 0;
    bool has_video_slice     = false;

    out_data  = data;
    out_size  = size;
    main_type = media::HFrameType::UNKNOWN;
    lead_type = media::HFrameType::UNKNOWN;

    if (!data || size == 0) {
        return false;
    }

    do {
        offset += sz;
        sz = media::SeparateHVideoFrame(data + offset, size - offset);

        auto assign = [](auto& v, const uint8_t* d, size_t off, size_t len) {
            if (v.empty()) {
                v.assign(d + off, d + off + len);
            }
        };

        auto type = media::GetFrameType(codec_type_, data + offset);
        switch (type) {
            case media::HFrameType::VPS:
                assign(vps_, data, offset, sz);
                break;
            case media::HFrameType::SPS:
                assign(sps_, data, offset, sz);
                break;
            case media::HFrameType::PPS:
                assign(pps_, data, offset, sz);
                break;
            case media::HFrameType::SEI:
                assign(sei_, data, offset, sz);
                break;
            case media::HFrameType::P:
            case media::HFrameType::I:
                last_frame_offset = offset;
                has_video_slice   = true;
                sz                = size - offset;
                break;
            default:
                break;
        }
    } while (offset + sz != size);

    lead_type = media::GetFrameType(codec_type_, data);
    if (!has_video_slice) {
        return false;
    }

    main_type = media::GetFrameType(codec_type_, data + last_frame_offset);

    // If I-frame lacks SPS prefix, prepend cached VPS/SPS/PPS/SEI
    // SPS/PPS from in-band stream have 00 00 00 01 prefix (SeparateHVideoFrame
    // includes it); those from avcC extradata are raw NAL units.
    // Annex-B-mix would corrupt the bytestream — detect and add prefix as needed.
    if (main_type == media::HFrameType::I && lead_type != media::HFrameType::SPS &&
        lead_type != media::HFrameType::VPS) {
        static const uint8_t kStartCode[]{0x00, 0x00, 0x00, 0x01};
        auto needsPrefix = [](const std::vector<uint8_t>& nal) {
            return !nal.empty() &&
                   (nal.size() < 4 || nal[0] != 0x00 || nal[1] != 0x00 ||
                    nal[2] != 0x00 || nal[3] != 0x01);
        };
        auto appendNal = [&](const std::vector<uint8_t>& nal) {
            if (nal.empty()) return;
            if (needsPrefix(nal)) {
                prepend_buffer_.insert(prepend_buffer_.end(), kStartCode, kStartCode + 4);
            }
            prepend_buffer_.insert(prepend_buffer_.end(), nal.begin(), nal.end());
        };

        prepend_buffer_.clear();
        prepend_buffer_.reserve(4 * 4 + vps_.size() + sps_.size() + pps_.size() + sei_.size() + size);
        appendNal(vps_);
        appendNal(sps_);
        appendNal(pps_);
        appendNal(sei_);
        prepend_buffer_.insert(prepend_buffer_.end(), data, data + size);
        out_data = prepend_buffer_.data();
        out_size = prepend_buffer_.size();
    } else {
        out_data = data;
        out_size = size;
    }

    return true;
}

void RtmpCodecConfig::ConfigureStream(AVStream* stream) const {
    if (!stream || !stream->codecpar) {
        return;
    }

    AVCodecParameters* par = stream->codecpar;
    par->codec_id          = ToAvCodecId(codec_type_);
    par->codec_type        = AVMEDIA_TYPE_VIDEO;
    par->bit_rate          = EstimateBitrate(width_, height_);
    par->codec_tag         = 0;
    par->width             = width_;
    par->height            = height_;
    par->profile           = FF_PROFILE_H264_BASELINE;

    stream->time_base      = {1, 90000};
    stream->duration       = static_cast<int64_t>(90000.0f / fps_);
    stream->r_frame_rate   = {static_cast<int>(fps_ * 1000), 1000};
    stream->avg_frame_rate = stream->r_frame_rate;
    stream->id             = stream->index;

    // Write SPS/PPS/SEI into extradata — required for playback
    size_t total_size = sps_.size() + pps_.size() + sei_.size();
    if (total_size == 0) {
        LOG_WARN("{}", "ConfigureStream: no SPS/PPS extracted yet");
        return;
    }

    auto* mem = static_cast<uint8_t*>(av_mallocz(total_size + AV_INPUT_BUFFER_PADDING_SIZE));
    if (mem) {
        size_t pos = 0;
        memcpy(mem + pos, sps_.data(), sps_.size());
        pos += sps_.size();
        memcpy(mem + pos, pps_.data(), pps_.size());
        pos += pps_.size();
        memcpy(mem + pos, sei_.data(), sei_.size());

        av_freep(&par->extradata);
        par->extradata      = mem;
        par->extradata_size = static_cast<int>(total_size);
    }

    LOG_INFO("ConfigureStream: codec={} {}x{} bitrate={} fps={}", static_cast<int>(codec_type_), width_,
             height_, par->bit_rate, fps_);
}

void RtmpCodecConfig::SetParameters(const std::vector<uint8_t>& extradata) {
    if (extradata.empty() || extradata.size() < 7) {
        LOG_WARN("SetParameters: extradata empty or too small ({} bytes)", extradata.size());
        return;
    }
    switch (codec_type_) {
        case media::VideoCodecType::kH264:
            ParseAvcC(extradata);
            break;
        case media::VideoCodecType::kH265:
            ParseHevcC(extradata);
            break;
        default:
            break;
    }
}

void RtmpCodecConfig::ParseAvcC(const std::vector<uint8_t>& data) {
    // ISO 14496-15 AVCDecoderConfigurationRecord
    // byte 0: configurationVersion (=1)
    // byte 1: AVCProfileIndication
    // byte 2: profile_compatibility
    // byte 3: AVCLevelIndication
    // byte 4: 0xFC | (lengthSizeMinusOne & 0x03)
    // byte 5: 0xE0 | (numOfSequenceParameterSets & 0x1F)
    //   for each SPS: uint16(spsLength) + spsNALUnit
    // byte: numOfPictureParameterSets
    //   for each PPS: uint16(ppsLength) + ppsNALUnit

    // NAL units from avcC extradata are raw (no Annex B start code).
    // Add 00 00 00 01 so ConfigureStream writes valid Annex B extradata
    // (FFmpeg FLV muxer auto-detects Annex B → AVCC), and so ParseAndPrepare
    // can prepend them to I-frame bytestream data correctly.
    static const uint8_t kAnnexB[]{0x00, 0x00, 0x00, 0x01};

    if (data.size() < 7) return;

    size_t pos = 5;                        // skip configurationVersion + 3 profile bytes + lengthSize
    uint8_t num_sps = data[pos] & 0x1F;    // lower 5 bits
    pos++;

    for (uint8_t i = 0; i < num_sps; ++i) {
        if (pos + 2 > data.size()) return;
        uint16_t sps_len = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
        pos += 2;
        if (pos + sps_len > data.size()) return;
        sps_.assign(kAnnexB, kAnnexB + 4);
        sps_.insert(sps_.end(), data.begin() + pos, data.begin() + pos + sps_len);
        pos += sps_len;
    }

    if (pos >= data.size()) return;
    uint8_t num_pps = data[pos];
    pos++;

    for (uint8_t i = 0; i < num_pps; ++i) {
        if (pos + 2 > data.size()) return;
        uint16_t pps_len = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
        pos += 2;
        if (pos + pps_len > data.size()) return;
        pps_.assign(kAnnexB, kAnnexB + 4);
        pps_.insert(pps_.end(), data.begin() + pos, data.begin() + pos + pps_len);
        pos += pps_len;
    }

    LOG_INFO("ParseAvcC: extracted SPS({}B) PPS({}B)", sps_.size(), pps_.size());
}

void RtmpCodecConfig::ParseHevcC(const std::vector<uint8_t>& data) {
    // ISO 14496-15 HEVCDecoderConfigurationRecord — similar structure
    // with arrays of VPS, SPS, PPS.  For now just log; full HEVC support
    // follows the same pattern as ParseAvcC.
    LOG_INFO("ParseHevcC: {} bytes (HEVC extradata parsing not yet implemented)", data.size());
}

}  // namespace cosmo
