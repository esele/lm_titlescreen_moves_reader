#include "miniz.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using ios = std::ios;

#define SNES_WRAM_OFFSET 0x23F

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

    char head[4];
    savestate.get(head, 4);
    if(std::strcmp(head, "MSS") == 0)
    {
        uint32_t read_double;

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
        unsigned long uncompressed_size = read_double;

        read_double = savestate.get() | (savestate.get()<<8) | (savestate.get()<<16) | (savestate.get()<<24);
        char * compressed_data = new char[read_double];
        savestate.read(compressed_data, read_double);
        unsigned long compressed_size = read_double;

        if( uncompress(uncompressed_data, &uncompressed_size, (const unsigned char *)(compressed_data), compressed_size) != MZ_OK )
            return err("An error ocurred decompressing data from this Mesen 2 save.");

        // Work RAM
        if(uncompressed_data[SNES_WRAM_OFFSET] != 0x00)
            return err("Expected uncompressed RAM. How exactly are you generating this savestate?");

        read_double = uncompressed_data[SNES_WRAM_OFFSET+1] | (uncompressed_data[SNES_WRAM_OFFSET+2]<<8) |\
                      ( uncompressed_data[SNES_WRAM_OFFSET+3]<<16 ) | ( uncompressed_data[SNES_WRAM_OFFSET+4]<<24 );
        if(read_double!=0x00020000)
            return err("Expected a memory dump of 128KiB. Are you sure this is a SNES save?");

        // LM stores the controller input info starting at $7F0000
        memcpy(ram_page_2, &(uncompressed_data[0x10000+SNES_WRAM_OFFSET+5]), 0x10000);
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
