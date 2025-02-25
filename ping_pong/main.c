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
#define BUTTON_B 6       // Botão B: sem função por enquanto

// Pinos dos LEDs
#define LED_R 13         // LED vermelho (game over)
#define LED_G 11         // LED verde (jogo em andamento)
#define LED_B 12         // LED azul (aguardando início ou pausado)

// Dimensões do display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Estados do jogo
typedef enum { WAITING, PLAYING, PAUSED, GAME_OVER } game_state_t;
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
    ball.vx = 2;
    ball.vy = 2;
}

// Atualiza o desenho dos elementos na tela
void draw_game() {
    ssd1306_fill(&ssd, false);
    
    if (game_state == WAITING) {
        int instr1_width = 8 * 8;
        int instr2_width = 8 * 13;
        ssd1306_draw_string(&ssd, "aperte A", (SCREEN_WIDTH - instr1_width) / 2, 24);
        ssd1306_draw_string(&ssd, "para comecar", (SCREEN_WIDTH - instr2_width) / 2, 32);
    } else {
        ssd1306_rect(&ssd, player.x, player.y, player.width, player.height, true, true);
        ssd1306_rect(&ssd, cpu.x, cpu.y, cpu.width, cpu.height, true, true);
        ssd1306_rect(&ssd, ball.x, ball.y, ball.size, ball.size, true, true);
        
        if (game_state == PAUSED) {
            ssd1306_draw_string(&ssd, "pause", (SCREEN_WIDTH - 5*8) / 2, 30);
        }
        if (game_state == GAME_OVER) {
            ssd1306_draw_string(&ssd, "game over", (SCREEN_WIDTH - 9*8) / 2, 30);
        }
    }
    
    ssd1306_send_data(&ssd);
}

// Atualiza a lógica do jogo
void update_game() {
    // Atualiza a raquete do jogador com base no joystick (eixo X, GPIO 27)
    uint16_t x_adc = adc_read();
    // Valores iniciais para teste (substitua pelos reais após depuração)
    const uint16_t MIN_ADC = 550;  // Valor mínimo real do ADC (esquerda)
    const uint16_t MAX_ADC = 3450; // Valor máximo real do ADC (direita)
    
    // Normaliza o valor do ADC
    uint16_t normalized_adc;
    if (x_adc < MIN_ADC) {
        normalized_adc = 0;
    } else if (x_adc > MAX_ADC) {
        normalized_adc = MAX_ADC - MIN_ADC;
    } else {
        normalized_adc = x_adc - MIN_ADC;
    }
    
    // Remove a inversão para corrigir a direção (direita = x maior)
    player.x = ((uint32_t)normalized_adc * (SCREEN_WIDTH - player.width)) / (MAX_ADC - MIN_ADC);
    
    // Depuração para encontrar MIN_ADC e MAX_ADC reais
    static uint16_t min_observed = 4095;
    static uint16_t max_observed = 0;
    if (x_adc < min_observed) min_observed = x_adc;
    if (x_adc > max_observed) max_observed = x_adc;
    printf("ADC: %u, Player.x: %d, Min: %u, Max: %u\n", x_adc, player.x, min_observed, max_observed);
    
    // AI simples para a raquete da CPU
    int cpu_center = cpu.x + cpu.width / 2;
    int ball_center = ball.x + ball.size / 2;
    if (ball_center > cpu_center && cpu.x + cpu.width < SCREEN_WIDTH)
        cpu.x += 1;
    else if (ball_center < cpu_center && cpu.x > 0)
        cpu.x -= 1;
    
    // Atualiza a posição da bola
    ball.x += ball.vx;
    ball.y += ball.vy;
    
    // Colisão com as paredes laterais
    if (ball.x <= 0 || ball.x + ball.size >= SCREEN_WIDTH)
        ball.vx = -ball.vx;
    
    // Colisão com a raquete da CPU
    if (ball.y <= cpu.y + cpu.height) {
        if (ball.x + ball.size >= cpu.x && ball.x <= cpu.x + cpu.width) {
            ball.vy = -ball.vy;
            ball.y = cpu.y + cpu.height;
        } else if (ball.y < 0) {
            game_state = GAME_OVER;
        }
    }
    
    // Colisão com a raquete do jogador
    if (ball.y + ball.size >= player.y) {
        if (ball.x + ball.size >= player.x && ball.x <= player.x + player.width) {
            ball.vy = -ball.vy;
            ball.y = player.y - ball.size;
        } else if (ball.y + ball.size > SCREEN_HEIGHT) {
            game_state = GAME_OVER;
        }
    }
}

// Callback para interrupção dos botões
void gpio_callback(uint gpio, uint32_t events) {
    static uint32_t last_time_A = 0, last_time_J = 0;
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
}

int main() {
    stdio_init_all();
    
    // Configura o I2C e o display SSD1306
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
    
    // Inicializa o ADC para o joystick (GPIO 27, canal 1)
    adc_init();
    adc_gpio_init(VRX_PIN); // GPIO 27
    adc_select_input(1);    // Canal 1 (GPIO 27)
    
    // Configura os botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
    
    // Configura os LEDs
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_put(LED_G, false);
    
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_put(LED_R, false);
    
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_put(LED_B, false);
    
    // Registra a callback para os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    while (true) {
        if (game_state == PLAYING)
            update_game();
        
        // Atualiza os LEDs
        gpio_put(LED_B, game_state == WAITING || game_state == PAUSED); // Azul para WAITING e PAUSED
        gpio_put(LED_G, game_state == PLAYING); // Verde apenas para PLAYING
        gpio_put(LED_R, game_state == GAME_OVER); // Vermelho para GAME_OVER
        
        draw_game();
        sleep_ms(50);
    }
    
    return 0;
}