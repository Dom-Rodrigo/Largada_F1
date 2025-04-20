#include <stdio.h>
#include "pico/stdlib.h"
#include "ws2818b.pio.h"
#include "pico/bootrom.h"
#include "inc/ssd1306.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"



#define VRX 27 // input 1 s:24 h:1885 b:4090
#define VRY 26 // input 0 s:24 h:2087 b:4090
#define BUTTON_BOOTSEL 22
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C


ssd1306_t ssd; // Inicializa a estrutura do display

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7
#define START_COUNT_BUTTON 5

// Definição de pixel GRB
struct pixel_t
{
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin)
{

    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0)
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}
/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b)
{
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear()
{
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite()
{
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

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

    npInit(LED_PIN);
    npClear();

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

    gpio_init(START_COUNT_BUTTON);
    gpio_set_dir(START_COUNT_BUTTON, GPIO_IN);
    gpio_pull_up(START_COUNT_BUTTON);

    gpio_set_irq_enabled_with_callback(BUTTON_BOOTSEL, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    uint counted = 0;
    while (true) {
        if (!gpio_get(START_COUNT_BUTTON)){
            if (counted == 0) {
                npSetLED(4, 255, 0, 0);
                npWrite();
                sleep_ms(1000);
                npSetLED(3, 255, 0, 0);
                npWrite();
                sleep_ms(1000);
                npSetLED(2, 255, 0, 0);
                npWrite();
                sleep_ms(1000);
                npSetLED(1, 255, 0, 0);
                npWrite();
                sleep_ms(1000);
                npSetLED(0, 255, 0, 0);
                npWrite();
                counted = 1;
                sleep_ms(200);
            }
            else {
                npClear();
                npWrite();
                counted = 0;
                sleep_ms(3000);
            }
        }

        blit();
        adc_select_input(0);
        uint16_t vry_value = adc_read();

        adc_select_input(1);
        uint16_t vrx_value = adc_read();
        
        handle_display_rect(vrx_value, vry_value);

        sleep_ms(200);
    }
}
