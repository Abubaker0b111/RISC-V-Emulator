#define UART (*(volatile char *)0x10000000)
#define UART_STATUS (*(volatile char *)0x10000005)

char get_char() {
    // Wait until key pressed (status bit 1)
    while (UART_STATUS == 0); 
    return UART;
}

void print(const char *s) {
    while (*s) UART = *s++;
}

void _start() {
    int target = 5; // Fixed "random" number for demo
    char input;
    
    print("\n--- Guess the Number (0-9) ---\n");
    print("I am thinking of a number...\n");

    while (1) {
        print("Your Guess: ");
        input = get_char();
        UART = input; // Echo back
        UART = '\n';

        int guess = input - '0';

        if (guess == target) {
            print("CORRECT! You win!\n");
            break;
        } else if (guess < target) {
            print("Too Low!\n");
        } else {
            print("Too High!\n");
        }
    }
    asm volatile ("li a7, 93; li a0, 0; ecall");
}