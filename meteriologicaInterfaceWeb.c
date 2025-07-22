#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lib/sensores/aht20.h"
#include "lib/sensores/bmp280.h"
#include "lib/buzzer/buzzer.h"
#include "lib/led/led.h"
#include "lib/matriz/matriz.h"
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "lib/index_html.h"          // Página com gráficos
#include "lib/html_limits_config.h" //  Página de limites
#include "lwip/tcp.h"
#include <math.h>

#define I2C_PORT_0 i2c0               // i2c0 pinos 0 e 1
#define I2C_SDA_0 0                   // 0 
#define I2C_SCL_0 1                   // 1

#define I2C_PORT_1 i2c1               // i2c1 pinos 2 e 3
#define I2C_SDA_1 2                   // 2 
#define I2C_SCL_1 3                   // 3


// Wi-Fi credentials
#define WIFI_SSID "Leonardo"
#define WIFI_PASSWORD "00695470PI"

volatile float g_aht_temperature = 0.0f;
volatile float g_aht_humidity = 0.0f;
volatile float g_bmp_temperature = 0.0f; // Em °C
volatile float g_bmp_pressure = 0.0f;    // Em Pa

volatile float g_temp_min_limit = 18.0f; // Exemplo: 18°C
volatile float g_temp_max_limit = 30.0f; // Exemplo: 30°C

volatile float g_humidity_min_limit = 40.0f; // Exemplo: 40%
volatile float g_humidity_max_limit = 85.0f; // Exemplo: 85%

volatile float g_pressure_min_limit = 98000.0f; //  980 hPa (em Pa)
volatile float g_pressure_max_limit = 102000.0f; // 1020 hPa (em Pa)

volatile float g_temp_offset = 0.0f;

volatile float g_humidity_offset = 0.0f;

volatile float g_pressure_offset = 0.0f;

volatile bool g_alerts_enabled = true; //  Alertas habilitados por padrão

volatile int g_current_page = 0; // 0 para a página principal (gráficos), 1 para a página de limites


// Estrutura de dados
struct http_state
{
    char response[14000];
    size_t len;
    size_t sent;
};


#define BOTAO_A_PIN 5 
#define BOTAO_B_PIN 6
void gpio_irq_handler(uint gpio, uint32_t events){
    if (gpio == BOTAO_B_PIN){
         static uint64_t last_gpio_event_time = 0;
        uint64_t current_time = time_us_64();
        if (current_time - last_gpio_event_time > 200000) { // 200ms de debounce
            g_alerts_enabled = !g_alerts_enabled; // Alterna entre alertas ativos e desativos
            printf("DEBUG: Botao B pressionado. Desativando alertas %d\n", g_alerts_enabled);
            last_gpio_event_time = current_time;
        }
    }
    if (gpio == BOTAO_A_PIN) {
        static uint64_t last_gpio_event_time = 0;
        uint64_t current_time = time_us_64();
        if (current_time - last_gpio_event_time > 200000) { // 200ms de debounce
            g_current_page = 1 - g_current_page; // Alterna entre 0 e 1
            printf("DEBUG: Botao A pressionado. Alternando para pagina %d\n", g_current_page);
            last_gpio_event_time = current_time;
        }
    }
}



// Função de callback para enviar dados HTTP
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Função de recebimento HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) { 

    if (!p) { // Se não há pbuf, a conexão foi fechada pelo cliente
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) { // Falha na alocação de memória para o estado HTTP
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM; // Erro de memória
    }
    hs->sent = 0; // Zera o contador de bytes enviados

    // IMPRIME A REQUISIÇÃO RECEBIDA PARA DEBBUG
    printf("DEBUG: Requisição recebida: %s\n", req);

    // TRATAMENTO DE ENDPOINTS ESPECÍFICOS (JSON ou TEXT PLAIN)
    // 1. GET /system_state (busca de dados dos sensores e configurações)
    if (strstr(req, "GET /system_state")) {
        printf("DEBUG: Processando GET /system_state (JSON).\n");
        char json_payload[512]; // Aumente se o JSON ficar maior
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                "{"
                                "\"temperatura_aht\":%.2f,"
                                "\"umidade_aht\":%.2f,"
                                "\"temperatura_bmp\":%.2f,"
                                "\"pressao_bmp\":%.2f,"
                                "\"temp_offset\":%.2f,"
                                "\"humidity_offset\":%.2f,"
                                "\"pressure_offset\":%.2f,"
                                // Limites (seguem os offsets)
                                "\"temp_min\":%.2f,"
                                "\"temp_max\":%.2f,"
                                "\"humidity_min\":%.2f,"
                                "\"humidity_max\":%.2f,"
                                "\"pressure_min\":%.2f,"
                                "\"pressure_max\":%.2f,"
                                "\"alerts_enabled\":%d" 
                                "}\r\n",
                                g_aht_temperature, g_aht_humidity,
                                g_bmp_temperature, g_bmp_pressure,
                                g_temp_offset, g_humidity_offset, g_pressure_offset,
                                g_temp_min_limit, g_temp_max_limit,
                                g_humidity_min_limit, g_humidity_max_limit,
                                g_pressure_min_limit, g_pressure_max_limit,
                                (int)g_alerts_enabled
                            );

        hs->len = snprintf(hs->response, sizeof(hs->response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            json_len, json_payload);
    }
    // 2. POST /set_limits (salva os limites e estado dos alertas)
    else if (strstr(req, "POST /set_limits")) {
        printf("DEBUG: Processando POST /set_limits.\n");
        char *body = strstr(req, "\r\n\r\n");
        if (body) {
            body += 4; // Avança para o início do corpo JSON
            float t_min, t_max, h_min, h_max, p_min, p_max;
            int alerts_on_int;
            // Tenta ler todos os 7 valores do JSON
            if (sscanf(body, "{\"temp_min\":%f,\"temp_max\":%f,\"humidity_min\":%f,\"humidity_max\":%f,\"pressure_min\":%f,\"pressure_max\":%f,\"alerts_enabled\":%d",
                       &t_min, &t_max, &h_min, &h_max, &p_min, &p_max, &alerts_on_int) == 7) {
                if (t_min < t_max && h_min < h_max && p_min < p_max) {
                    g_temp_min_limit = t_min;
                    g_temp_max_limit = t_max;
                    g_humidity_min_limit = h_min;
                    g_humidity_max_limit = h_max;
                    g_pressure_min_limit = p_min;
                    g_pressure_max_limit = p_max;
                    g_alerts_enabled = (bool)alerts_on_int; // Atualiza o estado dos alertas

                    printf("DEBUG: Limites atualizados: Temp %.2f-%.2f, Hum %.2f-%.2f, Press %.2f-%.2f, Alertas: %d\n",
                           g_temp_min_limit, g_temp_max_limit, g_humidity_min_limit, g_humidity_max_limit,
                           g_pressure_min_limit, g_pressure_max_limit, g_alerts_enabled);

                    const char *success_msg = "Limites atualizados com sucesso.";
                    hs->len = snprintf(hs->response, sizeof(hs->response),
                                        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                        (int)strlen(success_msg), success_msg);
                } else {
                    const char *error_msg = "Valores de limite invalidos (min >= max).";
                    hs->len = snprintf(hs->response, sizeof(hs->response),
                                        "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                        (int)strlen(error_msg), error_msg);
                }
            } else {
                const char *error_msg = "Formato de dados invalido para set_limits.";
                hs->len = snprintf(hs->response, sizeof(hs->response),
                                    "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                    (int)strlen(error_msg), error_msg);
            }
        } else {
            const char *error_msg = "Corpo da requisicao vazio para set_limits.";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                                "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                (int)strlen(error_msg), error_msg);
        }
    }
    else if (strstr(req, "POST /set_offsets")) {
        printf("DEBUG: Processando POST /set_offsets.\n");
        char *body = strstr(req, "\r\n\r\n");
        if (body) {
            body += 4;
            float t_offset, h_offset, p_offset;
            if (sscanf(body, "{\"temp_offset\":%f,\"humidity_offset\":%f,\"pressure_offset\":%f",
                    &t_offset, &h_offset, &p_offset) == 3) {
                g_temp_offset = t_offset;
                g_humidity_offset = h_offset;
                g_pressure_offset = p_offset;


                const char *success_msg = "Offsets atualizados com sucesso.";
                hs->len = snprintf(hs->response, sizeof(hs->response),
                                    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                    (int)strlen(success_msg), success_msg);
            } else {
                const char *error_msg = "Formato de dados invalido para set_offsets.";
                hs->len = snprintf(hs->response, sizeof(hs->response),
                                    "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                    (int)strlen(error_msg), error_msg);
            }
        } else {
            const char *error_msg = "Corpo da requisicao vazio para set_offsets.";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                                "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                                (int)strlen(error_msg), error_msg);
        }
    }
    // TRATAMENTO DA PÁGINA PRINCIPAL OU DE LIMITES (HTML)
    // Se a requisição não corresponder a nenhum endpoint específico, serve a página HTML adequada
    else {
        // Verifica qual página deve ser servida com base na variável global
        if (g_current_page == 0) { // Página de gráficos
            printf("DEBUG: Servindo pagina de Graficos (g_current_page == 0).\n");
            hs->len = snprintf(hs->response, sizeof(hs->response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: %d\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "%s",
                                (int)strlen(index_html), index_html); 
        } else { // Página de limites
            printf("DEBUG: Servindo pagina de Limites (g_current_page == 1).\n");
            hs->len = snprintf(hs->response, sizeof(hs->response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: %d\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "%s",
                                (int)strlen(html_limits_config), html_limits_config); 
        }
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

// Função de callback para aceitar conexões TCP
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Função para iniciar o servidor HTTP
static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

int main(){
    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_B_PIN);
    gpio_set_irq_enabled_with_callback(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


    gpio_init(BOTAO_A_PIN); 
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN);

    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


    init_leds();


    init_buzzer(BUZZER_A_PIN,4.0f);

    configura_Inicializa_Pio();
   
    stdio_init_all();// Para depuração no terminal


    // Inicializa o I2C_0
    i2c_init(I2C_PORT_0, 400 * 1000);
    gpio_set_function(I2C_SDA_0, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_0, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_0);
    gpio_pull_up(I2C_SCL_0);

    // Inicializa o I2C_1
    i2c_init(I2C_PORT_1, 400 * 1000);
    gpio_set_function(I2C_SDA_1, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_1, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_1);
    gpio_pull_up(I2C_SCL_1);



    // Inicializa a biblioteca CYW43 para Wi-Fi
    if (cyw43_arch_init()){
        printf("Falha ao iniciar o chip Wifi!");
        set_led_red();
        while (true){
            tight_loop_contents();
        }
    }

    cyw43_arch_enable_sta_mode();// Coloca em modo cliente
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 60000)){ // Tenta se conectar durante 60s
        printf("Erro ao se conectar no wifi Wifi!!!!");
        set_led_red();
        while (true){
            tight_loop_contents();
        }
        
        
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);//Obtem o ip do dispositivo nesta rede
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n",ip_str);
    start_http_server();
    set_led_green();
    
    // Inicializa o BMP280
    bmp280_init(I2C_PORT_0);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT_0, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT_1);
    aht20_init(I2C_PORT_1);

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;


    while (1)
    {
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT_0, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        

        g_bmp_temperature = temperature / 100.0f; // 'temperature' do BMP280 em centésimos
        g_bmp_pressure = pressure; // 'pressure' do BMP280 em Pa
       

        g_bmp_temperature += g_temp_offset;
        g_bmp_pressure += g_pressure_offset;

        // PRINTS PARA DEPURAÇÃO NO TERMINAL//////////////////////////
        printf("-----------BMP280 LEITURAS-----------------\n");
        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT_1, &data)){
            printf("----------AHT LEITURAS------------------\n");
            printf("Temperatura : %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
        }
        else{
            play_tone(BUZZER_A_PIN,3000);
            sleep_ms(2000);
            stop_tone(BUZZER_A_PIN);
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        g_aht_temperature = data.temperature; 
        g_aht_humidity = data.humidity;

        g_aht_temperature += g_temp_offset;
        g_aht_humidity += g_humidity_offset;

        if (g_alerts_enabled) { // Verifica se os alertas estão habilitados
            bool alert_active = false;

            // Verificar Pressão BMP280 e umidade aht20
            if ((g_bmp_pressure < g_pressure_min_limit || g_bmp_pressure > g_pressure_max_limit)  && (g_aht_humidity < g_humidity_min_limit || g_aht_humidity > g_humidity_max_limit)) {
                printf("ALERTA: Pressao BMP280 fora dos limites! (%.2f Pa)\n", g_bmp_pressure);
                set_one_led(0,125,0,matriz_preenchida);
                alert_active = true;
            }
            // Verificar Temperatura AHT20 e umidade aht20
            else if ((g_aht_humidity < g_humidity_min_limit || g_aht_humidity > g_humidity_max_limit) && (g_aht_temperature < g_temp_min_limit || g_aht_temperature > g_temp_max_limit)) {
                printf("ALERTA: Umidade AHT20 fora dos limites! (%.2f %%)\n", g_aht_humidity);
                set_one_led(125,0,125,matriz_preenchida);
                alert_active = true;
            }
            // Verificar Temperatura AHT20 e pressao bmp280
            else if ((g_aht_temperature < g_temp_min_limit || g_aht_temperature > g_temp_max_limit) && (g_bmp_pressure < g_pressure_min_limit || g_bmp_pressure > g_pressure_max_limit)) {
                printf("ALERTA: Umidade AHT20 fora dos limites! (%.2f %%)\n", g_aht_humidity);
                set_one_led(125,125,125,matriz_preenchida);
                alert_active = true;
            }
            // Verificar Umidade AHT20
            else if (g_aht_humidity < g_humidity_min_limit || g_aht_humidity > g_humidity_max_limit) {
                printf("ALERTA: Umidade AHT20 fora dos limites! (%.2f %%)\n", g_aht_humidity);
                set_one_led(0,0,125,matriz_preenchida);
                alert_active = true;
            }
            // Verificar Pressão BMP280 (pode precisar de ajuste se g_bmp_pressure for kPa ou hPa)
            else if (g_bmp_pressure < g_pressure_min_limit || g_bmp_pressure > g_pressure_max_limit) {
                printf("ALERTA: Pressao BMP280 fora dos limites! (%.2f Pa)\n", g_bmp_pressure);
                set_one_led(125,125,0,matriz_preenchida);
                alert_active = true;
            }
            // Verificar Temperatura AHT20
            else if (g_aht_temperature < g_temp_min_limit || g_aht_temperature > g_temp_max_limit) {
                printf("ALERTA: Temperatura AHT20 fora dos limites! (%.2f C)\n", g_aht_temperature);
                set_one_led(125,0,0,matriz_preenchida);
                alert_active = true;
            }
             // Verificar Pressão BMP280 e umidade aht20
            if ((g_bmp_pressure < g_pressure_min_limit || g_bmp_pressure > g_pressure_max_limit)  && (g_aht_humidity < g_humidity_min_limit || g_aht_humidity > g_humidity_max_limit)) {
                printf("ALERTA: Pressao BMP280 fora dos limites! (%.2f Pa)\n", g_bmp_pressure);
                set_one_led(0,125,0,matriz_preenchida);
                alert_active = true;
            }


            if (alert_active) {
                play_tone(BUZZER_A_PIN, 2000); // Toca um tom de alerta
                sleep_ms(50); // Duração do tom
                stop_tone(BUZZER_A_PIN);
                set_led_red();
            } else {
                set_led_green(); // Se não houver alerta, LED verde
                set_one_led(0,0,0,matriz_preenchida);
            }
        } else {
            // Se os alertas estiverem desabilitados, garanta que o LED não esteja em estado de alerta
            set_led_green(); // Ex: LED verde quando não há alerta e sistema normal
            set_one_led(0,0,0,matriz_preenchida);
        }

           
        // Mantém o servidor HTTP ativo
        cyw43_arch_poll();
        sleep_ms(1000);
        //////////////////////////////////////////////////////////////

    }

    return 0;
}