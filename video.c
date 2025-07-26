#include "address_map_arm.h"
#include <stdlib.h>
#include <stdbool.h>

/* function prototypes */
void video_text(int, int, char *);
void video_box(int, int, int, int, short);
int  resample_rgb(int, int);
int  get_data_bits(int);
void delay(int);
void clear_text_area(int, int, int);

#define STANDARD_X 320
#define STANDARD_Y 240
#define INTEL_BLUE 0x0071C5
#define GREEN 0x00FF00
#define YELLOW 0xFFFF00
#define WHITE 0xFFFFFF
#define BLACK 0x000000
#define RED 0xFF0000

/* Simplified constants */
#define BIRD_SIZE 10
#define BIRD_X 80
#define GRAVITY -1
#define JUMP_STRENGTH 8
#define PIPE_WIDTH 30
#define PIPE_GAP 80
#define PIPE_SPEED 2

/* global variables */
int screen_x;
int screen_y;
int res_offset;
int col_offset;

/* Simple game variables */
int bird_y, bird_velocity;
int pipe_x, pipe_gap_y;
int score;
bool game_active;


/*******************************************************************************
 * Simplified Flappy Bird - Press KEY0 to jump
 ******************************************************************************/
int main(void) {
    volatile int * video_resolution = (int *)(PIXEL_BUF_CTRL_BASE + 0x8);
    screen_x = *video_resolution & 0xFFFF;
    screen_y = (*video_resolution >> 16) & 0xFFFF;

    volatile int * rgb_status = (int *)(RGB_RESAMPLER_BASE);
    int db = get_data_bits(*rgb_status & 0x3F);

    /* check if resolution is smaller than the standard 320 x 240 */
    res_offset = (screen_x == 160) ? 1 : 0;

    /* check if number of data bits is less than the standard 16-bits */
    col_offset = (db == 8) ? 1 : 0;

    /* update colors */
    short blue_color = resample_rgb(db, INTEL_BLUE);
    short green_color = resample_rgb(db, GREEN);
    short yellow_color = resample_rgb(db, YELLOW);
    short red_color = resample_rgb(db, RED);

    // Initialize game
    bird_y = STANDARD_Y / 2;
    bird_velocity = 0;
    pipe_x = STANDARD_X;
    pipe_gap_y = 80;
    score = 0;
    game_active = true; // Start game immediately

    while (1) {
        volatile int * key_ptr = (int *)KEY_BASE;
        
        if (game_active) {
            // Game running
            
            // Check for jump input
            if ((*key_ptr & 0x1) == 0) { // KEY0 pressed
                bird_velocity = JUMP_STRENGTH;
                delay(50000); // Simple debounce
            }
            
            // Update bird physics
            bird_velocity += GRAVITY;
            bird_y += bird_velocity;
            
            // Update pipe
            pipe_x -= PIPE_SPEED;
            if (pipe_x < -PIPE_WIDTH) {
                pipe_x = STANDARD_X;
                pipe_gap_y = 60 + (rand() % 80);
                score++;
            }
            
            // Check collisions
            if (bird_y < 0 || bird_y > STANDARD_Y - BIRD_SIZE) {
                game_active = false;
            }
            
            // Check pipe collision
            if (BIRD_X + BIRD_SIZE > pipe_x && BIRD_X < pipe_x + PIPE_WIDTH) {
                if (bird_y < pipe_gap_y || bird_y + BIRD_SIZE > pipe_gap_y + PIPE_GAP) {
                    game_active = false;
                }
            }
            
            // Clear screen
            video_box(0, 0, STANDARD_X, STANDARD_Y, blue_color);
            
            // Draw pipe
            if (pipe_gap_y > 0) {
                video_box(pipe_x, 0, pipe_x + PIPE_WIDTH - 1, pipe_gap_y - 1, green_color);
            }
            if (pipe_gap_y + PIPE_GAP < STANDARD_Y) {
                video_box(pipe_x, pipe_gap_y + PIPE_GAP, pipe_x + PIPE_WIDTH - 1, STANDARD_Y - 1, green_color);
            }
            
            // Draw bird
            video_box(BIRD_X, bird_y, BIRD_X + BIRD_SIZE - 1, bird_y + BIRD_SIZE - 1, yellow_color);
            
            // Show score
            video_text(1, 1, "Score:");
            if (score == 0) {
                video_text(7, 1, "0");
            } else if (score == 1) {
                video_text(7, 1, "1");
            } else if (score == 2) {
                video_text(7, 1, "2");
            } else if (score == 3) {
                video_text(7, 1, "3");
            } else if (score == 4) {
                video_text(7, 1, "4");
            } else if (score == 5) {
                video_text(7, 1, "5");
            } else if (score > 5) {
                video_text(7, 1, "5+");
            }
            
            delay(30000);
            
        } else {
            // GAME OVER - Tela vermelha com mensagem
            video_box(0, 0, STANDARD_X, STANDARD_Y, red_color);
            video_text(35, 29, "GAME OVER");
            video_text(32, 30, "Press KEY1 restart");
            
            // Aguarda KEY1 para reiniciar
            if ((*key_ptr & 0x2) != 0) { // KEY1 pressed
                clear_text_area(35, 29, 9);  // Limpa "GAME OVER" (9 caracteres)
                clear_text_area(32, 30, 18); // Limpa "Press KEY1 restart" (18 caracteres)
                game_active = true;
                bird_y = STANDARD_Y / 2;
                bird_velocity = 0;
                pipe_x = STANDARD_X;
                pipe_gap_y = 80;
                score = 0;
                delay(200000);
            }
        }
    }
    
    return 0;
}

/*******************************************************************************
 * Simple delay function
 ******************************************************************************/
void delay(int count) {
    volatile int i;
    for (i = 0; i < count; i++);
}

/*******************************************************************************
 * Subroutine to send a string of text to the video monitor
 ******************************************************************************/
void video_text(int x, int y, char * text_ptr) {
    int offset;
    volatile char * character_buffer = (char *)FPGA_CHAR_BASE;

    offset = (y << 7) + x;
    while (*(text_ptr)) {
        *(character_buffer + offset) = *(text_ptr);
        ++text_ptr;
        ++offset;
    }
}

/*******************************************************************************
 * Draw a filled rectangle on the video monitor
 * Takes in points assuming 320x240 resolution and adjusts based on differences
 * in resolution and color bits.
 ******************************************************************************/
void video_box(int x1, int y1, int x2, int y2, short pixel_color) {
    int pixel_buf_ptr = *(int *)PIXEL_BUF_CTRL_BASE;
    int pixel_ptr, row, col;
    int x_factor = 0x1 << (res_offset + col_offset);
    int y_factor = 0x1 << (res_offset);
    x1 = x1 / x_factor;
    x2 = x2 / x_factor;
    y1 = y1 / y_factor;
    y2 = y2 / y_factor;

    /* assume that the box coordinates are valid */
    for (row = y1; row <= y2; row++)
        for (col = x1; col <= x2; ++col) {
            pixel_ptr = pixel_buf_ptr + (row << (10 - res_offset - col_offset)) + (col << 1);
            *(short *)pixel_ptr = pixel_color;
        }
}

/********************************************************************************
 * Resamples 24-bit color to 16-bit or 8-bit color
 *******************************************************************************/
int resample_rgb(int num_bits, int color) {
    if (num_bits == 8) {
        color = (((color >> 16) & 0x000000E0) | ((color >> 11) & 0x0000001C) |
                 ((color >> 6) & 0x00000003));
        color = (color << 8) | color;
    } else if (num_bits == 16) {
        color = (((color >> 8) & 0x0000F800) | ((color >> 5) & 0x000007E0) |
                 ((color >> 3) & 0x0000001F));
    }
    return color;
}

/********************************************************************************
 * Finds the number of data bits from the mode
 *******************************************************************************/
int get_data_bits(int mode) {
    switch (mode) {
    case 0x0: return 1;
    case 0x7: return 8;
    case 0x11: return 8;
    case 0x12: return 9;
    case 0x14: return 16;
    case 0x17: return 24;
    case 0x19: return 30;
    case 0x31: return 8;
    case 0x32: return 12;
    case 0x33: return 16;
    case 0x37: return 32;
    case 0x39: return 40;
    default: return 16;
    }
}

void clear_text_area(int x, int y, int width) {
    volatile char * character_buffer = (char *)FPGA_CHAR_BASE;
    int offset = (y << 7) + x;
    int i;
    
    for (i = 0; i < width; i++) {
        *(character_buffer + offset + i) = ' '; // Espaço em branco
    }
}