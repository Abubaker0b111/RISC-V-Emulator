#define UART_TX (*(volatile char *)0x10000000)

void print_char(char c) {
    UART_TX = c;
}

void print_str(const char *s) {
    while (*s) {
        print_char(*s++);
    }
}

void _start() {
    print_str("\nHello! This is running on a custom RISC-V Emulator.\n");
    
    // Exit code 0
    asm volatile ("li a7, 93; li a0, 0; ecall");
}