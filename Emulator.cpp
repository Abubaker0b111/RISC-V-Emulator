#include<cstdint>

struct RISC_V
{
    std::uint32_t* regs;
    std::uint8_t* PC;
    std::uint32_t MAX_MEMORY;
    std::uint8_t* memory;
    RISC_V()
    {
        //Memory size is set to 64MB
        MAX_MEMORY = 1024 * 1024 * 64;  
        //Initializing registers 
        regs = new uint32_t[32];
        //Setting the 0x0 register to 0
        regs[0] = 0;   
        //Allocating Memory
        memory = new uint8_t[MAX_MEMORY];   
        //Pointing the program counter to address 0x00 in memory
        PC = memory;  
    }
};

int main(){

}