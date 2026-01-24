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

    std::uint32_t READ_32(std::uint32_t addr){  //Reads a complete word from the memory
        if(addr + 3 >= MAX_MEMORY){
            return 0;
        }
        std::uint32_t word = (std::uint32_t)memory[addr] | 
                             ((std::uint32_t)memory[addr + 1] << 8) | 
                             ((std::uint32_t)memory[addr + 2] << 16) | 
                             ((std::uint32_t)memory[addr + 3] << 24);
        return word;
    }

    std::uint16_t READ_16(std::uint32_t addr){  //Reads a half word from the memory
        if(addr + 1 >= MAX_MEMORY){
            return 0;
        }
        std::uint16_t hword = (std::uint16_t)memory[addr] | 
                              ((std::uint16_t)memory[addr + 1] << 8);
        return hword;
    }

    void WRITE_32(std::uint32_t addr, std::uint32_t val){// Writes a word to memory
        if(addr + 3 > MAX_MEMORY) return;
        memory[addr] = val & 0xFF;
        memory[addr + 1] = (val >> 8) & 0xFF;
        memory[addr + 2] = (val >> 16) & 0xFF;
        memory[addr + 3] = (val >> 24) & 0xFF;
    }

    void WRITE_16(std::uint32_t addr, std::uint32_t val){// Writes a half word to memory
        if(addr + 1 > MAX_MEMORY) return;
        memory[addr] = val & 0xFF;
        memory[addr + 1] = (val >> 8) & 0xFF;
    }

    //Executes the given instruction
    void Execute(Decoded_Instruction& inst){
        bool branched = false;

        switch(inst.opcode){
            case 0x03:
                switch(inst.func3){
                    case 0x0:   //LB
                        regs[inst.rd] = (std::int8_t)memory[regs[inst.rs1] + inst.imm];
                        break;
                    case 0x1:   //LH
                        regs[inst.rd] = (std::int16_t)READ_16(regs[inst.rs1] + inst.imm);
                        break;
                    case 0x2:   //LW
                        regs[inst.rd] = READ_32(regs[inst.rs1] + inst.imm);
                        break;
                    case 0x4:   //LBU
                        regs[inst.rd] = memory[regs[inst.rs1]] + inst.imm;
                        break;
                    case 0x5:   //LHU
                        regs[inst.rd] = READ_16(regs[inst.rs1] + inst.imm);
                        break;
                }
                break;
            case 0x13:
                switch(inst.func3){
                    case 0x0:   //ADDI
                        regs[inst.rd] = regs[inst.rs1] + inst.imm;
                        break;
                    case 0x1:
                        switch(inst.func7){
                            case 0x00:  //SLLI
                                regs[inst.rd] = regs[inst.rs1] << (inst.imm & 0x1F);
                                break;
                        }
                        break;
                    case 0x2:   //SLTI
                        regs[inst.rd] = ((int32_t)regs[inst.rs1] < inst.imm) ? 1 : 0;
                        break;
                    case 0x3:   //SLTIU
                        regs[inst.rd] = (regs[inst.rs1] < (uint32_t)inst.imm) ? 1 : 0;
                        break;
                    case 0x4:   //XORI
                        regs[inst.rd] = regs[inst.rs1] ^ inst.imm;
                        break;
                    case 0x5:
                        switch(inst.func7){
                            case 0x00:  //SRLI
                                regs[inst.rd] = regs[inst.rs1] >> (inst.imm & 0x1F);
                                break;
                            case 0x20:  //SRAI
                                regs[inst.rd] = (int32_t)regs[inst.rs1] >> (inst.imm & 0x1F);
                                break;
                        }
                        break;
                    case 0x6:   //ORI
                        regs[inst.rd] = regs[inst.rs1] | inst.imm;
                        break;
                    case 0x7:   //ANDI
                        regs[inst.rd] = regs[inst.rs1] & inst.imm;
                        break;
                 }
                break;
            case 0x17:  //AUIPC
                regs[inst.rd] = (PC - 4) + inst.imm;
                break;
            case 0x23:
                switch(inst.func3){
                    case 0x0:   //SB
                        memory[regs[inst.rs1] + inst.imm] = regs[inst.rs2] & 0xFF;
                        break;
                    case 0x1:   //SH
                        WRITE_16(regs[inst.rs1] + inst.imm, regs[inst.rs2] & 0xFFFF);
                        break;
                    case 0x2:   //SW
                        WRITE_32(regs[inst.rs1] + inst.imm, regs[inst.rs2]);
                        break;
                }
                break;
            case 0x33:
                switch(inst.func3){
                    case 0x0:
                        switch(inst.func7){
                            case 0x00:  //ADD
                                regs[inst.rd] = regs[inst.rs1] + regs[inst.rs2];
                                break;
                            case 0x20:  //SUB
                                regs[inst.rd] = regs[inst.rs1] - regs[inst.rs2];
                                break;
                        }
                        break;
                    case 0x1:
                        switch(inst.func7){
                            case 0x00:  //SLL
                                regs[inst.rd] = regs[inst.rs1] << (regs[inst.rs2] & 0x1F);
                                break;
                        }
                        break;
                    case 0x2:
                        switch(inst.func7){
                            case 0x00:  //SLT
                                regs[inst.rd] = ((std::int32_t)regs[inst.rs1] < (std::int32_t)regs[inst.rs2]) ? 1 : 0;
                                break;
                        }
                        break;
                    case 0x3:
                        switch(inst.func7){
                            case 0x00:  //SLTU
                                regs[inst.rd] = (regs[inst.rs1] < regs[inst.rs2]) ? 1 : 0;
                                break;
                        }
                        break;
                    case 0x4:
                        switch(inst.func7){
                            case 0x00:  //XOR
                                regs[inst.rd] = regs[inst.rs1] ^ regs[inst.rs2];
                                break;
                        }
                        break;
                    case 0x5:
                        switch(inst.func7){
                            case 0x00:  //SRL
                                regs[inst.rd] = regs[inst.rs1] >> (regs[inst.rs2] & 0x1F);
                                break;
                            case 0x20:  //SRA
                                regs[inst.rd] = (std::int32_t)regs[inst.rs1] >> (regs[inst.rs2] & 0x1F);
                                break;
                        }
                        break;
                    case 0x6:
                        switch(inst.func7){
                            case 0x00: //OR
                                regs[inst.rd] = regs[inst.rs1] | regs[inst.rs2];
                                break;
                        }
                        break;
                    case 0x7:
                        switch(inst.func7){
                            case 0x00:  //AND
                                regs[inst.rd] = regs[inst.rs1] & regs[inst.rs2];
                                break;
                        }
                        break;
                 }
                break; 
            case 0x37:  //LUI
                regs[inst.rd] = inst.imm;
                break;
            case 0x63:
                switch(inst.func3){
                    case 0x0:   //BEQ
                        PC = (regs[inst.rs1] == regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                    case 0x1:   //BNE
                        PC = (regs[inst.rs1] != regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                    case 0x4:   //BLT
                        PC = ((std::int32_t)regs[inst.rs1] < (std::int32_t)regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                    case 0x5:   //BGE
                        PC = ((std::int32_t)regs[inst.rs1] >= (std::int32_t)regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                    case 0x6:   //BLTU
                        PC = (regs[inst.rs1] < regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                    case 0x7:   //BGEU
                        PC = (regs[inst.rs1] >= regs[inst.rs2]) ? (PC - 4) + inst.imm : PC;
                        break;
                }
                break;
            case 0x67:
                switch(inst.func3){
                    case 0x0:   //JALR
                        std::uint32_t targ = (regs[inst.rs1] + inst.imm) & ~1;
                        regs[inst.rd] = PC;
                        PC = targ;
                        break;
                }
                break;
            case 0x6F:  //JAL
                regs[inst.rd] = PC;
                PC = (PC - 4) + inst.imm;
                break;
            case 0x73:
                switch(inst.imm){
                    case 0x0:   //ECALL
                        switch(regs[17]){
                            case 93:
                                std::cout<<"\n[Emulator] Program exited with code "<<regs[10]<<std::endl;
                                exit(0);
                                break;
                            case 64:
                                if(regs[10] == 1){
                                    for(std::uint32_t i = 0; i<regs[12]; i++){
                                        std::cout<<(char)memory[regs[11] + i];
                                    }
                                }
                                break;
                        }
                        break;
                    case 0x1:   //EBREAK
                        std::cout << "Breakpoint hit at PC: " << std::hex << (PC-4) << std::endl;
                        break;
                }
                break;
            }

        regs[0] = 0;
    }
};


int main() {
}