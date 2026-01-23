#include<cstdint>
#include<iostream>
#include<fstream>
#include<string>

struct Decoded_Instruction{//Struct to Hold the decoded Instructions
    std::uint8_t opcode;
    std::uint8_t rd;
    std::uint8_t func3;
    std::uint8_t rs1;
    std::uint8_t rs2;
    std::uint8_t func7;
    std::int32_t imm;
};

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

    //Decodes the raw binary code into instructions
    Decoded_Instruction DECODE(std::uint32_t raw){
        Decoded_Instruction inst;   //stores the decoded instructions
        inst.opcode = raw & 0x7F;   //Contains the opcode to specify the instruction type
        inst.rd = (raw >> 7) & 0x1F;    //stores the address of destination register
        inst.func3 = (raw >> 12) & 0x07;    //use to diffrentiate between instructions
        inst.rs1 = (raw >> 15) & 0x1F;  //stores the address of source register 1
        inst.rs2 = (raw >> 20) & 0x1F;  //stores the address of source register 2
        inst.func7 = (raw >> 25) & 0x7F;

        switch(inst.opcode){    //Decodes the imm value based on opcode
            case 0x03:
            case 0x13:
            case 0x67:
            case 0x73:
                inst.imm = static_cast<std::int32_t>(raw) >> 20;
                break;
            case 0x23:
                inst.imm = ((raw >> 7) & 0x1F) | (raw >> 25)<< 5;
                if (inst.imm & 0x800) {
                    inst.imm |= 0xFFFFF000;
                }
                break;
            case 0x37:
            case 0x17:
                inst.imm = raw & 0xFFFFF000;
                break;
            case 0x63:
                inst.imm = ((raw >> 8) & 0xF) << 1;
                inst.imm |= ((raw >> 25) & 0x3F) << 5; 
                inst.imm |= ((raw >> 7) & 0x1) << 11 ;
                inst.imm |= ((raw >> 31) & 0x1) << 12;

                if(inst.imm & 0x1000) {
                    inst.imm |= 0xFFFFE000;
                }
                break;
            case 0x6F:
                inst.imm = ((raw >> 21) & 0x3FF) << 1;
                inst.imm |= ((raw >> 20) & 0x1) << 11;
                inst.imm |= ((raw >> 12) & 0xFF) << 12;
                inst.imm |= ((raw >> 31) & 0x1) << 20;

                if(inst.imm & 0x100000) {
                    inst.imm |= 0xFFE00000;
                }
                break;
        } 

        return inst;
    }
};


int main() {
}