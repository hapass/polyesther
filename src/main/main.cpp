#include <iostream>

int main() {
    std::cout << "Color test:" << "\n";

    for(unsigned char green = 0; green < 6; green++) {
        for (unsigned char red = 0; red < 6; red++) {
            for (unsigned char blue = 0; blue < 6; blue++) {
                short color = 16 + (red * 36) + (green * 6) + blue;
                std::cout << "\x1b[38;5;" << color << "m" << "*";
            }
        }
        std::cout << "\n";
    }
    
    return 0;
}