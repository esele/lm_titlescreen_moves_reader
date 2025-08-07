#include "tinf.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;
using ios = std::ios;

#define MESEN2_WRAM_OFFSET 0x23F

template<typename... T> inline bool err(T... what)
{
    std::cout << "ERROR: " ;
    ( (std::cout << what), ...);
    return false;
}

void cleanup_str(std::string* str)
{
    std::string path;
    for(auto& c : *str)
        if(c!='\"')
            path.push_back( std::tolower(c) );
    *str = path;
}

bool read_savestate(std::string * path)
{
    cleanup_str(path);
    std::ifstream savestate(fs::absolute(*path), ios::binary);

    const char * e = path->c_str();

    if(!savestate)
        return err("Couldn't open savestate ", path->c_str());

    uint8_t * ram_page_2 = new uint8_t[0x10000] {};
    uint32_t read_double;

    char head[4];
    savestate.get(head, 4);
    if(std::strcmp(head, "MSS") == 0)
    {
        // Expect Mesen 2 savestate
        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        if(read_double<(2<<16))
            return err("Mesen savestates must be from Mesen 2 or newer.");

        // Skip video data
        savestate.seekg(0x1F);
        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        savestate.seekg(read_double, ios::cur);

        // Skip ROM name
        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        savestate.seekg(read_double, ios::cur);

        // Compressed savestate
        if(savestate.get() != 0x01)
            return err("Expected compressed data. How exactly are you generating this savestate?");

        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        uint8_t * uncompressed_data = new uint8_t[read_double];
        unsigned int uncompressed_size = read_double;

        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        char * compressed_data = new char[read_double];
        savestate.read(compressed_data, read_double);
        unsigned int compressed_size = read_double;

        if( tinf_zlib_uncompress(uncompressed_data, &uncompressed_size, compressed_data, compressed_size) != TINF_OK )
            return err("An error ocurred decompressing data from this Mesen 2 savestate.");

        // Work RAM
        if(uncompressed_data[MESEN2_WRAM_OFFSET] != 0x00)
            return err("Expected uncompressed RAM. How exactly are you generating this savestate?");

        read_double = uncompressed_data[MESEN2_WRAM_OFFSET+1] | (uncompressed_data[MESEN2_WRAM_OFFSET+2]<<8) |\
                      ( uncompressed_data[MESEN2_WRAM_OFFSET+3]<<16 ) | ( uncompressed_data[MESEN2_WRAM_OFFSET+4]<<24 );
        if(read_double!=0x00020000)
            return err("Expected a memory dump of 128KiB. Are you sure this is a SNES save?");

        // LM stores the controller input info starting at $7F0000
        memcpy(ram_page_2, &(uncompressed_data[0x10000+MESEN2_WRAM_OFFSET+5]), 0x10000);
    }
    else if( ( ( ((uint8_t)(head[0]))<<8 ) | (uint8_t)(head[1]) ) == 0x1F8B )
    {
        // Expect gzip-compressed (snes9x 1.52+ most likely) savestate
        savestate.seekg(0, ios::end);
        unsigned int compressed_size = savestate.tellg();
        char * compressed_data = new char[compressed_size];

        savestate.seekg(0);
        savestate.read(compressed_data, compressed_size);

        // A savestate is not going to be 4GiB uncompressed so let's just take the value as is
        savestate.seekg(compressed_size-4, ios::beg);
        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        uint8_t * uncompressed_data = new uint8_t[read_double];
        unsigned int uncompressed_size = read_double;

        if( tinf_gzip_uncompress(uncompressed_data, &uncompressed_size, compressed_data, compressed_size) != TINF_OK )
            return err("An error ocurred decompressing data from a gzip-compressed savestate.");

        // snes9x magic string
        std::string signature((const char *)(uncompressed_data), 8);
        if(std::strcmp(signature.c_str(), "#!s9xsnp") != 0)
            return err("Expected a snes9x savestate.");

        // Find RAM string
        std::string_view uncompressed_data_sv { (const char *)(uncompressed_data), uncompressed_size };
        std::string_view ram_header { "RAM:131072", 10 };
        const std::boyer_moore_searcher s(ram_header.begin(), ram_header.end());
        const auto it = std::search(uncompressed_data_sv.begin(), uncompressed_data_sv.end(), s);

        if(it == uncompressed_data_sv.end())
            return err("Couldn't find where the RAM data begins in this snes9x savestate. What version are you using?");

        const auto ram_off = std::distance(uncompressed_data_sv.begin(), it);

        // LM stores the controller input info starting at $7F0000
        memcpy(ram_page_2, &(uncompressed_data[0x10000+ram_off+11]), 0x10000);
    }

    // 0xFF = end data
    for(int i = 0; i<0x10000; ++i)
    {
        if(ram_page_2[i] == 0xFF)
        {
            std::ofstream("./raw.bin", ios::binary).write((const char *)(ram_page_2), i+1);
            return true;
        }
        if(i+1==0x10000)
            return err("Found no terminator byte for this input sequence. Are you sure you have the titlescreen recording hack enabled?");
    }

}

int main(int argc, char * argv[])
{
    std::string savestate_path;

    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
        {
            savestate_path = std::string(argv[i]);
            if(!read_savestate(&savestate_path))
                return 1;
        }

        return 0;
    }
    else
    {
        std::cout << "Insert path to savestate: ";
        std::getline(std::cin, savestate_path);

        return !read_savestate(&savestate_path);
    }
}
