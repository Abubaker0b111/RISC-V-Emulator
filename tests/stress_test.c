// --------------------------------------------------------------------
// 1. DRIVERS & HELPERS
// --------------------------------------------------------------------
#define UART_TX (*(volatile char *)0x10000000)

void uart_putc(char c) { UART_TX = c; }

void print_str(const char *str) {
    while (*str) uart_putc(*str++);
}

void print_hex(unsigned long num) {
    print_str("0x");
    for (int i = 28; i >= 0; i -= 4) {
        unsigned char nibble = (num >> i) & 0xF;
        uart_putc(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
    }
}

void assert(int condition, const char* name) {
    print_str(name);
    print_str(": ");
    if (condition) print_str("[PASS]\n");
    else {
        print_str("[FAIL]\n");
        // We do not exit, we try to run the next test
    }
}

// SOFTWARE MATH (Standard RV32I helpers)
long __mulsi3(long a, long b) {
    long r = 0;
    while (b) {
        if (b & 1) r += a;
        a <<= 1;
        b = (unsigned long)b >> 1;
    }
    return r;
}

unsigned long __udivsi3(unsigned long n, unsigned long d) {
    unsigned long q = 0, r = 0;
    for (int i = 31; i >= 0; i--) {
        r <<= 1;
        r |= (n >> i) & 1;
        if (r >= d) { r -= d; q |= (1UL << i); }
    }
    return q;
}

long __divsi3(long a, long b) {
    int neg = 0;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }
    long q = __udivsi3(a, b);
    return neg ? -q : q;
}

long __modsi3(long a, long b) {
    int neg = 0;
    if (a < 0) { a = -a; neg = 1; }
    if (b < 0) { b = -b; }
    unsigned long ua = a, ub = b;
    unsigned long r = 0;
    for (int i = 31; i >= 0; i--) {
        r <<= 1;
        r |= (ua >> i) & 1;
        if (r >= ub) { r -= ub; }
    }
    return neg ? -r : r;
}

// --------------------------------------------------------------------
// 2. CSR DEFINITIONS
// --------------------------------------------------------------------
#define CSR_MSTATUS 0x300
#define CSR_MIE     0x304
#define CSR_MTVEC   0x305
#define CSR_MSCRATCH 0x340 
#define CSR_MCAUSE   0x342
#define CSR_MEPC     0x341

volatile unsigned long long *mtimecmp = (unsigned long long*)0x02004000;
volatile unsigned long long *mtime    = (unsigned long long*)0x0200BFF8;

void write_csr(int reg, long val) {
    if (reg == CSR_MTVEC) asm volatile ("csrw mtvec, %0" :: "r"(val));
    if (reg == CSR_MIE)   asm volatile ("csrw mie, %0"   :: "r"(val));
    if (reg == CSR_MSTATUS) asm volatile ("csrw mstatus, %0" :: "r"(val));
    if (reg == CSR_MSCRATCH) asm volatile ("csrw mscratch, %0" :: "r"(val));
}

long read_csr(int reg) {
    long val;
    if (reg == CSR_MSCRATCH) asm volatile ("csrr %0, mscratch" : "=r"(val));
    if (reg == CSR_MCAUSE)   asm volatile ("csrr %0, mcause"   : "=r"(val));
    if (reg == CSR_MEPC)     asm volatile ("csrr %0, mepc"     : "=r"(val));
    return val;
}

// --------------------------------------------------------------------
// 3. STRICT TESTS
// --------------------------------------------------------------------

void test_signed_vs_unsigned() {
    int a = -1;  // 0xFFFFFFFF
    int b = 1;   // 0x00000001
    
    // Signed: -1 is LESS than 1
    int slt_res = (a < b); 
    
    // Unsigned: 0xFFFFFFFF (4 Billion) is GREATER than 1
    int sltu_res = ((unsigned int)a < (unsigned int)b);

    assert(slt_res == 1 && sltu_res == 0, "Signed vs Unsigned Logic");
}

void test_endianness() {
    // We write a 32-bit word: 0x12345678
    // In Memory (Little Endian) it should look like:
    // Addr+0: 0x78
    // Addr+1: 0x56
    // Addr+2: 0x34
    // Addr+3: 0x12
    
    volatile unsigned int val = 0x12345678;
    volatile unsigned char* bytes = (volatile unsigned char*)&val;
    
    int pass = 1;
    if (bytes[0] != 0x78) pass = 0;
    if (bytes[1] != 0x56) pass = 0;
    if (bytes[2] != 0x34) pass = 0;
    if (bytes[3] != 0x12) pass = 0;
    
    assert(pass, "Little-Endian Memory");
}

void test_csr_bit_manipulation() {
    // 1. Write Initial Pattern (Binary 0101)
    write_csr(CSR_MSCRATCH, 0x5);
    
    // 2. CSRRS (Set Bit 1 -> Binary 0111 -> 0x7)
    // We use inline assembly to force the exact instruction
    long val = 0x2;
    asm volatile ("csrs mscratch, %0" :: "r"(val));
    
    long res1 = read_csr(CSR_MSCRATCH);
    
    // 3. CSRRC (Clear Bit 0 -> Binary 0110 -> 0x6)
    val = 0x1;
    asm volatile ("csrc mscratch, %0" :: "r"(val));
    
    long res2 = read_csr(CSR_MSCRATCH);

    assert(res1 == 0x7 && res2 == 0x6, "CSR Bitwise Ops (Set/Clear)");
}

void test_shifts() {
    int val = -4; // 0xFFFFFFFC
    
    // Arithmetic Shift Right (preserves sign bit) -> -2 (0xFFFFFFFE)
    int sra = val >> 1;
    
    // Logical Shift Right (fills with zeros) -> 0x7FFFFFFE
    unsigned int srl = (unsigned int)val >> 1;
    
    assert(sra == -2 && srl == 0x7FFFFFFE, "Arithmetic vs Logical Shift");
}

// --------------------------------------------------------------------
// 4. INTERRUPT CONTEXT CHECK
// --------------------------------------------------------------------

void __attribute__((interrupt("machine"))) strict_handler() {
    // Check 1: Did we land here because of a timer?
    long cause = read_csr(CSR_MCAUSE);
    
    // Check 2: Is the MEPC valid? (Should be > 0x80000000)
    long epc = read_csr(CSR_MEPC);
    
    // Store results in scratch for main loop to verify
    if (cause == 0x80000007 && epc > 0x80000000) {
        write_csr(CSR_MSCRATCH, 1); // PASS
    } else {
        write_csr(CSR_MSCRATCH, 0xBAD); // FAIL code
    }

    *mtimecmp = *mtime + 100000; 
}

void test_interrupt_context() {
    write_csr(CSR_MSCRATCH, 0); // Clear Flag
    write_csr(CSR_MTVEC, (long)&strict_handler);
    write_csr(CSR_MIE, 0x80);
    write_csr(CSR_MSTATUS, 0x8);

    *mtimecmp = *mtime + 100;

    int timeout = 100000;
    while (read_csr(CSR_MSCRATCH) == 0 && timeout > 0) {
        timeout--;
    }

    long res = read_csr(CSR_MSCRATCH);
    assert(res == 1, "Interrupt Cause & Context");
}

// --------------------------------------------------------------------
// 5. MAIN
// --------------------------------------------------------------------
void _start() {
    print_str("\n=== STRICT MODE TEST SUITE ===\n\n");

    test_signed_vs_unsigned();
    test_shifts();
    test_endianness();
    test_csr_bit_manipulation();
    test_interrupt_context();

    print_str("\n=== SUITE COMPLETE ===\n");
    asm volatile ("li a7, 93; li a0, 0; ecall");
}