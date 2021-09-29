/*
Copyright (c) 2017-2019 Tony Pottier

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

@file main.c
@author Tony Pottier
@brief Ponto de entrada para o aplicativo ESP32.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "wifi_manager.h"

/* tag @brief usada para mensagens do console serial ESP */
static const char TAG[] = "main";

/**
 * @brief RTOS task que imprime periodicamente a memória heap disponível. 
 * As informações de depuração do @note Pure, nunca devem ser iniciadas no código de produção! Este é um exemplo de como você pode integrar seu código com o wifi-manager 
 */
void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}


/**
 * @brief este é um exemplo de retorno de chamada que você pode configurar em seu próprio aplicativo para ser notificado sobre o evento do gerenciador de wi-fi.
 */
void cb_connection_ok(void *pvParameter){
	ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;

	/* transformar IP em string legível por humanos */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

	ESP_LOGI(TAG, "Eu tenho uma conexão e meu IP e %s!", str_ip);
}

void app_main()
{
	/* inicie o gerenciador de wi-fi */
	wifi_manager_start();

	/* registre um retorno de chamada como um exemplo de como você pode integrar seu código com o gerenciador wi-fi */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

	/* seu código deve ir aqui. Aqui, simplesmente criamos uma tarefa no núcleo 2 que monitora a memória heap livre */
	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
}
