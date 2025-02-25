#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "ssd1306.h"    // Biblioteca do display
#include "font.h"       // Fonte para o display

// Configurações do I2C e display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define I2C_ADDR 0x3C

// Pinos do joystick e botões
#define VRX_PIN 27       // Eixo X (horizontal) do joystick, controla a raquete
#define SW_PIN 22        // Botão do joystick: pausa/retoma
#define BUTTON_A 5       // Botão A: inicia o jogo
#define BUTTON_B 6       // Botão B: reinicia o jogo

// Pinos dos LEDs
#define LED_R 13         // LED vermelho (game over)
#define LED_G 11         // LED verde (jogo em andamento ou vitória)
#define LED_B 12         // LED azul (aguardando início ou pausado)

// Dimensões do display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Estados do jogo
typedef enum { WAITING, PLAYING, PAUSED, GAME_OVER, WIN } game_state_t;
volatile game_state_t game_state = WAITING;

// Estrutura do display
ssd1306_t ssd;

// Estruturas dos elementos do jogo
typedef struct {
    int x, y;
    int vx, vy;
    int size;
} Ball;

typedef struct {
    int x, y;
    int width, height;
} Paddle;

Ball ball;
Paddle player;
Paddle cpu;

// Inicializa as posições iniciais dos elementos do jogo
void init_game() {
    player.width = 20;
    player.height = 3;
    player.x = (SCREEN_WIDTH - player.width) / 2; // x=54
    player.y = SCREEN_HEIGHT - player.height - 1;
    
    cpu.width = 20;
    cpu.height = 3;
    cpu.x = (SCREEN_WIDTH - cpu.width) / 2; // x=54
    cpu.y = 1;
    
    ball.size = 8;
    ball.x = (SCREEN_WIDTH - ball.size) / 2; // x=60
    ball.y = SCREEN_HEIGHT / 2 - ball.size / 2; // y=28
    ball.vx = 3; // Velocidade aumentada
    ball.vy = 3; // Velocidade aumentada
}

// Atualiza o desenho dos elementos na tela
void draw_game() {
    ssd1306_fill(&ssd, false);
    
    if (game_state == PLAYING || game_state == PAUSED) {
        ssd1306_rect(&ssd, player.x, player.y, player.width, player.height, true, true);
        ssd1306_rect(&ssd, cpu.x, cpu.y, cpu.width, cpu.height, true, true);
        ssd1306_rect(&ssd, ball.x, ball.y, ball.size, ball.size, true, true);
    }
    
    if (game_state == WAITING) {
        int instr1_width = 8 * 8;
        int instr2_width = 8 * 13;
        ssd1306_draw_string(&ssd, "aperte A", (SCREEN_WIDTH - instr1_width) / 2, 24);
        ssd1306_draw_string(&ssd, "para comecar", (SCREEN_WIDTH - instr2_width) / 2, 32);
    } else if (game_state == PAUSED) {
        ssd1306_draw_string(&ssd, "pause", (SCREEN_WIDTH - 5*8) / 2, 30);
    } else if (game_state == GAME_OVER) {
        int game_over_width = 8 * 9;
        int restart1_width = 8 * 13;
        int restart2_width = 8 * 9;
        ssd1306_draw_string(&ssd, "game over", (SCREEN_WIDTH - game_over_width) / 2, 24);
        ssd1306_draw_string(&ssd, "aperte B para", (SCREEN_WIDTH - restart1_width) / 2, 34);
        ssd1306_draw_string(&ssd, "reiniciar", (SCREEN_WIDTH - restart2_width) / 2, 42);
    } else if (game_state == WIN) {
        int win_width = 8 * 11;
        int restart1_width = 8 * 13;
        int restart2_width = 8 * 9;
        ssd1306_draw_string(&ssd, "voce ganhou", (SCREEN_WIDTH - win_width) / 2, 24);
        ssd1306_draw_string(&ssd, "aperte B para", (SCREEN_WIDTH - restart1_width) / 2, 34);
        ssd1306_draw_string(&ssd, "reiniciar", (SCREEN_WIDTH - restart2_width) / 2, 42);
    }
    
    ssd1306_send_data(&ssd);
}

// Atualiza a lógica do jogo
void update_game() {
    uint16_t x_adc = adc_read();
    const uint16_t MIN_ADC = 600;  // Substitua pelo valor real mínimo (esquerda)
    const uint16_t MAX_ADC = 3400; // Substitua pelo valor real máximo (direita)
    const int PLAYER_SPEED = 3;    // Velocidade máxima da raquete do jogador
    
    // Calcula a posição desejada do jogador baseada no joystick
    uint16_t normalized_adc;
    if (x_adc < MIN_ADC) {
        normalized_adc = 0;
    } else if (x_adc > MAX_ADC) {
        normalized_adc = MAX_ADC - MIN_ADC;
    } else {
        normalized_adc = x_adc - MIN_ADC;
    }
    int target_x = ((uint32_t)normalized_adc * (SCREEN_WIDTH - player.width)) / (MAX_ADC - MIN_ADC);
    
    // Move a raquete do jogador lentamente
    if (player.x < target_x) {
        player.x += PLAYER_SPEED;
        if (player.x > target_x) player.x = target_x;
    } else if (player.x > target_x) {
        player.x -= PLAYER_SPEED;
        if (player.x < target_x) player.x = target_x;
    }
    
    // CPU acompanha a bola diretamente
    int ball_center = ball.x + ball.size / 2;
    int cpu_target_x = ball_center - cpu.width / 2;
    if (cpu_target_x < 0) cpu_target_x = 0;
    if (cpu_target_x + cpu.width > SCREEN_WIDTH) cpu_target_x = SCREEN_WIDTH - cpu.width;
    cpu.x = cpu_target_x;
    
    // Atualiza a posição da bola (velocidade aumentada)
    ball.x += ball.vx;
    ball.y += ball.vy;
    
    if (ball.x <= 0 || ball.x + ball.size >= SCREEN_WIDTH)
        ball.vx = -ball.vx;
    
    if (ball.y <= cpu.y + cpu.height) {
        if (ball.x + ball.size >= cpu.x && ball.x <= cpu.x + cpu.width) {
            ball.vy = -ball.vy;
            ball.y = cpu.y + cpu.height;
        } else if (ball.y <= 0) {
            game_state = WIN;
        }
    }
    
    if (ball.y + ball.size >= player.y) {
        if (ball.x + ball.size >= player.x && ball.x <= player.x + player.width) {
            ball.vy = -ball.vy;
            ball.y = player.y - ball.size;
        } else if (ball.y + ball.size >= SCREEN_HEIGHT) {
            game_state = GAME_OVER;
        }
    }
}

// Callback para interrupção dos botões
void gpio_callback(uint gpio, uint32_t events) {
    static uint32_t last_time_A = 0, last_time_J = 0, last_time_B = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (gpio == BUTTON_A && current_time - last_time_A >= 200) {
        last_time_A = current_time;
        if (game_state == WAITING) {
            init_game();
            game_state = PLAYING;
        }
    }
    
    if (gpio == SW_PIN && current_time - last_time_J >= 200) {
        last_time_J = current_time;
        if (game_state == PLAYING)
            game_state = PAUSED;
        else if (game_state == PAUSED)
            game_state = PLAYING;
    }
    
    if (gpio == BUTTON_B && current_time - last_time_B >= 200) {
        last_time_B = current_time;
        if (game_state == PLAYING || game_state == PAUSED || game_state == GAME_OVER || game_state == WIN) {
            init_game();
            game_state = WAITING;
        }
    }
}

int main() {
    stdio_init_all();
    
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, SCREEN_WIDTH, SCREEN_HEIGHT, false, I2C_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    sleep_ms(50);
    
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_select_input(1);
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
    
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_put(LED_G, false);
    
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_put(LED_R, false);
    
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_put(LED_B, false);
    
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    while (true) {
        if (game_state == PLAYING) {
            update_game();
        }
        
        gpio_put(LED_B, game_state == WAITING || game_state == PAUSED);
        gpio_put(LED_G, game_state == PLAYING || game_state == WIN);
        gpio_put(LED_R, game_state == GAME_OVER);
        
        draw_game();
        sleep_ms(50);
    }
    
    return 0;
}