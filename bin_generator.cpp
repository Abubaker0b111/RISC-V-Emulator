#include <fstream>

int main() {
    unsigned char code[] = {
        0x93, 0x00, 0x10, 0x00, // addi x1, x0, 1
        0x93, 0x02, 0xf0, 0xff, // addi x5, x0, -1
        0xe3, 0x08, 0x04, 0xfe  // beq x8, x0, -16
    };
    std::ofstream file("test.bin", std::ios::binary);
    file.write((char*)code, sizeof(code));
    file.close();
    return 0;
}