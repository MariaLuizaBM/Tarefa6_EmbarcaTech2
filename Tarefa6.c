#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lib/font.h"
#include "pico/bootrom.h"
#include <stdio.h>

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define Button_Joystick 22
#define Button_A 5
#define Button_B 6
#define LED_BLUE 12
#define LED_GREEN 11
#define LED_RED 13
#define BUZZER_PIN 21
#define MAX_USUARIOS 10

absolute_time_t last_press_A = 0;
absolute_time_t last_press_B = 0;
absolute_time_t last_press_J = 0;

#define DEBOUNCE_TIME_MS 200

ssd1306_t ssd;

TaskHandle_t taskHandleEntrada = NULL;
TaskHandle_t taskHandleSaida = NULL;
TaskHandle_t taskHandleReset = NULL;

SemaphoreHandle_t xMutex;
SemaphoreHandle_t xSemEntrada;
SemaphoreHandle_t xSemSaida;
SemaphoreHandle_t xSemReset;

uint16_t usuarios_ativos = 0;
uint slice_num;


// ------------------- Funções -------------------

void gpio_irq_handler(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    absolute_time_t now = get_absolute_time();

    switch (gpio) {
        case Button_A:
            if (absolute_time_diff_us(last_press_A, now) > DEBOUNCE_TIME_MS * 1000) {
                last_press_A = now;
                xSemaphoreGiveFromISR(xSemEntrada, &xHigherPriorityTaskWoken);
            }
            break;
        case Button_B:
            if (absolute_time_diff_us(last_press_B, now) > DEBOUNCE_TIME_MS * 1000) {
                last_press_B = now;
                xSemaphoreGiveFromISR(xSemSaida, &xHigherPriorityTaskWoken);
            }
            break;
        case Button_Joystick:
            if (absolute_time_diff_us(last_press_J, now) > DEBOUNCE_TIME_MS * 1000) {
                last_press_J = now;
                xSemaphoreGiveFromISR(xSemReset, &xHigherPriorityTaskWoken);
            }
            break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void init_leds() {
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
}

void atualiza_leds() {
    if (usuarios_ativos == 0) {
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 0);
        gpio_put(LED_BLUE, 1);
    } else if (usuarios_ativos == (MAX_USUARIOS - 1)) {
        gpio_put(LED_RED, 1);
        gpio_put(LED_GREEN, 1);
        gpio_put(LED_BLUE, 0);
    } else if (usuarios_ativos == MAX_USUARIOS) {
        gpio_put(LED_RED, 1);
        gpio_put(LED_GREEN, 0);
        gpio_put(LED_BLUE, 0);
    } else {
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 1);
        gpio_put(LED_BLUE, 0);
    }
}

void init_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
}

void beep_pwm(int freq, int duracao_ms) {
    uint32_t clock = 125000000; // 125 MHz padrão
    uint32_t div = 4;
    uint32_t wrap = clock / div / freq - 1;

    pwm_set_clkdiv(slice_num, div);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER_PIN), wrap / 2);
    pwm_set_enabled(slice_num, true);

    vTaskDelay(pdMS_TO_TICKS(duracao_ms));

    pwm_set_enabled(slice_num, false);
}
void beep_simples() {
    beep_pwm(2000, 100);
}

void beep_duplo() {
    beep_pwm(2000, 100);
    vTaskDelay(pdMS_TO_TICKS(100));
    beep_pwm(2000, 100);
}

void config_button(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
}

void atualiza_display() {
    char buffer[32];
    ssd1306_fill(&ssd, 0);

    if (usuarios_ativos == 0) {
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);     // Moldura
        ssd1306_draw_string(&ssd, "Sistema:", 10, 12);      
        ssd1306_draw_string(&ssd, "VAZIO", 45, 24);      
        ssd1306_line(&ssd, 5, 35, 122, 35, true);           // Linha divisória
        sprintf(buffer, "Ocupacao:%d/%d", usuarios_ativos, MAX_USUARIOS);
        ssd1306_draw_string(&ssd, buffer, 10, 45);

    } else if (usuarios_ativos == MAX_USUARIOS) {
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);     // Moldura
        ssd1306_draw_string(&ssd, "Sistema:", 10, 12);     
        ssd1306_draw_string(&ssd, "CHEIO", 44, 24);    
        ssd1306_line(&ssd, 5, 35, 122, 35, true);           // Linha divisória
        sprintf(buffer, "Ocupacao:%d/%d", usuarios_ativos, MAX_USUARIOS);
        ssd1306_draw_string(&ssd, buffer, 10, 45);

    } else if (usuarios_ativos == (MAX_USUARIOS - 1)) {
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);     // Moldura
        ssd1306_draw_string(&ssd, "Sistema:", 10, 12);
        ssd1306_draw_string(&ssd, "QUASE CHEIO", 21, 24);
        ssd1306_line(&ssd, 5, 35, 122, 35, true);           // Linha divisória
        sprintf(buffer, "Ocupacao:%d/%d", usuarios_ativos, MAX_USUARIOS);
        ssd1306_draw_string(&ssd, buffer, 8, 45);
    } else {
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);     // Moldura
        ssd1306_draw_string(&ssd, "Sistema:", 10, 12);
        ssd1306_draw_string(&ssd, "LIVRE", 45, 24);
        ssd1306_line(&ssd, 5, 35, 122, 35, true);           // Linha divisória
        sprintf(buffer, "Ocupacao:%d/%d", usuarios_ativos, MAX_USUARIOS);
        ssd1306_draw_string(&ssd, buffer, 8, 45);
    }

    ssd1306_send_data(&ssd);
}

// ------------------- Tasks -------------------

void vTaskEntrada(void *params) {
    while (1) {
        if (xSemaphoreTake(xSemEntrada, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(xMutex, portMAX_DELAY);

            if (usuarios_ativos < MAX_USUARIOS) {
                usuarios_ativos++;
                atualiza_leds();
                atualiza_display();
            } else {
                beep_simples();
                atualiza_display();
            }

            xSemaphoreGive(xMutex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vTaskSaida(void *params) {
    while (1) {
        if (xSemaphoreTake(xSemSaida, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(xMutex, portMAX_DELAY);

            if (usuarios_ativos > 0) {
                usuarios_ativos--;
                atualiza_leds();
                atualiza_display();
            } else {
                atualiza_display();
            }

            xSemaphoreGive(xMutex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vTaskReset(void *params) {
    while (1) {
        if (xSemaphoreTake(xSemReset, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(xMutex, portMAX_DELAY);

            if (usuarios_ativos > 0) {
                usuarios_ativos = 0;
                beep_duplo();
                atualiza_leds();
                atualiza_display();
            } else {
                atualiza_display();
            }

            xSemaphoreGive(xMutex);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ------------------- Main -------------------

int main() {
    stdio_init_all();

    init_leds();
    init_buzzer();

// Inicialização do I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    config_button(Button_A);
    config_button(Button_B);
    config_button(Button_Joystick);

    xSemEntrada = xSemaphoreCreateCounting(10, 0);
    xSemSaida = xSemaphoreCreateBinary();
    xSemReset = xSemaphoreCreateBinary();
    xMutex = xSemaphoreCreateMutex();

    xTaskCreate(vTaskEntrada, "Task Entrada", 512, NULL, 1, &taskHandleEntrada);
    xTaskCreate(vTaskSaida, "Task Saida", 512, NULL, 1, &taskHandleSaida);
    xTaskCreate(vTaskReset, "Task Reset", 512, NULL, 1, &taskHandleReset);

    vTaskStartScheduler();

    panic_unsupported();
}


