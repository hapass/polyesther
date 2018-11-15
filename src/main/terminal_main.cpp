#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;

//renderer
const int screen_width = 50;
const int screen_height = 50;
const string pixel = " ";
const string command = "\x1b[";
const string color_background = "48;5;";
const string set_color_command = "m";
const string cursor_up_command = "A";
const string cursor_left_command = "D";

vector<unsigned short> frame_buffer(screen_width * screen_height);

bool isValidColor(int color) {
    return 0 <= color && color <= 15;
}

//8-15
string change_color(int color) {
    if(!isValidColor(color))
        throw invalid_argument("color");

    stringstream change_color_command;

    change_color_command <<
        command << color_background << color << set_color_command;

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
    stringstream backbuffer;
    bool should_color_pixel = true;
    for(int i = 0; i < frame_buffer.size(); i++) {
        unsigned short pixel_color = frame_buffer[i];

        if(should_color_pixel) {
            backbuffer << change_color(pixel_color);
        }

        backbuffer << pixel;

        if((i + 1) % screen_width == 0) {
            backbuffer << '\n';
        }

        should_color_pixel = pixel_color != frame_buffer[i + 1];
    }

    backbuffer << reset_cursor();

    cout << backbuffer.str();
}

void clear(unsigned short color) {
    std::fill(frame_buffer.begin(), frame_buffer.end(), color);
}

void draw_pixel(int x, int y, unsigned short color) {
    int index = y * screen_width + x;
    frame_buffer[index] = color;
}

//stars demo
float star_speed = 0.01;
int stars_count = 200;
int fps = 25;

vector<float> star_x(stars_count);
vector<float> star_y(stars_count);
vector<float> star_z(stars_count);

float generate_random() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void create_star(int i) {
    star_x[i] = generate_random() * 2 - 1;
    star_y[i] = generate_random() * 2 - 1;
    star_z[i] = generate_random();
}

bool within_screen(int x, int y) {
    return (0 <= x && x < screen_width) &&
           (0 <= y && y < screen_height);
}

bool too_close(float z) {
    return z <= 0;
}

void draw_stars(chrono::milliseconds delta) {
    clear(8);

    auto delta_seconds = chrono::duration_cast<chrono::seconds>(delta);
    for(int i = 0; i < stars_count; i++) {
        star_z[i] -= star_speed;
        
        float z = star_z[i];
        if(too_close(z)) {
            create_star(i);
            continue;
        }

        int half_width = screen_width / 2;
        int half_height = screen_height / 2;

        int x = static_cast<int>((star_x[i]/z) * half_width + half_width);
        int y = static_cast<int>((star_y[i]/z) * half_height + half_height);

        if(!within_screen(x, y)) {
            create_star(i);
            continue;
        }

        draw_pixel(x, y, 11);
    }

    swap_buffers();
}

int main() {
    cout << '\n';

    for(int i = 0; i < stars_count; i++) {
        create_star(i);
    }

    bool isFinished = false;
    auto frame_time = chrono::milliseconds(1000 / fps);
    auto total_time = 0ms;

    while(!isFinished) {
        auto start = chrono::steady_clock::now();

        draw_stars(frame_time);

        auto finish = chrono::steady_clock::now();
        auto time_left = frame_time - (finish - start);

        if(time_left > 0ms) {
            this_thread::sleep_for(time_left);
        }

        total_time += frame_time;

        if(total_time > 30s) {
            isFinished = true;
        }
    }

    cout << command << 0 << set_color_command;

    cout << '\n';
    return 0;
}