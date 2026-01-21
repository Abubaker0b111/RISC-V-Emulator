#include<cstdint>
#include<iostream>
#include<fstream>
#include<string>

struct RISC_V
{
    std::uint32_t regs[32];
    std::uint32_t PC;
    std::uint32_t MAX_MEMORY;
    std::uint8_t* memory;
    RISC_V()
    {
        //Memory size is set to 64MB
        MAX_MEMORY = 1024 * 1024 * 64;  
        //Setting all registers to 0
        for(int i=0 ; i<32 ; i++) {
            regs[i] = 0;
        }
        //Allocating Memory
        memory = new uint8_t[MAX_MEMORY]{0};
        //The Program counter starts at the begging of memory
        PC = 0;
    }

    //Read a File and Loads it into the memory;
    bool LOAD_FILE(std::string FileName)
    {
        std::ifstream file(FileName, std::ios::binary | std::ios::ate);
        if(!file.is_open()) {
            std::cerr<<"Error: Cannot open File \""<<FileName<<"\"";
            return false;
        }

        //Checking the size of the file
        std::streamsize size = file.tellg();

        //Moving to the begining of the file
        file.seekg(0, std::ios::beg);

        if(size > MAX_MEMORY) {
            std::cerr<<"Error: Memory OverFlow. The file\""<<FileName<<"\" is too large";
            return false;
        }

        file.read(reinterpret_cast<char*>(&memory[0]), size);
        return true;
    }

    //Fetches a 32bit word
    std::uint32_t FETCH()
    {
        std::uint32_t word = 0;
        word |= memory[PC];
        word |= memory[PC + 1] << 8;
        word |= memory[PC + 2] << 16;
        word |= memory[PC + 3] << 24;
        return word;
    }
};

int main()
{
       
}