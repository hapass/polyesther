#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;

const int screen_width = 30;
const int screen_height = 20;
const char pixel = '*';
const string command = "\x1b[";
const string color_palette_256 = "38;5;";
const string set_color_command = "m";
const string cursor_up_command = "A";
const string cursor_left_command = "D";

vector<unsigned short> frame_buffer(screen_width * screen_height);

string change_color(int color) {
    stringstream change_color_command;

    change_color_command <<
        command << color_palette_256 << color << set_color_command;

    return change_color_command.str();
}

string reset_cursor() {
    stringstream reset_cursor_command;

    reset_cursor_command <<
        command << screen_width << cursor_left_command;

    reset_cursor_command <<
        command << screen_height << cursor_up_command;

    return reset_cursor_command.str();
}

void swap_buffers() {
    bool should_color_pixel = true;
    for(int i = 0; i < frame_buffer.size(); i++) {
        unsigned char pixel_color = frame_buffer[i];

        if(should_color_pixel) {
            cout << change_color(pixel_color);
        }

        cout << pixel;

        if((i + 1) % screen_width == 0) {
            cout << '\n';
        }

        should_color_pixel = pixel_color != frame_buffer[i + 1];
    }
}

void clear(unsigned char color) {
    std::fill(frame_buffer.begin(), frame_buffer.end(), color);
}

int main() {
    cout << '\n';

    int color = 255;
    while(color >= 0) {
        clear(color);
        swap_buffers();
        cout << reset_cursor();
        color--;
        this_thread::sleep_for(20ms);
    }

    cout << command << 0 << set_color_command;

    cout << '\n';
    return 0;
}