

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h" // Inclui o driver para o chip CYW43, que é o chip Wi-Fi da Pico W. 
// Essa lib permite inicializar e controlar a conexão Wi-Fi
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h" // Essa é uma parte da pilha TCP/IP lwIP (lightweight IP). A pbuf (packet buffer) é usada para armazenar 
// e manipular dados de pacotes de rede
#include "lwip/tcp.h" // Outro componente do lwIP, que cuida da comunicação TCP — conexão, envio e recebimento de dados, etc.
#include "lwip/netif.h" // Essa biblioteca permite o acesso à interface de rede (netif = network interface), 
// incluindo variáveis globais como netif_default, que representa a interface padrão da rede, e funções para configurar IPs

// Configurações de Wi-Fi
#define WIFI_SSID "Labirang"
#define WIFI_PASSWORD "1fp1*007"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN // vem da biblioteca pico/cyw43_arch.h
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Controle dos LEDs
    if (strstr(request, "GET /on") != NULL)
    {
        cyw43_arch_gpio_put(LED_PIN, 1);
    }
    else if (strstr(request, "GET /off") != NULL)
    {
        cyw43_arch_gpio_put(LED_PIN, 0);
    }

    bool estado_botao_a = gpio_get(BUTTON_A_PIN) == 0; // botão A
    bool estado_botao_b = gpio_get(BUTTON_B_PIN) == 0; // botão B

    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

    // Cria a resposta HTML
    char html[1024];

    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html lang='pt-br'>\n"
            "<head>\n"
                "<script> setTimeout(() => location.reload(), 1000); </script>\n" // recarrega a página a cada 1 segundo
                "<meta charset='UTF-8'>\n"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
                "<title> Atividade 1 </title>\n"
            "</head>\n"
            "<body style='text-align: center'>\n"
                "<h1> Leitura dos status dos botões da placa Bitdoglab </h1>\n"
                "<p class=\"estado_botao_a\"> Estado botão A: %s </p>\n"
                "<p class=\"estado_botao_b\"> Estado botão B: %s </p>\n"
                "<h1> Leitura da temperatura da placa Bitdoglab </h1>\n"
                "<p class=\"temperature\">Temperatura interna: %.2f &deg;C</p>\n"
            "</body>\n"
        "</html>\n",
        estado_botao_a? "Pressionado": "Solto", 
        estado_botao_b? "Pressionado": "Solto",
        temperature        
    );

    // Envia a página web pela coneção TCP
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    // Solicita que os dados pendentes no buffer sejam realmente enviados pela pilha TCP
    tcp_output(tpcb);

    // Libera a memória usada para armazenar o pedido recebido do cliente
    free(request);
    pbuf_free(p);

    return ERR_OK; // Retorna ERR_OK indicando que o processamento foi feito sem problemas 
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função principal
int main()
{
    // Inicialização dos GPIOs e dos botões
    stdio_init_all();

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);

    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);

    // cyw43_arch_deinit(); // Desativa o Wi-Fi

   // Tenta inicializar a arquitetura do módulo wifi
   while (cyw43_arch_init())
   {
       printf("Falha ao inicializar Wi-Fi\n");
       sleep_ms(100);
       return -1; // Se a inicialização falhar, imprime uma mensagem e encerra o programa
   }

   // cyw43_arch_gpio_put(LED_PIN, 0);
   cyw43_arch_enable_sta_mode(); // permite conectar a um roteador wifi

   printf("Conectando ao Wi-Fi...\n");

   // Tenta se conectar à rede wifi passando o nome de uma rede e a senha
   // Espera até 20 segundos para conectar
   while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
   {
       printf("Falha ao conectar ao Wi-Fi\n");
       sleep_ms(100);
       return -1; // Encerra o programa com erro
   }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default)
    {
        // Exibe o endereço IP atribuído ao dispositivo
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server); // Coloca o servidor TCP pronto para aceitar conexões
    tcp_accept(server, tcp_server_accept); // Define a função de callback que será chamada quando uma nova conexão for aceita

    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true) // Repetição principal do programa
    {
        cyw43_arch_poll(); // garante que o wifi funcione corretamente
    }

    cyw43_arch_deinit(); // Desliga o wifi
    return 0;
}
