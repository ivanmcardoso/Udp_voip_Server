//udp server
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_timer.h"
#include <driver/adc.h>
#include "driver/i2s.h"

#define SSID 						"Katia Cardoso"
#define PASSWORD					"jnc196809"
#define PORT 						8000
#define SAMPLE_RATE 				8000
#define BUFFER_MAX  				8000

size_t i2s_bytes_write = 0;

int16_t audioBuffer[BUFFER_MAX];

const int WIFI_CONNECTED_BIT = BIT0;

static EventGroupHandle_t s_wifi_event_group;
/*
static void recv_all(int sock, void *vbuf, size_t size_buf)
{
	char *buf = (char*)vbuf;
	int recv_size;
	size_t size_left;
	const int flags = 0;
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

	size_left = size_buf;

	while(size_left > 0 )
	{
		if((recv_size = recvfrom(sock, buf, size_left, flags, (struct sockaddr *)&source_addr, &socklen)) == -1)
		{
			printf("Erro ao receber\n");
			break;
		}
		if(recv_size == 0)
		{
			printf("Recebimento completo\n");
			break;
		}
		i2s_write(0, buf, recv_size,&i2s_bytes_write,  portMAX_DELAY);
		size_left -= recv_size;
		buf += recv_size;
	}

	return;
}
*/
static void udp_server_task(void *pvParameters)
{
	char  rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int sock = socket(addr_family,SOCK_DGRAM, ip_protocol);
    bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    while(1){

    	//recv_all(sock,(void*)&audioBuffer, sizeof(int16_t)*BUFFER_MAX);
        printf("port: %d esperando recebimento\n", PORT);

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
       int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

      // printf("len = %d\n",len);
    	rx_buffer[len] = 0;
    	printf("%s \n",rx_buffer);
    	shutdown(sock, 0);
    	close(sock);
    	vTaskDelay(10/portTICK_PERIOD_MS);
    }

}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        gpio_set_level(GPIO_NUM_2, 1);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xTaskCreatePinnedToCore(udp_server_task, "udp_server", 4096, NULL, 5, NULL,0);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
        	gpio_set_level(GPIO_NUM_2, 0);
            do {
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                vTaskDelay(300/portTICK_PERIOD_MS);
            }while(esp_wifi_connect() != ESP_OK);

            break;
        }
    default:
        break;
    }
    return ESP_OK;
}

//configuração do wifi com station
void setup_wifi()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

}
//configuração do protocolo i2s
static void i2s_setup(){

	i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 3,
        .dma_buf_len = 1024,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = 33,
        .ws_io_num = 25,
        .data_out_num = 26,
        .data_in_num = -1                                                       //não utilizado
    };
    i2s_driver_install(0, &i2s_config, 0, NULL);
    i2s_set_pin(0, &pin_config);

}
//configuração dos pinos de saída
static void pins_setup(){
	gpio_pad_select_gpio(GPIO_NUM_2);
	gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
}

void app_main(void)
{

	i2s_setup();
	pins_setup();
	nvs_flash_init();

	i2s_set_clk(0, SAMPLE_RATE, 16, 1);

	nvs_flash_init();
	setup_wifi();
}
