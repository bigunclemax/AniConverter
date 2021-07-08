//
// Created by user on 06.07.2021.
//

#ifndef ANICONVERTER_ANICONVERTER_H
#define ANICONVERTER_ANICONVERTER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

#include "h264bitstream/h264_stream.h"

using std::ifstream;
using std::ios;
using std::vector;
using std::ofstream;
namespace fs = std::filesystem;

namespace ani {
    constexpr uint32_t ANI_MAGIC = 0x8f2a6ec9;

#pragma pack(push, 1)
    struct ani_header {
        uint32_t signature;
        uint32_t frames_count;
        uint32_t file_size;
        uint32_t unknown_0;
        uint64_t framerate; //it's not actual framerate
        uint32_t frame_buff_sz;
        uint32_t unknown_1;
    };
#pragma pack(pop)
#pragma pack(push, 1)
    struct finfo {
        uint32_t length :31, start_frame :1;
    };
#pragma pack(pop)

    inline void print_ani_header(const ani_header &header) {

        auto fr_numerator = (uint32_t)header.framerate;
        auto fr_denominator = (uint32_t)(header.framerate >> 32);

        std::cout << "Frames: " << header.frames_count << std::endl;
        std::cout << "File sz: " << header.file_size << std::endl;
        std::cout << "Framerate: " << ((double)(fr_denominator * 1000000) / fr_numerator )
                  << " (" << fr_numerator << "/" << fr_denominator << ")" << std::endl;
        std::cout << "Frame buff sz: " << header.frame_buff_sz << std::endl;
        std::cout << "unknown_0: " << header.unknown_0 << std::endl;
        std::cout << "unknown_1: " << header.unknown_1 << std::endl;
    }

    inline int extract_ani_frames(const fs::path &p, const fs::path &out_p) {

        ani_header header{};

        ifstream aniFile(p, ios::binary | ios::ate);
        auto fileSize = aniFile.tellg();
        if (fileSize < sizeof(ani_header)) {
            std::cerr << "ANI file too short" << std::endl;
            return -1;
        }

        aniFile.seekg(0);
        aniFile.read(reinterpret_cast<char *>(&header), sizeof(ani_header));
        if (header.signature != ANI_MAGIC) {
            std::cerr << "ANI signature mismatch" << std::endl;
            return -1;
        }

        print_ani_header(header);

        vector<vector<uint8_t>> frames(header.frames_count);

        ofstream outfile(out_p, ios::binary);

        for (auto i = 0; i < header.frames_count; ++i) {
            finfo f_info{};
            aniFile.read(reinterpret_cast<char *>(&f_info), sizeof(finfo));

            std::cout << "Frame " << i << " sz: " << f_info.length
                      << (f_info.start_frame? " (start frame)" : "") << std::endl;

            frames[i].resize(f_info.length);
            aniFile.read(reinterpret_cast<char *>(frames[i].data()), f_info.length);
        }

        for (auto f : frames) {
            outfile.write(reinterpret_cast<char *>(f.data()), (long) f.size());
        }

        return 0;
    }

    inline int pack_ani(const fs::path &path, const fs::path &out_p) {

        const int BUFSIZE = (32*1024*1024);

        if (h264_dbgfile == nullptr) { h264_dbgfile = stderr; }

        auto h = h264_new();

        ifstream h264File(path, ios::binary | ios::ate);
        auto fileSize = h264File.tellg();
        h264File.seekg(0);

        struct sFrame {
            uint32_t size;
            bool start;
            vector <uint8_t> data;
        };
        vector<sFrame> frames_descriptors;

        vector<uint8_t> buf(BUFSIZE);
        size_t sz = 0;
        size_t rsz = 0;
        uint8_t* p = buf.data();
        int64_t off = 0;
        int nal_start = 0, nal_end = 0;

        bool isStartFrame = false;
        int64_t prev_frame_start = -1;

        while (true) {

            h264File.read(reinterpret_cast<char *>(buf.data() + sz), BUFSIZE - sz);
            rsz = h264File.gcount();
            if (rsz == 0) {
                if(h264File.fail() && !h264File.eof()) {
                    perror("Read failed");
                    h264_free(h);
                    return -1;
                }
                break;
            }

            sz += rsz;

            while (find_nal_unit(p, (int)sz, &nal_start, &nal_end) > 0) {

                auto nal_offset = (off + (p - buf.data()) + nal_start);

                fprintf( h264_dbgfile, "NAL at offset %ld (0x%04lX), size %d (0x%04X) \n",
                         nal_offset,
                         nal_offset,
                         (nal_end - nal_start),
                         (nal_end - nal_start));

                p += nal_start;

                //read_nal_unit(h, p, nal_end - nal_start); //this call crash on some data
                auto t = peek_nal_unit(h, p, nal_end - nal_start);
                if (9 == t) {
                    if (prev_frame_start >= 0) {
                        frames_descriptors.push_back({
                            .size = (uint32_t)((nal_offset - 4) - prev_frame_start),
                            .start = isStartFrame
                        });
                        isStartFrame = false;
                    }
                    prev_frame_start = nal_offset - 4;
                }

                if (5 == t) {
                    isStartFrame = true;
                }

                //skip to next NAL
                p += (nal_end - nal_start);
                sz -= nal_end;
            }

            // if no NALs found in buffer, discard it
            if (p == buf.data())
            {
                fprintf(h264_dbgfile,
                        "Did not find any NALs between offset %lld (0x%04llX), size %lld (0x%04llX), discarding \n",
                         (long long int)off,
                         (long long int)off,
                         (long long int)off + sz,
                         (long long int)off + sz);

                p = buf.data() + sz;
                sz = 0;
            }

            memmove(buf.data(), p, sz);
            off += p - buf.data();
            p = buf.data();
        }

        h264_free(h);

        frames_descriptors.push_back({
                                             .size = (uint32_t)(fileSize - prev_frame_start),
                                             .start = isStartFrame
                                     });


        h264File.clear();
        h264File.seekg(0);
        uint32_t max_frame_sz = 0;
        for (auto &frame_d : frames_descriptors) {
            const auto &frame_sz = frame_d.size;
            if(frame_sz > max_frame_sz) {
                max_frame_sz = (uint32_t)frame_sz;
            }
            frame_d.data.resize(frame_sz);
            h264File.read(reinterpret_cast<char *>(frame_d.data.data()), frame_sz);
        }

        ani_header header{};
        header.signature = ANI_MAGIC;
        header.frames_count = frames_descriptors.size();
        header.file_size = sizeof(ani_header) + fileSize + header.frames_count * sizeof(uint32_t);
        header.frame_buff_sz = max_frame_sz;
        header.framerate = 1000000;
        header.framerate |= (30ll << 32); //30 fps

        print_ani_header(header);

        ofstream outfile(out_p, ios::binary);
        outfile.write(reinterpret_cast<const char *>(&header), sizeof(header));

        for (const auto &frame :frames_descriptors) {

            finfo f_info{};
            f_info.length = frame.data.size();
            f_info.start_frame = frame.start;

            outfile.write(reinterpret_cast<const char *>(&f_info), sizeof (f_info));
            outfile.write(reinterpret_cast<const char *>(frame.data.data()), f_info.length);
        }

        return 0;
    }
}

#endif //ANICONVERTER_ANICONVERTER_H
