#include<cstdint>
#include<iostream>
#include<fstream>
#include<string>
#include<cstring>
#include<vector>
#include<conio.h>

//Global Variables
uint64_t cycle_count = 0;   //cycles executed
uint64_t inst_count = 0;    //instructions executed;

struct TraceRecord{ //Struct to hold trace buffer records
    uint32_t pc;
    uint32_t raw;
};

static const int TRACE_SIZE = 100;
TraceRecord trace_buffer[TRACE_SIZE];
int trace_index = 0;
bool trace_full = false;

void Log_Trace(uint32_t pc, uint32_t raw){  //Logs traces using circular logic
    trace_buffer[trace_index].pc = pc;
    trace_buffer[trace_index].raw = raw;

    trace_index = (trace_index + 1) % TRACE_SIZE;
    if(trace_index == 0) trace_full = true;
}

void Dump_Trace(){  //Dumps trace duh!
    std::cout<<"\n|| Trace Dump ||\n";
    std::cout<<"---------------------------------\n";

    int start = trace_full ? trace_index : 0;
    int count = trace_full ? TRACE_SIZE : trace_index;

    for(int i=0; i<count ; i++){
        int idx = (start + i) % TRACE_SIZE;
        std::cout<<"PC: 0x"<<std::hex<<trace_buffer[idx].pc<<" | Inst: 0x"<<trace_buffer[idx].raw<<std::dec<<std::endl;
    }
    std::cout<<"---------------------------------\n";
}

struct BranchPredictor{ //Predictes Branch in advance to save time
    uint8_t table[4096];
    uint64_t total; //Helper var: Total prediction;
    uint64_t correct;   //Helper var: Correct predictions;

    BranchPredictor(){
        std::memset(table, 1, 4096);    //initialzing the table with 1s
    }

    bool predict(uint32_t pc){
        return table[(pc >> 2) & 0xFFF] >= 2;
    }

    void update(uint32_t pc, bool taken){   //Updates the table based on actual and predicted operation
        uint32_t index = (pc >> 2) & 0xFFF;
        uint8_t state = table[index];

        total++;

        if(taken){
            if(state < 3) table[index]++; 
        }
        else{
            if(state > 0) table[index]--;
        }
    }

};
BranchPredictor btb;

struct Memory_Segment{  //Struct to hold info about a Given memory segment
    uint32_t start; // Starting address
    uint32_t end;   // End Address
    uint32_t flags; //Flags i.e 1,2,4 etc
};

struct Decoded_Instruction{//Struct to Hold the decoded Instructions
    uint8_t opcode;
    uint8_t rd;
    uint8_t func3;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t func7;
    int32_t imm;
};

//ELF32 Header
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    uint16_t      e_type; //Format Type
    uint16_t      e_machine;// Machine Type
    uint32_t      e_version;
    uint32_t      e_entry;    // Entry point address
    uint32_t      e_phoff;    // Program header offset
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;    // Number of program headers
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
};

struct Elf32_Phdr {// Program Header
    uint32_t p_type;   
    uint32_t p_offset; // File offset
    uint32_t p_vaddr;  // Virtual address in memory
    uint32_t p_paddr;
    uint32_t p_filesz; // Size in file
    uint32_t p_memsz;  // Size in memory
    uint32_t p_flags;
    uint32_t p_align;
};

struct RISC_V
{
    uint32_t regs[32];
    uint32_t PC;
    uint32_t MAX_MEMORY;
    uint8_t* memory;
    uint32_t MEM_Offset;
    std::vector<Memory_Segment> memory_map;
    bool running;

    uint32_t csrs[4096]; // CSR registers

    const uint32_t MCYCLE_L   = 0xB00; // machine cycle counter (low)
    const uint32_t MCYCLE_H   = 0xB80; // machine cycle counter (high)
    const uint32_t MINSTRET_L  = 0xB02; // instructions retired (low)
    const uint32_t MINSTRET_H = 0xB82; // instructions retired (high)

    const uint32_t UART_addr = 0x10000000; //Address of the special i/o location

    uint64_t mtimecmp = 0xffffffffffffffff; //Alarm time
    uint32_t mtvec = 0; //Address of the interrupt handler
    uint32_t mepc = 0;  //Old PC (return after interrupt)
    uint32_t mcause = 0;    //Cause of interrupt
    uint32_t mstatus = 0;   //machine status

    RISC_V()
    {
        
        MAX_MEMORY = 1024 * 1024 * 64;  //Memory size is set to 64MB
        //Setting all registers to 0
        for(int i=0 ; i<32 ; i++) {
            regs[i] = 0;
        }
       
        memory = new uint8_t[MAX_MEMORY]{0}; //Allocating Memory
        PC = 0; //The Program counter starts at the begging of memory

        running = false;

        for(int i=0; i<4096; i++) csrs[i] = 0;

    }

    ~RISC_V(){
        delete[] memory;
        memory = nullptr;
    }

    bool Check_Permission(uint32_t addr, int required_perm) {   //Checks the permission for a Given Memory address
        for(const auto& seg : memory_map){
            if(addr >= seg.start && addr < seg.end){
                if((seg.flags & required_perm) == required_perm) return true;
                else{
                    return false;
                }
            }
        }

        if (addr == 0) return false;

        if(addr >= (MEM_Offset + MAX_MEMORY - 0x10000) && addr < (MEM_Offset + MAX_MEMORY)){
            if((6 & required_perm) == required_perm) return true;   //Checking for stack memory
        }

        return false;
    }

    //Reads a file parses it and Loads the program into the memory;
    bool LOAD_FILE(std::string FileName) {
        std::ifstream file(FileName, std::ios::binary);
        if(!file.is_open()) return false;

        Elf32_Ehdr header;

        file.read(reinterpret_cast<char*>(&header), sizeof(Elf32_Ehdr));

        if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E' 
            || header.e_ident[2] != 'L' || header.e_ident[3] != 'F') {
                std::cerr<<"Error: Not a valid ELF file."<<std::endl;
                return false;
            }
        if(header.e_machine != 0xF3){
            std::cerr << "Error: Not a RISC-V architecture file." << std::endl;
            return false;
        }

        PC = header.e_entry;
        Elf32_Phdr phdr;

        uint32_t lowest_vaddr = 0xFFFFFFFF;

        //Calculating the lowest virtual address to Set Memory Offset in order to fit inside the given memory
        for(int i = 0; i < header.e_phnum; i++) {
            file.seekg(header.e_phoff + (i * header.e_phentsize));
            file.read(reinterpret_cast<char*>(&phdr), sizeof(Elf32_Phdr));
            if (phdr.p_type == 1 && phdr.p_vaddr < lowest_vaddr) {
                lowest_vaddr = phdr.p_vaddr;
            }
        }

        MEM_Offset = lowest_vaddr; 

        regs[2] = MEM_Offset + MAX_MEMORY - 4;// Setting the regs[2] to End of Memmory

        for(int i=0 ; i<header.e_phnum ; i++){

            file.seekg(header.e_phoff + (i * header.e_phentsize));
            file.read(reinterpret_cast<char*>(&phdr), sizeof(Elf32_Phdr));


            if(phdr.p_type == 1){// Only loads a phdr data into memory if p_type = 1

                if (phdr.p_vaddr < MEM_Offset){
                    std::cerr << "Error: Segment address 0x" << std::hex << phdr.p_vaddr 
                            << " is below MEM_Offset 0x" << MEM_Offset << std::dec << std::endl;
                    return false;
                }

                uint32_t local_addr = phdr.p_vaddr - MEM_Offset;//Calculates the local address using Memory Offset
                
                if(local_addr + phdr.p_memsz > MAX_MEMORY) {
                    std::cerr << "Error: Segment too large for emulator memory." << std::endl;
                    return false;
                }

                Memory_Segment seg;
                seg.start = phdr.p_vaddr;
                seg.end = phdr.p_vaddr + phdr.p_memsz;
                seg.flags = phdr.p_flags;
                memory_map.push_back(seg);

                file.seekg(phdr.p_offset);
                file.read(reinterpret_cast<char*>(&memory[local_addr]), phdr.p_filesz);

                if(phdr.p_memsz > phdr.p_filesz) {
                    std::memset(&memory[local_addr + phdr.p_filesz], 0, phdr.p_memsz-phdr.p_filesz);
                }
            }
        }
        return true;
    }

    //Fetches a 32 bit word
    uint32_t FETCH()
    {
        if(PC - MEM_Offset >= MAX_MEMORY || PC < MEM_Offset) return 0;
        uint32_t word = 0;
        word |= memory[PC - MEM_Offset];
        word |= memory[PC + 1 - MEM_Offset] << 8;
        word |= memory[PC + 2 - MEM_Offset] << 16;
        word |= memory[PC + 3 - MEM_Offset] << 24;
        return word;
    }

    //Decodes the raw binary code into instructions
    Decoded_Instruction DECODE(uint32_t raw){
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
                inst.imm = static_cast<int32_t>(raw) >> 20;
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

    uint32_t READ_32(uint32_t addr){  //Reads a complete word from the memory

        uint64_t current_time = ((uint64_t)csrs[MCYCLE_H] << 32) | csrs[MCYCLE_L];

        if (addr == 0x0200BFF8) return (uint32_t)(current_time & 0xFFFFFFFF);   //higher 32 bits

        if (addr == 0x0200BFFC) return (uint32_t)(current_time >> 32);  //lower 32 bits

        if(addr + 3 - MEM_Offset >= MAX_MEMORY){
            return 0;
        }

        if(!Check_Permission(addr, 4) || !Check_Permission(addr + 3, 4)){   //Read Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Read)" << std::endl;
            Dump_Trace();
            running = false;
            return 0;
        }

        uint32_t word = (uint32_t)memory[addr - MEM_Offset] | 
                        ((uint32_t)memory[addr + 1 - MEM_Offset] << 8) | 
                        ((uint32_t)memory[addr + 2 - MEM_Offset] << 16) | 
                        ((uint32_t)memory[addr + 3 - MEM_Offset] << 24);
        return word;
    }

    uint16_t READ_16(uint32_t addr){  //Reads a half word from the memory
        if(addr + 1 - MEM_Offset >= MAX_MEMORY){
            return 0;
        }

        if(!Check_Permission(addr, 4) || !Check_Permission(addr + 1, 4)){   //Read Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Read)" << std::endl;
            Dump_Trace();
            running = false;
            return 0;
        }

        uint16_t hword = (uint16_t)memory[addr - MEM_Offset] | 
                         ((uint16_t)memory[addr + 1 - MEM_Offset] << 8);
        return hword;
    }

    uint8_t READ_8(uint32_t addr){  //Reads a byte from memory

        if(addr == 0x10000005) { //Checks if a key is pressed or not
            return _kbhit() ? 0x01 : 0x00;
        }

        if(addr == 0x10000000) {    //If key is pressed reads the character and returns it
            if(_kbhit()){
                return _getch();
            }
            else{
                return 0;
            }
        }

        if (addr < MEM_Offset || addr - MEM_Offset >= MAX_MEMORY) {
            return 0;
        }

        if(!Check_Permission(addr, 4)){   //Read Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Read)" << std::endl;
            Dump_Trace();
            running = false;
            return 0;
        }

        return memory[addr - MEM_Offset];
    }

    void WRITE_32(uint32_t addr, uint32_t val){ // Writes a word to memory

        if(addr == 0x02004000){//lower 32 bits
            mtimecmp = (mtimecmp & 0xFFFFFFFF00000000) | (uint64_t)val;
            return;
        }
        
        if (addr == 0x02004004){//higher 32 bits
            mtimecmp = (mtimecmp & 0x00000000FFFFFFFF) | ((uint64_t)val << 32);
            csrs[0x344] &= ~(1 << 7); //clear pending bit
            return;
        }

        if(addr + 3 - MEM_Offset >= MAX_MEMORY) return;

        if(!Check_Permission(addr, 2) || !Check_Permission(addr + 3, 2)){   //Write Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Write)" << std::endl;
            Dump_Trace();
            running = false;
            return;
        }

        memory[addr - MEM_Offset] = val & 0xFF;
        memory[addr + 1 - MEM_Offset] = (val >> 8) & 0xFF;
        memory[addr + 2 - MEM_Offset] = (val >> 16) & 0xFF;
        memory[addr + 3 - MEM_Offset] = (val >> 24) & 0xFF;
    }

    void WRITE_16(uint32_t addr, uint32_t val){ // Writes a half word to memory
        if(addr + 1 - MEM_Offset >= MAX_MEMORY) return;

        if(!Check_Permission(addr, 2) || !Check_Permission(addr + 1, 2)){   //Write Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Write)" << std::endl;
            Dump_Trace();
            running = false;
            return;
        }

        memory[addr - MEM_Offset] = val & 0xFF;
        memory[addr + 1 - MEM_Offset] = (val >> 8) & 0xFF;
    }

    void WRITE_8(uint32_t addr, uint8_t val) {  // Writes a byte to memory 

        if (addr == UART_addr){
            std::cout << (char)val; // Print to terminal
            std::cout.flush();
            return;
        }

        if (addr < MEM_Offset || addr - MEM_Offset >= MAX_MEMORY) {
            return;
        }

        if(!Check_Permission(addr, 2)){   //Write Permission Check
            std::cerr << "Fatal Error: Segmentation Fault (Write)" << std::endl;
            Dump_Trace();
            running = false;
            return;
        }

        memory[addr - MEM_Offset] = val;
    }
    
    //Executes the given instruction
    void EXECUTE(Decoded_Instruction& inst){
        bool branched = false;

        switch(inst.opcode){
            case 0x03:
                switch(inst.func3){
                    case 0x0:   //LB
                        regs[inst.rd] = (int8_t)READ_8(regs[inst.rs1] + inst.imm);
                        break;
                    case 0x1:   //LH
                        regs[inst.rd] = (int16_t)READ_16(regs[inst.rs1] + inst.imm);
                        break;
                    case 0x2:   //LW
                        regs[inst.rd] = READ_32(regs[inst.rs1] + inst.imm);
                        break;
                    case 0x4:   //LBU
                        regs[inst.rd] = READ_8(regs[inst.rs1] + inst.imm);
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
                        WRITE_8(regs[inst.rs1] + inst.imm, regs[inst.rs2] & 0xFF);
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
                                regs[inst.rd] = ((int32_t)regs[inst.rs1] < (int32_t)regs[inst.rs2]) ? 1 : 0;
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
                                regs[inst.rd] = (int32_t)regs[inst.rs1] >> (regs[inst.rs2] & 0x1F);
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
                { 
                bool take = false;

                switch(inst.func3){
                    case 0x0:   //BEQ
                        take = regs[inst.rs1] == regs[inst.rs2];
                        break;
                    case 0x1:   //BNE
                        take = regs[inst.rs1] != regs[inst.rs2];
                        break;
                    case 0x4:   //BLT
                        take = (int32_t)regs[inst.rs1] < (int32_t)regs[inst.rs2];
                        break;
                    case 0x5:   //BGE
                        take = (int32_t)regs[inst.rs1] >= (int32_t)regs[inst.rs2];
                        break;
                    case 0x6:   //BLTU
                        take = regs[inst.rs1] < regs[inst.rs2];
                        break;
                    case 0x7:   //BGEU
                        take = regs[inst.rs1] >= regs[inst.rs2];
                        break;
                }

                bool prediction = btb.predict(PC - 4);

                if(prediction == take){
                    btb.correct++;
                }
                else{
                    cycle_count += 2; //Simulating pipeline flush due to misprediction
                }

                btb.update(PC - 4, take); //Updating the table

                if(take){
                    PC = (PC - 4) + inst.imm; //Executing the actual instruction
                }
                break;
            }
            case 0x67:
                switch(inst.func3){
                    case 0x0:   //JALR
                        uint32_t targ = (regs[inst.rs1] + inst.imm) & ~1;
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
                if(inst.func3 == 0x0){
                    if(inst.func7 == 0x18 && inst.rs2 == 0x2){
                        PC = mepc;  //Restoring PC

                        //Restoring interrupts
                        uint32_t mpie_bit = (mstatus >> 7) & 1;
                        mstatus &= ~(1 << 3);
                        mstatus |= (mpie_bit << 3);
                        mstatus |= (1 << 7);
                    }
                    switch(inst.imm){
                        case 0x0:   //ECALL
                            switch(regs[17]){
                                case 93:
                                    std::cout<<"\n[Emulator] Program exited with code "<<regs[10]<<std::endl;
                                    running = false;
                                    break;
                                case 64:
                                    if(regs[10] == 1 || regs[10] == 2){
                                        for(uint32_t i = 0; i<regs[12]; i++){
                                            char c = (char)READ_8(regs[11] + i);
                                            std::cout << c;
                                        }
                                    }
                            }
                            break;
                        case 0x1:   //EBREAK
                            std::cout << "Breakpoint hit at PC: " << std::hex << (PC-4) << std::endl;
                            break;
                    }
                    break;
                }
                else{
                    uint32_t csr_addr = inst.imm & 0xFFF;
                    uint32_t old_val = csrs[csr_addr];
                    
                    if(csr_addr == 0x300) old_val = mstatus;
                    if(csr_addr == 0x305) old_val = mtvec;
                    if(csr_addr == 0x341) old_val = mepc;
                    if(csr_addr == 0x342) old_val = mcause;
                    
                    if (inst.rd != 0) regs[inst.rd] = old_val;

                    uint32_t new_val = old_val;

                    switch (inst.func3) {
                        case 0x1: // CSRRW
                            new_val = regs[inst.rs1];
                            break;
                        
                        case 0x2: // CSRRS
                            new_val |= regs[inst.rs1];
                            break;
                        
                        case 0x3: // CSRRC
                            new_val &= ~regs[inst.rs1];
                            break;

                        case 0x5: // CSRRWI
                            new_val = inst.rs1; 
                            break;
                        
                        case 0x6: // CSRRSI
                            new_val |= inst.rs1;
                            break;
                        
                        case 0x7: // CSRRCI
                            new_val &= ~inst.rs1;
                            break;
                    }
                    csrs[csr_addr] = new_val;

                    if(csr_addr == 0x300) mstatus = new_val;
                    if(csr_addr == 0x305) mtvec = new_val;
                    if(csr_addr == 0x341) mepc = new_val;
                    if(csr_addr == 0x342) mcause = new_val;
                }
                break;
            }

        regs[0] = 0;
    }

    void checkInterrupt(){  //Runs every cycle and checks interrupts

        uint64_t current_time = ((uint64_t)csrs[MCYCLE_H] << 32) | csrs[MCYCLE_L];

        if(current_time >= mtimecmp) {
            csrs[0x344] |= (1 << 7);
        }

        bool g_enable = (mstatus >> 3) & 1; //Checking Global interrupt enable
        if(!g_enable) return;

        bool timer_enable = (csrs[0x304] >> 7) & 1; //Checking timer interrupt enable

        bool timer_pending = (csrs[0x344] >> 7) & 1;

        if(timer_enable && timer_pending){
            mepc = PC;// Saving the current PC

            mcause = 0x80000007; //Setting the cause to Machine Timer Interrupt

            uint32_t mie_bit = (mstatus >> 3) & 1;
            mstatus &= ~(1 << 3);
            mstatus |= (mie_bit << 7);
            
            PC = mtvec;// Jumping to handler
        }
    }

    void RUN(std::string FileName){ // Runs the program loop and Instruction Cycle
        if(!LOAD_FILE(FileName)) {
            std::cerr<<"\nError: Cannot open file \""<<FileName<<"\"\n";
            return;
        }

        running = true;

        while(running){
            uint32_t current_pc = PC;

            checkInterrupt();

            if(!Check_Permission(PC, 1)){   //Checking if address has Execute Permission
                std::cerr << "Fatal Error: Segmentation Fault (Instruction Fetch)" << std::endl;
                std::cerr.flush();
                Dump_Trace();
                running = false;
                break;
            }

            uint32_t raw = FETCH();

            Log_Trace(PC, raw); //Loggin Trace after each fetch

            PC += 4;

            Decoded_Instruction inst = DECODE(raw);

            cycle_count++;
            inst_count++;

            csrs[MCYCLE_L]    = cycle_count & 0xFFFFFFFF;
            csrs[MCYCLE_H]   = cycle_count >> 32;
            csrs[MINSTRET_L]  = inst_count & 0xFFFFFFFF;
            csrs[MINSTRET_H] = inst_count >> 32;

            EXECUTE(inst);

            if(PC - MEM_Offset >= MAX_MEMORY){
                running = false;
            }
        }
    }
};


int main(int argc, char* argv[]) {
    if(argc < 2){
        std::cout << "Usage: ./emulator <elf_file>" << std::endl;
        return 1;
    }

    RISC_V CPU;
    std::string filename = argv[1];

    std::cout << "Starting Emulator..." << std::endl;
    CPU.RUN(filename);
    
    return 0;
}