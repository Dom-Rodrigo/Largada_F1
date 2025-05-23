#include <stdio.h>
#include "pico/stdlib.h"
#include "ws2818b.pio.h"
#include "pico/bootrom.h"
#include "inc/ssd1306.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"



#define VRX 27 // input 1 s:24 h:1885 b:4090
#define VRY 26 // input 0 s:24 h:2087 b:4090
#define BUTTON_BOOTSEL 22
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define LED_RED 13
#define LED_GREEN 11

ssd1306_t ssd; // Inicializa a estrutura do display

// Definição do número de LEDs e pino.
#define LED_COUNT 5
#define LED_PIN 7
#define START_COUNT_BUTTON 5


#define BUZZER_PIN 10
// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 100

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
volatile uint32_t apagou_time;
bool exact_time = false;
bool apagou = false;
bool start = false;
volatile uint position = -1;
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_time > 200000){
        if (!gpio_get(BUTTON_BOOTSEL)) {
            last_time = current_time;
            rom_reset_usb_boot(0, 0);
        }
        if (!gpio_get(START_COUNT_BUTTON)){
            if (!apagou){
                printf("Queimou!\n");
                gpio_put(LED_RED, 1);
                gpio_put(LED_GREEN, 1);
            }
            if (apagou){
                printf("Tempo de reação: %d (ms) \n", (current_time-apagou_time)/1000);
                gpio_put(LED_RED, 0);
                gpio_put(LED_GREEN, 0);
                exact_time=false;
                start = true;
                position=5;
            }
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
}

void blit(){
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);

}

bool acendeu=false;
bool repeating_timer_callback(struct repeating_timer *t){
    //printf("POSITION: %d\n", position);
    position--;
    acendeu=true;
    return true;
    
}

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
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


    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    pwm_init_buzzer(BUZZER_PIN);

    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);


    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_set_irq_enabled_with_callback(BUTTON_BOOTSEL, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(START_COUNT_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Dispara o timer

    uint counted = 0;
    struct repeating_timer timer;
    add_repeating_timer_ms(1000, repeating_timer_callback, NULL, &timer); // Falta ligar o timer só quando apertar no botão

    while (true) {

        if (!start){ // Chegou no ultimo led, limpa tudo (como tá no timer só para depois de 1 seugndo que acendeu o ultimo)
            npClear();
            srand(to_us_since_boot(get_absolute_time()));
            npWrite();
            if (!exact_time){
                apagou_time = to_us_since_boot(get_absolute_time()); // Coleta o tempo exato parou
                exact_time=true;
            }
            apagou=true;
            acendeu=false;

        }
        else{
            if (acendeu){
                npSetLED(position, 255, 0, 0);
                npWrite();
                // Configurar o duty cycle para 25% (ativo)
                if (acendeu && position<5){
                    apagou=false;
                    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
                    pwm_set_gpio_level(BUZZER_PIN, 2048);
                    sleep_ms(200);
                    pwm_set_gpio_level(BUZZER_PIN, 0); 
                }


                // Obter o slice do PWM associado ao pino

            
                //printf("T. restante: %d\n", remaining_alarm_time_ms(timer.alarm_id));
                acendeu=false;
                if (position == 0)
                    start=false;

                if (position == 0){
                    int num = rand() % 2000 + 1; // Numero aleatorio faz com que a ultima luz se apague entre 1 a 3 segundos
                    sleep_ms(num);
                }
                    
                // Desativar o sinal PWM (duty cycle 0)

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
