#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "inc/ssd1306.h"
#include "hardware/adc.h"

#define VRX 27 // input 1 s:24 h:1885 b:4090
#define VRY 26 // input 0 s:24 h:2087 b:4090
#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13
#define BUTTON_BOOTSEL 22
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

ssd1306_t ssd; // Inicializa a estrutura do display


volatile uint32_t last_time;
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_time > 200000){
        if (!gpio_get(BUTTON_BOOTSEL)) {
            last_time = current_time;
            rom_reset_usb_boot(0, 0);
        }
    }   
}

volatile uint y_borda = 3;
volatile uint x_borda = 3;
void handle_display_rect(uint16_t vrx_value, uint16_t vry_value){
            int y_pos = vry_value/(4090/64);
            int x_pos = vrx_value/(4090/127);

            if (y_pos >= HEIGHT - (8 + y_borda)) // Impossibilita escapar da borda de baixo
                y_pos = HEIGHT - (8 + y_borda);
            
            if (y_pos <= y_borda) // Impossibilita escapar da borda de cima
                y_pos = y_borda;
            
            if (x_pos >= WIDTH - (8 + x_borda)) // Impossibilita escapar da borda da direita
                x_pos = WIDTH - (8 + x_borda);
            
            if (x_pos <= x_borda) // Impossibilita escapar da borda da direita
                x_pos = x_borda;
                
            ssd1306_rect(&ssd, y_pos, x_pos, 8, 8, true, true); // Desenha um retângulo
            ssd1306_send_data(&ssd); // LEMBRAR DE FAZER QUE NÂO SAIA DA BORDA
            printf("x: %d, y: %d\n", x_pos, y_pos);
}

void blit(){
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);

}

int main()
{
    stdio_init_all();
    adc_init();
    i2c_init(I2C_PORT, 400 * 1000); // Inicia o i2c com 400kHz

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    adc_gpio_init(27);
    adc_gpio_init(26);

    gpio_init(BUTTON_BOOTSEL);
    gpio_set_dir(BUTTON_BOOTSEL, GPIO_IN);
    gpio_pull_up(BUTTON_BOOTSEL);

    gpio_set_irq_enabled_with_callback(BUTTON_BOOTSEL, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {

        blit();
        adc_select_input(0);
        uint16_t vry_value = adc_read();

        adc_select_input(1);
        uint16_t vrx_value = adc_read();
        
        handle_display_rect(vrx_value, vry_value);

        sleep_ms(200);
    }
}
