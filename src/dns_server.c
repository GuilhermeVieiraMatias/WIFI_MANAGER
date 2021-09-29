/*
Copyright (c) 2019 Tony Pottier

A permissão é concedida, gratuitamente, a qualquer pessoa que obtenha uma cópia
deste software e arquivos de documentação associados (o "Software"), para lidar
no Software sem restrição, incluindo, sem limitação, os direitos
para usar, copiar, modificar, mesclar, publicar, distribuir, sublicenciar e / ou vender
cópias do Software, e para permitir que as pessoas a quem o Software é
fornecido para fazê-lo, sujeito às seguintes condições:

O aviso de direitos autorais acima e este aviso de permissão devem ser incluídos em todos
cópias ou partes substanciais do Software.

O SOFTWARE É FORNECIDO "COMO ESTÁ", SEM GARANTIA DE QUALQUER TIPO, EXPRESSA OU
IMPLÍCITA, INCLUINDO, MAS NÃO SE LIMITANDO ÀS GARANTIAS DE COMERCIALIZAÇÃO,
ADEQUAÇÃO A UMA FINALIDADE ESPECÍFICA E NÃO VIOLAÇÃO. EM NENHUMA HIPÓTESE O
AUTORES OU TITULARES DE DIREITOS AUTORAIS SÃO RESPONSÁVEIS POR QUALQUER RECLAMAÇÃO, DANOS OU OUTROS
RESPONSABILIDADE, SEJA EM AÇÃO DE CONTRATO, DELITO OU DE OUTRA FORMA, DECORRENTE DE,
FORA DE OU EM CONEXÃO COM O SOFTWARE OU O USO OU OUTRAS NEGOCIAÇÕES NO
PROGRAMAS.

@file dns_server.c
@author Tony Pottier
@brief Define um servidor DNS extremamente básico para a funcionalidade do portal cativo.
É basicamente um sequestro de DNS que responde ao endereço do esp, não importa qual
pedido é enviado para ele.

Contém a tarefa freeRTOS para o servidor DNS que processa as solicitações.

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <lwip/sockets.h>
#include <string.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <byteswap.h>

#include "wifi_manager.h"
#include "dns_server.h"

static const char TAG[] = "dns_server";
static TaskHandle_t task_dns_server = NULL;
int socket_fd;

void dns_server_start() {
	if(task_dns_server == NULL){
		xTaskCreate(&dns_server, "dns_server", 3072, NULL, WIFI_MANAGER_TASK_PRIORITY-1, &task_dns_server);
	}
}

void dns_server_stop(){
	if(task_dns_server){
		vTaskDelete(task_dns_server);
		close(socket_fd);
		task_dns_server = NULL;
	}

}



void dns_server(void *pvParameters) {



    struct sockaddr_in ra;

    /* Defina o redirecionamento de sequestro de DNS para o IP do ponto de acesso */
    ip4_addr_t ip_resolved;
    inet_pton(AF_INET, DEFAULT_AP_IP, &ip_resolved);


    /* Criar soquete UDP */
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0){
        ESP_LOGE(TAG, "Failed to create socket");
        exit(0);
    }

    /* Vincule à porta 53 (porta típica do servidor DNS) */
    esp_netif_ip_info_t ip;
    esp_netif_t* netif_sta = wifi_manager_get_esp_netif_sta();
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif_sta, &ip));
    ra.sin_family = AF_INET;
    ra.sin_addr.s_addr = ip.ip.addr;
    ra.sin_port = htons(53);
    if (bind(socket_fd, (struct sockaddr *)&ra, sizeof(struct sockaddr_in)) == -1) {
        ESP_LOGE(TAG, "Failed to bind to 53/udp");
        close(socket_fd);
        exit(1);
    }

    struct sockaddr_in client;
    socklen_t client_len;
    client_len = sizeof(client);
    int length;
    uint8_t data[DNS_QUERY_MAX_SIZE];	/* buffer de consulta DNS */
    uint8_t response[DNS_ANSWER_MAX_SIZE]; /* buffer de resposta dns */
    char ip_address[INET_ADDRSTRLEN]; /* buffer para armazenar IPs como texto. Isso é usado apenas para depuração e não serve a nenhum outro propósito */
    char *domain; /* Isso é usado apenas para depuração e não serve a nenhum outro propósito */
    int err;

    ESP_LOGI(TAG, "DNS Server listening on 53/udp");

    /* Inicie o loop para processar solicitações de DNS */
    for(;;) {

    	memset(data, 0x00,  sizeof(data)); /* reset buffer */
        length = recvfrom(socket_fd, data, sizeof(data), 0, (struct sockaddr *)&client, &client_len); /* ler pedido udp */

        /*se a consulta for maior que o tamanho do buffer, simplesmente a ignoramos. Este caso só deve acontecer em caso de múltiplos
         * consultas dentro do mesmo pacote DNS e não é compatível com este simples sequestro de DNS. */
        if ( length > 0   &&  ((length + sizeof(dns_answer_t)-1) < DNS_ANSWER_MAX_SIZE)   ) {

        	data[length] = '\0'; /*no caso de haver um nome de domínio falso que não tenha terminação nula */

            /* Gerar mensagem de cabeçalho */
            memcpy(response, data, sizeof(dns_header_t));
            dns_header_t *dns_header = (dns_header_t*)response;
            dns_header->QR = 1; /*bit de resposta */
            dns_header->OPCode  = DNS_OPCODE_QUERY; /* sem suporte para outro tipo de resposta */
            dns_header->AA = 1; /*resposta autoritária */
            dns_header->RCode = DNS_REPLY_CODE_NO_ERROR; /* nenhum erro */
            dns_header->TC = 0; /*sem truncamento */
            dns_header->RD = 0; /*sem recursão */
            dns_header->ANCount = dns_header->QDCount; /* definir contagem de respostas = contagem de perguntas - duhh! */
            dns_header->NSCount = 0x0000; /* registros de recursos do servidor de nomes = 0 */
            dns_header->ARCount = 0x0000; /* registros de recursos = 0 */


            /* copie o resto da consulta na resposta */
            memcpy(response + sizeof(dns_header_t), data + sizeof(dns_header_t), length - sizeof(dns_header_t));


            /* extrair o nome de domínio e solicitar IP para depuração */
            inet_ntop(AF_INET, &(client.sin_addr), ip_address, INET_ADDRSTRLEN);
            domain = (char*) &data[sizeof(dns_header_t) + 1];
            for(char* c=domain; *c != '\0'; c++){
            	if(*c < ' ' || *c > 'z') *c = '.'; /* tecnicamente, devemos testar se os primeiros dois bits são 00 (por exemplo, if ((* c & 0xC0) == 0x00) * c = '.'), mas isso torna o código muito mais legível */
            }
            ESP_LOGI(TAG, "Replying to DNS request for %s from %s", domain, ip_address);


            /* crie uma resposta DNS no final da consulta*/
            dns_answer_t *dns_answer = (dns_answer_t*)&response[length];
            dns_answer->NAME = __bswap_16(0xC00C); /* Este é um indicador para o início da pergunta. De acordo com o padrão DNS, os primeiros dois bits devem ser definidos como 11 por algum motivo estranho, portanto, 0xC0 */
            dns_answer->TYPE = __bswap_16(DNS_ANSWER_TYPE_A);
            dns_answer->CLASS = __bswap_16(DNS_ANSWER_CLASS_IN);
            dns_answer->TTL = (uint32_t)0x00000000; /* sem cache. Evita envenenamento de DNS, pois se trata de um sequestro de DNS */
            dns_answer->RDLENGTH = __bswap_16(0x0004); /* 4 byte => tamanho de um endereço ipv4 */
            dns_answer->RDATA = ip_resolved.addr;

            err = sendto(socket_fd, response, length+sizeof(dns_answer_t), 0, (struct sockaddr *)&client, client_len);
            if (err < 0) {
            	ESP_LOGE(TAG, "UDP sendto failed: %d", err);
            }
        }

        taskYIELD(); /* permite que o agendador freeRTOS assuma o controle, se necessário. O daemon DNS não deve sobrecarregar o sistema */

    }
    close(socket_fd);

    vTaskDelete ( NULL );
}




