/*
Copyright (c) 2017-2020 Tony Pottier

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

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contém a tarefa freeRTOS e todo o suporte necessário

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <http_app.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"


#include "json.h"
#include "dns_server.h"
#include "nvs_sync.h"
#include "wifi_manager.h"



/* objetos usados ​​para manipular a fila principal de eventos */
QueueHandle_t wifi_manager_queue;

/* @brief temporizador de software para esperar entre cada tentativa de conexão.
 * Não faz sentido monopolizar um cronômetro de hardware para uma funcionalidade como esta, que só precisa ser 'accurate enough' */
TimerHandle_t wifi_manager_retry_timer = NULL;

/* @brief temporizador de software que irá disparar o desligamento do AP após uma conexão STA bem-sucedida
 * Não faz sentido monopolizar um cronômetro de hardware para uma funcionalidade como esta, que só precisa ser "precisa o suficiente" */
TimerHandle_t wifi_manager_shutdown_ap_timer = NULL;

SemaphoreHandle_t wifi_manager_json_mutex = NULL;
SemaphoreHandle_t wifi_manager_sta_ip_mutex = NULL;
char *wifi_manager_sta_ip = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records;
char *accessp_json = NULL;
char *ip_info_json = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;

/* @brief Matriz de ponteiros de função de retorno de chamada */
void (**cb_ptr_arr)(void*) = NULL;

/* @brief tag usada para mensagens do console serial ESP */
static const char TAG[] = "wifi_manager";

/* @brief identificador de tarefa para a tarefa wifi_manager principal */
static TaskHandle_t task_wifi_manager = NULL;

/* @brief objeto netif para a ESTAÇÃO */
static esp_netif_t* esp_netif_sta = NULL;

/* @brief objeto netif para o ACCESS POINT */
static esp_netif_t* esp_netif_ap = NULL;

/**
 * As configurações reais de WiFi em uso
 */
struct wifi_settings_t wifi_settings = {
	.ap_ssid = DEFAULT_AP_SSID,
	.ap_pwd = DEFAULT_AP_PASSWORD,
	.ap_channel = DEFAULT_AP_CHANNEL,
	.ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
	.ap_bandwidth = DEFAULT_AP_BANDWIDTH,
	.sta_only = DEFAULT_STA_ONLY,
	.sta_power_save = DEFAULT_STA_POWER_SAVE,
	.sta_static_ip = 0,
};

const char wifi_manager_nvs_namespace[] = "espwifimgr";

static EventGroupHandle_t wifi_manager_event_group;

/* @brief indicar que o ESP32 está conectado no momento. */
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

/* @brief Definido automaticamente assim que o SoftAP é iniciado */
const int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

/* @brief Quando definido, significa que um cliente solicitou a conexão a um ponto de acesso.*/
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

/* @brief Este bit é definido automaticamente assim que uma conexão for perdida */
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

/* @brief Quando definido, significa que o gerenciador wi-fi tenta restaurar uma conexão salva anteriormente na inicialização. */
const int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

/* @brief Quando definido, significa que um cliente solicitou a desconexão do AP conectado no momento. */
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

/* @brief Quando definido, significa que uma varredura está em andamento */
const int WIFI_MANAGER_SCAN_BIT = BIT7;

/* @brief Quando definido, significa que o usuário solicitou uma desconexão */
const int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;



void wifi_manager_timer_retry_cb( TimerHandle_t xTimer ){

	ESP_LOGI(TAG, "Retry Timer Tick! Sending ORDER_CONNECT_STA with reason CONNECTION_REQUEST_AUTO_RECONNECT");

	/* pare o timer */
	xTimerStop( xTimer, (TickType_t) 0 );

	/* Tentar reconectar */
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);

}

void wifi_manager_timer_shutdown_ap_cb( TimerHandle_t xTimer){

	/* pare o timer */
	xTimerStop( xTimer, (TickType_t) 0 );

	/* Tentativa de desligar o AP */
	wifi_manager_send_message(WM_ORDER_STOP_AP, NULL);
}

void wifi_manager_scan_async(){
	wifi_manager_send_message(WM_ORDER_START_WIFI_SCAN, NULL);
}

void wifi_manager_disconnect_async(){
	wifi_manager_send_message(WM_ORDER_DISCONNECT_STA, NULL);
}


void wifi_manager_start(){

	/* desative o registro de wi-fi padrão */
	esp_log_level_set("wifi", ESP_LOG_NONE);

	/* inicializar memória flash */
	nvs_flash_init();
	ESP_ERROR_CHECK(nvs_sync_create()); /* semáforo para sincronização de thread na memória NVS */

	/* alocação de memória */
	wifi_manager_queue = xQueueCreate( 3, sizeof( queue_message) );
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(MAX_AP_NUM * JSON_ONE_APP_SIZE + 4); /* 4 bytes para encapsulamento json de "[\n" and "]\0" */
	wifi_manager_clear_access_points_json();
	ip_info_json = (char*)malloc(sizeof(char) * JSON_IP_INFO_SIZE);
	wifi_manager_clear_ip_info_json();
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
	memset(&wifi_settings.sta_static_ip_config, 0x00, sizeof(esp_netif_ip_info_t));
	cb_ptr_arr = malloc(sizeof(void (*)(void*)) * WM_MESSAGE_CODE_COUNT);
	for(int i=0; i<WM_MESSAGE_CODE_COUNT; i++){
		cb_ptr_arr[i] = NULL;
	}
	wifi_manager_sta_ip_mutex = xSemaphoreCreateMutex();
	wifi_manager_sta_ip = (char*)malloc(sizeof(char) * IP4ADDR_STRLEN_MAX);
	wifi_manager_safe_update_sta_ip_string((uint32_t)0);
	wifi_manager_event_group = xEventGroupCreate();

	/* crie um cronômetro para manter o controle de novas tentativas */
	wifi_manager_retry_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_retry_cb);

	/* crie um cronômetro para acompanhar o desligamento do AP */
	wifi_manager_shutdown_ap_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_shutdown_ap_cb);

	/* iniciar tarefa de gerenciamento de wi-fi */
	xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &task_wifi_manager);
}

esp_err_t wifi_manager_save_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	size_t sz;

	/* variáveis ​​usadas para verificar se a gravação é realmente necessária */
	wifi_config_t tmp_conf;
	struct wifi_settings_t tmp_settings;
	memset(&tmp_conf, 0x00, sizeof(tmp_conf));
	memset(&tmp_settings, 0x00, sizeof(tmp_settings));
	bool change = false;

	ESP_LOGI(TAG, "About to save config to flash!!");

	if(wifi_manager_config_sta && nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK){
			nvs_sync_unlock();
			return esp_err;
		}

		sz = sizeof(tmp_conf.sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", tmp_conf.sta.ssid, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.ssid, (char*)wifi_manager_config_sta->sta.ssid) != 0){
			/* SSID ou SSID diferente não existe no flash: salvar novo SSID */
			esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s",wifi_manager_config_sta->sta.ssid);

		}

		sz = sizeof(tmp_conf.sta.password);
		esp_err = nvs_get_blob(handle, "password", tmp_conf.sta.password, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.password, (char*)wifi_manager_config_sta->sta.password) != 0){
			/* senha diferente ou senha não existe no flash: salvar nova senha */
			esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s",wifi_manager_config_sta->sta.password);
		}

		sz = sizeof(tmp_settings);
		esp_err = nvs_get_blob(handle, "settings", &tmp_settings, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) &&
				(
				strcmp( (char*)tmp_settings.ap_ssid, (char*)wifi_settings.ap_ssid) != 0 ||
				strcmp( (char*)tmp_settings.ap_pwd, (char*)wifi_settings.ap_pwd) != 0 ||
				tmp_settings.ap_ssid_hidden != wifi_settings.ap_ssid_hidden ||
				tmp_settings.ap_bandwidth != wifi_settings.ap_bandwidth ||
				tmp_settings.sta_only != wifi_settings.sta_only ||
				tmp_settings.sta_power_save != wifi_settings.sta_power_save ||
				tmp_settings.ap_channel != wifi_settings.ap_channel
				)
		){
			esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;

			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s",wifi_settings.ap_ssid);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s",wifi_settings.ap_pwd);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i",wifi_settings.ap_channel);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i",wifi_settings.ap_ssid_hidden);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i",wifi_settings.ap_bandwidth);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i",wifi_settings.sta_only);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i",wifi_settings.sta_power_save);
		}

		if(change){
			esp_err = nvs_commit(handle);
		}
		else{
			ESP_LOGI(TAG, "Wifi config was not saved to flash because no change has been detected.");
		}

		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);
		nvs_sync_unlock();

	}
	else{
		ESP_LOGE(TAG, "wifi_manager_save_sta_config failed to acquire nvs_sync mutex");
	}

	return ESP_OK;
}

bool wifi_manager_fetch_wifi_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);

		if(esp_err != ESP_OK){
			nvs_sync_unlock();
			return false;
		}

		if(wifi_manager_config_sta == NULL){
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* alocar buffer */
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		/* ssid */
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		/* settings */
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

		free(buff);
		nvs_close(handle);
		nvs_sync_unlock();


		ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s",wifi_settings.ap_ssid);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s",wifi_settings.ap_pwd);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i",wifi_settings.ap_channel);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i",wifi_settings.ap_ssid_hidden);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",wifi_settings.ap_bandwidth);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",wifi_settings.sta_only);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i",wifi_settings.sta_power_save);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",wifi_settings.sta_static_ip);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';


	}
	else{
		return false;
	}

}


void wifi_manager_clear_ip_info_json(){
	strcpy(ip_info_json, "{}\n");
}


void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code){

	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	if(config){

		const char *ip_info_json_format = ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n";

		memset(ip_info_json, 0x00, JSON_IP_INFO_SIZE);

		/* para evitar a declaração de um novo buffer, copiamos os dados diretamente para o buffer em seu endereço correto */
		strcpy(ip_info_json, "{\"ssid\":");
		json_print_string(config->sta.ssid,  (unsigned char*)(ip_info_json+strlen(ip_info_json)) );

		size_t ip_info_json_len = strlen(ip_info_json);
		size_t remaining = JSON_IP_INFO_SIZE - ip_info_json_len;
		if(update_reason_code == UPDATE_CONNECTION_OK){
			/* o resto das informações é copiado após o SSID */
			esp_netif_ip_info_t ip_info;
			ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));

			char ip[IP4ADDR_STRLEN_MAX]; /* note: IP4ADDR_STRLEN_MAX é definido em lwip */
			char gw[IP4ADDR_STRLEN_MAX];
			char netmask[IP4ADDR_STRLEN_MAX];

			esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);


			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
					ip,
					netmask,
					gw,
					(int)update_reason_code);
		}
		else{
			/* notificar na saída json o código de razão por que isso foi atualizado sem uma conexão */
			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
								"0",
								"0",
								"0",
								(int)update_reason_code);
		}
	}
	else{
		wifi_manager_clear_ip_info_json();
	}


}


void wifi_manager_clear_access_points_json(){
	strcpy(accessp_json, "[]\n");
}
void wifi_manager_generate_acess_points_json(){

	strcpy(accessp_json, "[");


	const char oneap_str[] = ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n";

	/* buffer de pilha para manter um AP até que seja copiado para accessp_json */
	char one_ap[JSON_ONE_APP_SIZE];
	for(int i=0; i<ap_num;i++){

		wifi_ap_record_t ap = accessp_records[i];

		/* SSID precisa ter escape de json. Para economizar na memória heap, ela é impressa diretamente no endereço correto */
		strcat(accessp_json, "{\"ssid\":");
		json_print_string( (unsigned char*)ap.ssid,  (unsigned char*)(accessp_json+strlen(accessp_json)) );

		/* imprima o resto do json para este ponto de acesso: não há mais string para escapar */
		snprintf(one_ap, (size_t)JSON_ONE_APP_SIZE, oneap_str,
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?']':',');

		/* adicione-o à lista */
		strcat(accessp_json, one_ap);
	}

}



bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait){
	if(wifi_manager_sta_ip_mutex){
		if( xSemaphoreTake( wifi_manager_sta_ip_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_sta_ip_string(){
	xSemaphoreGive( wifi_manager_sta_ip_mutex );
}

void wifi_manager_safe_update_sta_ip_string(uint32_t ip){

	if(wifi_manager_lock_sta_ip_string(portMAX_DELAY)){

		esp_ip4_addr_t ip4;
		ip4.addr = ip;

		char str_ip[IP4ADDR_STRLEN_MAX];
		esp_ip4addr_ntoa(&ip4, str_ip, IP4ADDR_STRLEN_MAX);

		strcpy(wifi_manager_sta_ip, str_ip);

		ESP_LOGI(TAG, "Set STA IP String to: %s", wifi_manager_sta_ip);

		wifi_manager_unlock_sta_ip_string();
	}
}

char* wifi_manager_get_sta_ip_string(){
	return wifi_manager_sta_ip;
}


bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait){
	if(wifi_manager_json_mutex){
		if( xSemaphoreTake( wifi_manager_json_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_json_buffer(){
	xSemaphoreGive( wifi_manager_json_mutex );
}

char* wifi_manager_get_ap_list_json(){
	return accessp_json;
}


/**
 * @brief Manipulador de eventos de wi-fi padrão
 */
static void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){


	if (event_base == WIFI_EVENT){

		switch(event_id){

		/* O driver Wi-Fi nunca irá gerar este evento, que, como resultado, pode ser ignorado pelo evento do aplicativo
		 * ligar de volta. Este evento pode ser removido em versões futuras. */
		case WIFI_EVENT_WIFI_READY:
			ESP_LOGI(TAG, "WIFI_EVENT_WIFI_READY");
			break;

		/* O evento scan-done é disparado por esp_wifi_scan_start () e surgirá nos seguintes cenários:
			  A varredura é concluída, por exemplo, o AP alvo foi encontrado com sucesso ou todos os canais foram varridos.
			  A varredura é interrompida por esp_wifi_scan_stop ().
			  O esp_wifi_scan_start () é chamado antes que a varredura seja concluída. Uma nova varredura irá substituir o atual
				scan e um evento scan-done será gerado.
			O evento de verificação concluída não surgirá nos seguintes cenários:
			  É uma verificação bloqueada.
			  A varredura é causada por esp_wifi_connect ().
			Ao receber este evento, a tarefa de evento não faz nada. O retorno de chamada do evento do aplicativo precisa chamar
			esp_wifi_scan_get_ap_num() e esp_wifi_scan_get_ap_records() para buscar a lista de APs escaneados e acionar
			o driver Wi-Fi para liberar a memória interna alocada durante a digitalização (não se esqueça de fazer isso)!
		 */
		case WIFI_EVENT_SCAN_DONE:
			ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
	    	xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
			wifi_event_sta_scan_done_t* event_sta_scan_done = (wifi_event_sta_scan_done_t*)malloc(sizeof(wifi_event_sta_scan_done_t));
			*event_sta_scan_done = *((wifi_event_sta_scan_done_t*)event_data);
	    	wifi_manager_send_message(WM_EVENT_SCAN_DONE, event_sta_scan_done);
			break;

		/* Se esp_wifi_start () retornar ESP_OK e o modo Wi-Fi atual for Estação ou AP + Estação, então este evento irá
		 * surgir. Ao receber este evento, a tarefa de evento inicializará a interface de rede LwIP (netif).
		 * Geralmente, o retorno de chamada do evento do aplicativo precisa chamar esp_wifi_connect () para se conectar ao AP configurado. */
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "");
			break;

		/* Se esp_wifi_stop () retornar ESP_OK e o modo Wi-Fi atual for Estação ou AP + Estação, então este evento irá surgir.
		 * Ao receber este evento, a tarefa do evento irá liberar o endereço IP da estação, parar o cliente DHCP, remover
		 * Conexões relacionadas a TCP / UDP e limpar a estação netif LwIP, etc. O retorno de chamada de evento de aplicativo geralmente faz
		 * não precisa fazer nada. */
		case WIFI_EVENT_STA_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
			break;

		/* Se esp_wifi_connect () retorna ESP_OK e a estação se conecta com sucesso ao AP alvo, o evento de conexão
		 * vai surgir. Ao receber este evento, a tarefa do evento inicia o cliente DHCP e inicia o processo DHCP de obtenção
		 * o endereço IP. Em seguida, o driver Wi-Fi está pronto para enviar e receber dados. Este momento é bom para começar
		 * a aplicação funciona, desde que a aplicação não dependa do LwIP, nomeadamente do endereço IP. No entanto, se
		 * o aplicativo é baseado em LwIP, então você precisa esperar até que o evento got ip chegue. */
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
			break;

		/* Este evento pode ser gerado nas seguintes situações:
		 *
		 *     Quando esp_wifi_disconnect (), ou esp_wifi_stop (), ou esp_wifi_deinit (), ou esp_wifi_restart () é chamado e
		 *     a estação já está conectada ao AP.
		 *
		 *     Quando esp_wifi_connect () é chamado, mas o driver Wi-Fi não consegue estabelecer uma conexão com o AP devido a certos
		 *     razões, por exemplo a varredura não consegue encontrar o AP de destino, o tempo de autenticação se esgota, etc. Se houver mais de um AP
		 *     com o mesmo SSID, o evento desconectado é gerado depois que a estação falha ao conectar todos os APs encontrados.
		 *
		 *     Quando a conexão Wi-Fi é interrompida por motivos específicos, por exemplo, a estação perde N beacons continuamente,
		 *     o AP inicia a estação, o modo de autenticação do AP é alterado, etc.
		 *
		 * Ao receber este evento, o comportamento padrão da tarefa de evento é: - Desliga o netif LwIP da estação.
		 * - Notifica a tarefa LwIP para limpar as conexões UDP / TCP que causam o status errado para todos os soquetes. Para baseado em soquete
		 * aplicativos, o retorno de chamada do aplicativo pode escolher fechar todos os sockets e recriá-los, se necessário, ao receber
		 * este evento.
		 *
		 * O código de identificador de evento mais comum para este evento no aplicativo é chamar esp_wifi_connect () para reconectar o Wi-Fi.
		 * No entanto, se o evento é gerado porque esp_wifi_disconnect () é chamado, o aplicativo não deve chamar esp_wifi_connect ()
		 * para reconectar. É responsabilidade do aplicativo distinguir se o evento é causado por esp_wifi_disconnect () ou
		 * outras razões. Às vezes, uma estratégia de reconexão melhor é necessária, consulte <Reconexão Wi-Fi> e
		 * <Verificar quando o Wi-Fi está conectando>.
		 *
		 * Outra coisa que merece nossa atenção é que o comportamento padrão do LwIP é abortar todas as conexões de soquete TCP em
		 * recebendo a desconexão. Na maioria das vezes, não é um problema. No entanto, para alguma aplicação especial, isso pode não ser
		 * o que eles querem, considere os seguintes cenários:
		 *
		 *    O aplicativo cria uma conexão TCP para manter os dados keep-alive de nível de aplicativo que são enviados
		 *    a cada 60 segundos.
		 *
		 *    Por alguns motivos, a conexão Wi-Fi é cortada e <WIFI_EVENT_STA_DISCONNECTED> é ativado.
		 *    De acordo com a implementação atual, todas as conexões TCP serão removidas e o soquete keep-alive será
		 *    em um status errado. No entanto, uma vez que o designer do aplicativo acredita que a camada de rede NÃO deve se preocupar com
		 *    este erro na camada Wi-Fi, o aplicativo não fecha o soquete.
		 *
		 *    Cinco segundos depois, a conexão Wi-Fi é restaurada porque esp_wifi_connect () é chamado no aplicativo
		 *    função de retorno de chamada de evento. Além disso, a estação se conecta ao mesmo AP e obtém o mesmo endereço IPV4 de antes.
		 *
		 *    Sessenta segundos depois, quando o aplicativo envia dados com o soquete keep-alive, o soquete retorna um erro
		 *    e o aplicativo fecha o soquete e o recria quando necessário.
		 *
		 * No cenário acima, o ideal é que os soquetes do aplicativo e a camada de rede não sejam afetados, uma vez que o Wi-Fi
		 * a conexão falha apenas temporariamente e se recupera muito rapidamente. O aplicativo pode habilitar “Manter conexões TCP quando
		 * IP alterado ”via menuconfig LwIP.*/
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");

			wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)malloc(sizeof(wifi_event_sta_disconnected_t));
			*wifi_event_sta_disconnected =  *( (wifi_event_sta_disconnected_t*)event_data );

			/* se uma mensagem DISCONNECT for postada enquanto uma varredura estiver em andamento, ela NUNCA terminará, fazendo com que a varredura nunca funcione novamente. Por este motivo, SCAN_BIT também foi apagado */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);

			/* pós evento de desconexão com código de razão */
			wifi_manager_send_message(WM_EVENT_STA_DISCONNECTED, (void*)wifi_event_sta_disconnected );
			break;

		/* Este evento surge quando o AP ao qual a estação está conectada muda seu modo de autenticação, por exemplo, sem autenticação
		 * para WPA. Ao receber este evento, a tarefa de evento não fará nada. Geralmente, o retorno de chamada do evento do aplicativo
		 * não precisa lidar com isso também. */
		case WIFI_EVENT_STA_AUTHMODE_CHANGE:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
			break;

		case WIFI_EVENT_AP_START:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

		case WIFI_EVENT_AP_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

		/* Cada vez que uma estação é conectada ao ESP32 AP, o <WIFI_EVENT_AP_STACONNECTED> aparecerá. Ao receber este
		 * evento, a tarefa de evento não fará nada e o retorno de chamada do aplicativo também pode ignorá-lo. No entanto, você pode querer
		 * fazer algo, por exemplo, para obter as informações da STA conectada, etc. */
		case WIFI_EVENT_AP_STACONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
			break;

		/* Este evento pode acontecer nas seguintes situações:
		 *   O aplicativo chama esp_wifi_disconnect (), ou esp_wifi_deauth_sta (), para desconectar manualmente a estação.
		 *   O driver Wi-Fi inicia a estação, por exemplo, porque o AP não recebeu nenhum pacote nos últimos cinco minutos, etc.
		 *   A estação dá início ao AP.
		 * Quando esse evento acontece, a tarefa do evento não fará nada, mas o retorno de chamada do evento do aplicativo precisa fazer
		 * algo, por exemplo, feche o soquete que está relacionado a esta estação, etc.. */
		case WIFI_EVENT_AP_STADISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
			break;

		/* Este evento está desabilitado por padrão. O aplicativo pode habilitá-lo via API esp_wifi_set_event_mask().
		 * Quando este evento está habilitado, ele será gerado cada vez que o AP receber uma solicitação de sondagem. */
		case WIFI_EVENT_AP_PROBEREQRECVED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
			break;

		} /* end switch */
	}
	else if(event_base == IP_EVENT){

		switch(event_id){

		/* Este evento surge quando o cliente DHCP obtém com sucesso o endereço IPV4 do servidor DHCP,
		 * ou quando o endereço IPV4 é alterado. O evento significa que tudo está pronto e o aplicativo pode começar
		 * suas tarefas (e.g., criando soquetes).
		 * O IPV4 pode ser alterado pelos seguintes motivos:
		 *    O cliente DHCP não consegue renovar / religar o endereço IPV4 e o IPV4 da estação é redefinido para 0.
		 *    O cliente DHCP é religado a um endereço diferente.
		 *    O endereço IPV4 com configuração estática foi alterado.
		 * Se o endereço IPV4 é alterado ou NÃO é indicado pelo campo ip_change de ip_event_got_ip_t.
		 * O soquete é baseado no endereço IPV4, o que significa que, se o IPV4 mudar, todos os soquetes relacionados a este
		 * IPV4 se tornará anormal. Ao receber este evento, o aplicativo precisa fechar todos os sockets e recriar
		 * a aplicação quando o IPV4 muda para um válido. */
		case IP_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
	        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
	        ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)malloc(sizeof(ip_event_got_ip_t));
			*ip_event_got_ip =  *( (ip_event_got_ip_t*)event_data );
	        wifi_manager_send_message(WM_EVENT_STA_GOT_IP, (void*)(ip_event_got_ip) );
			break;

		/* Este evento surge quando o suporte IPV6 SLAAC configura automaticamente um endereço para o ESP32, ou quando este endereço muda.
		 * O evento significa que tudo está pronto e o aplicativo pode iniciar suas tarefas (por exemplo, criar soquetes). */
		case IP_EVENT_GOT_IP6:
			ESP_LOGI(TAG, "IP_EVENT_GOT_IP6");
			break;

		/* Este evento surge quando o endereço IPV4 se torna inválido.
		 * IP_STA_LOST_IP não surge imediatamente após a desconexão do WiFi, em vez disso, ele inicia um cronômetro de perda de endereço IPV4,
		 * se o endereço IPV4 for obtido antes que o temporizador de perda de ip expire, IP_EVENT_STA_LOST_IP não acontece. Caso contrário, o evento
		 * surge quando o temporizador de perda de endereço IPV4 expira.
		 * Geralmente, o aplicativo não precisa se preocupar com esse evento, é apenas um evento de depuração para permitir que o aplicativo
		 * saiba que o endereço IPV4 foi perdido. */
		case IP_EVENT_STA_LOST_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_LOST_IP");
			break;

		}
	}

}


wifi_config_t* wifi_manager_get_wifi_sta_config(){
	return wifi_manager_config_sta;
}


void wifi_manager_connect_async(){
	/* para evitar um falso positivo no aplicativo front-end, precisamos liberar rapidamente o ip json
	 * Existe o risco de o front end ver um erro de IP ou senha quando na verdade
	 * é um resquício de uma conexão anterior
	 */
	if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
		wifi_manager_clear_ip_info_json();
		wifi_manager_unlock_json_buffer();
	}
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_USER);
}


char* wifi_manager_get_ip_info_json(){
	return ip_info_json;
}


void wifi_manager_destroy(){

	vTaskDelete(task_wifi_manager);
	task_wifi_manager = NULL;

	/* heap buffers */
	free(accessp_records);
	accessp_records = NULL;
	free(accessp_json);
	accessp_json = NULL;
	free(ip_info_json);
	ip_info_json = NULL;
	free(wifi_manager_sta_ip);
	wifi_manager_sta_ip = NULL;
	if(wifi_manager_config_sta){
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vSemaphoreDelete(wifi_manager_sta_ip_mutex);
	wifi_manager_sta_ip_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);
	wifi_manager_event_group = NULL;
	vQueueDelete(wifi_manager_queue);
	wifi_manager_queue = NULL;


}


void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps) {
	int total_unique;
	wifi_ap_record_t * first_free;
	total_unique=*aps;

	first_free=NULL;

	for(int i=0; i<*aps-1;i++) {
		wifi_ap_record_t * ap = &aplist[i];

		/* pule os APs removidos anteriormente */
		if (ap->ssid[0] == 0) continue;

		/* remove the identical SSID+authmodes */
		for(int j=i+1; j<*aps;j++) {
			wifi_ap_record_t * ap1 = &aplist[j];
			if ( (strcmp((const char *)ap->ssid, (const char *)ap1->ssid)==0) && 
			     (ap->authmode == ap1->authmode) ) { /* mesmo SSID, modo de autenticação diferente é ignorado */
				/* salve o rssi para o display */
				if ((ap1->rssi) > (ap->rssi)) ap->rssi=ap1->rssi;
				/* limpando o registro */
				memset(ap1,0, sizeof(wifi_ap_record_t));
			}
		}
	}
	/* reordene a lista para que os APs sigam uns aos outros na lista */
	for(int i=0; i<*aps;i++) {
		wifi_ap_record_t * ap = &aplist[i];
		/* pulando tudo que não tem nome */
		if (ap->ssid[0] == 0) {
			/* marque o primeiro slot livre */
			if (first_free==NULL) first_free=ap;
			total_unique--;
			continue;
		}
		if (first_free!=NULL) {
			memcpy(first_free, ap, sizeof(wifi_ap_record_t));
			memset(ap,0, sizeof(wifi_ap_record_t));
			/* encontre o próximo slot livre */
			for(int j=0; j<*aps;j++) {
				if (aplist[j].ssid[0]==0) {
					first_free=&aplist[j];
					break;
				}
			}
		}
	}
	/* atualize o comprimento da lista */
	*aps = total_unique;
}


BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSendToFront( wifi_manager_queue, &msg, portMAX_DELAY);
}

BaseType_t wifi_manager_send_message(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSend( wifi_manager_queue, &msg, portMAX_DELAY);
}


void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) ){

	if(cb_ptr_arr && message_code < WM_MESSAGE_CODE_COUNT){
		cb_ptr_arr[message_code] = func_ptr;
	}
}

esp_netif_t* wifi_manager_get_esp_netif_ap(){
	return esp_netif_ap;
}

esp_netif_t* wifi_manager_get_esp_netif_sta(){
	return esp_netif_sta;
}

void wifi_manager( void * pvParameters ){


	queue_message msg;
	BaseType_t xStatus;
	EventBits_t uxBits;
	uint8_t	retries = 0;


	/* inicializar a pilha tcp */
	ESP_ERROR_CHECK(esp_netif_init());

	/* loop de eventos para o driver wi-fi */
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_sta = esp_netif_create_default_wifi_sta();
	esp_netif_ap = esp_netif_create_default_wifi_ap();


	/* configuração wi-fi padrão */
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	/* manipulador de eventos para a conexão */
    esp_event_handler_instance_t instance_wifi_event;
    esp_event_handler_instance_t instance_ip_event;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_wifi_event));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_ip_event));


	/* SoftAP - Definição da configuração do ponto de acesso Wifi */
	wifi_config_t ap_config = {
		.ap = {
			.ssid_len = 0,
			.channel = wifi_settings.ap_channel,
			.ssid_hidden = wifi_settings.ap_ssid_hidden,
			.max_connection = DEFAULT_AP_MAX_CONNECTIONS,
			.beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
		},
	};
	memcpy(ap_config.ap.ssid, wifi_settings.ap_ssid , sizeof(wifi_settings.ap_ssid));

	/* se o comprimento da senha for inferior a 8 caracteres, que é o mínimo para WPA2, o ponto de acesso começa como aberto */
	if(strlen( (char*)wifi_settings.ap_pwd) < WPA2_MINIMUM_PASSWORD_LENGTH){
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
		memset( ap_config.ap.password, 0x00, sizeof(ap_config.ap.password) );
	}
	else{
		ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
		memcpy(ap_config.ap.password, wifi_settings.ap_pwd, sizeof(wifi_settings.ap_pwd));
	}
	

	/* DHCP AP configuration */
	esp_netif_dhcps_stop(esp_netif_ap); /* O cliente/servidor DHCP deve ser interrompido antes de definir novas informações de IP. */
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth));
	ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_settings.sta_power_save));


	/* por padrão, o modo é STA porque wifi_manager não iniciará o ponto de acesso a menos que seja necessário! */
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* iniciar servidor http */
	http_app_start(false);

	/* configuração do scanner wi-fi */
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};

	/* enfileirar o primeiro evento: carregar a configuração anterior */
	wifi_manager_send_message(WM_ORDER_LOAD_AND_RESTORE_STA, NULL);


	/* loop de processamento principal */
	for(;;){
		xStatus = xQueueReceive( wifi_manager_queue, &msg, portMAX_DELAY );

		if( xStatus == pdPASS ){
			switch(msg.code){

			case WM_EVENT_SCAN_DONE:{
				wifi_event_sta_scan_done_t *evt_scan_done = (wifi_event_sta_scan_done_t*)msg.param;
				/* apenas verifique se há AP se a varredura for bem-sucedida */
				if(evt_scan_done->status == 0){
					/* Como parâmetro de entrada, ele armazena o número máximo de AP que ap_records podem conter. Como parâmetro de saída, ele recebe o número real do AP que esta API retorna.
					* Como consequência, ap_num DEVE ser redefinido para MAX_AP_NUM a cada varredura */
					ap_num = MAX_AP_NUM;
					ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));
					/* certifique-se de que o servidor http não está tentando acessar a lista enquanto ela é atualizada */
					if(wifi_manager_lock_json_buffer( pdMS_TO_TICKS(1000) )){
						/* Irá remover os SSIDs duplicados da lista e atualizar ap_num */
						wifi_manager_filter_unique(accessp_records, &ap_num);
						wifi_manager_generate_acess_points_json();
						wifi_manager_unlock_json_buffer();
					}
					else{
						ESP_LOGE(TAG, "could not get access to json mutex in wifi_scan");
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(evt_scan_done);
				}
				break;

			case WM_ORDER_START_WIFI_SCAN:
				ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");

				/* se uma varredura já estiver em andamento, esta mensagem será simplesmente ignorada graças ao WIFI_MANAGER_SCAN_BIT uxBit */
				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if(! (uxBits & WIFI_MANAGER_SCAN_BIT) ){
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
					ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_LOAD_AND_RESTORE_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
				if(wifi_manager_fetch_wifi_sta_config()){
					ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to connect.");
					wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_RESTORE_CONNECTION);
				}
				else{
					/* nenhum wi-fi salvo: inicie o soft AP! Isso é o que deve acontecer durante a primeira execução */
					ESP_LOGI(TAG, "No saved wifi found on startup. Starting access point.");
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_CONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_CONNECT_STA");

				/* muito importante: preciso que esta tentativa de conexão seja especificamente solicitada.
				 * O parâmetro nesse caso é um booleano que indica se a solicitação foi feita automaticamente
				 * pelo wifi_manager.
				 * */
				if((BaseType_t)msg.param == CONNECTION_REQUEST_USER) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
				}
				else if((BaseType_t)msg.param == CONNECTION_REQUEST_RESTORE_CONNECTION) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( ! (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) ){
					/* atualize a configuração para a última e tente a conexão */
					ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_manager_get_wifi_sta_config()));

					/* se houver uma varredura de wi-fi em andamento, cancele-a primeiro
					   Chamar esp_wifi_scan_stop irá disparar um evento SCAN_DONE que irá reiniciar este bit */
					if(uxBits & WIFI_MANAGER_SCAN_BIT){
						esp_wifi_scan_stop();
					}
					ESP_ERROR_CHECK(esp_wifi_connect());
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_EVENT_STA_DISCONNECTED:
				;wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)msg.param;
				ESP_LOGI(TAG, "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", wifi_event_sta_disconnected->reason);

				/* isso ainda pode ser postado em várias condições diferentes
				 *
				 * 1. A senha SSID está errada
				 * 2. Desconexão manual solicitada
				 * 3. Conexão perdida
				 *
				 * Ter um entendimento claro de PORQUE o evento foi postado é fundamental para ter um gerenciador de wi-fi eficiente
				 *
				 * Com wifi_manager, determinamos:
				 *  Se WIFI_MANAGER_REQUEST_STA_CONNECT_BIT está definido, consideramos que é um cliente que solicitou a conexão.
				 *    Quando SYSTEM_EVENT_STA_DISCONNECTED é postado, provavelmente é uma senha / algo deu errado com o aperto de mão.
				 *
				 *  Se WIFI_MANAGER_REQUEST_STA_CONNECT_BIT está definido, é uma desconexão que foi PERGUNTADA pelo cliente (clicando em desconectar no aplicativo)
				 *    Quando SYSTEM_EVENT_STA_DISCONNECTED for postado, o wi-fi salvo será apagado da memória NVS.
				 *
				 *  Se WIFI_MANAGER_REQUEST_STA_CONNECT_BIT e WIFI_MANAGER_REQUEST_STA_CONNECT_BIT NÃO estão configurados, é uma conexão perdida
				 *
				 *  Nesta versão do software, os códigos de razão não são usados. Eles são indicados aqui para uso futuro em potencial.
				 *
				 *  CÓDIGO DE RAZÃO:
				 *  1		UNSPECIFIED
				 *  2		AUTH_EXPIRE					auth não é mais válido, isso cheira como se alguém alterasse uma senha no AP
				 *  3		AUTH_LEAVE
				 *  4		ASSOC_EXPIRE
				 *  5		ASSOC_TOOMANY				muitos dispositivos já conectados ao AP => AP não responde
				 *  6		NOT_AUTHED
				 *  7		NOT_ASSOCED
				 *  8		ASSOC_LEAVE					testado como desconexão manual pelo usuário OU na lista negra do MAC sem fio
				 *  9		ASSOC_NOT_AUTHED
				 *  10		DISASSOC_PWRCAP_BAD
				 *  11		DISASSOC_SUPCHAN_BAD
				 *	12		<n/a>
				 *  13		IE_INVALID
				 *  14		MIC_FAILURE
				 *  15		4WAY_HANDSHAKE_TIMEOUT		senha incorreta! Isso foi testado pessoalmente no wi-fi de minha casa com uma senha errada.
				 *  16		GROUP_KEY_UPDATE_TIMEOUT
				 *  17		IE_IN_4WAY_DIFFERS
				 *  18		GROUP_CIPHER_INVALID
				 *  19		PAIRWISE_CIPHER_INVALID
				 *  20		AKMP_INVALID
				 *  21		UNSUPP_RSN_IE_VERSION
				 *  22		INVALID_RSN_IE_CAP
				 *  23		802_1X_AUTH_FAILED			wrong password?
				 *  24		CIPHER_SUITE_REJECTED
				 *  200		BEACON_TIMEOUT
				 *  201		NO_AP_FOUND
				 *  202		AUTH_FAIL
				 *  203		ASSOC_FAIL
				 *  204		HANDSHAKE_TIMEOUT
				 *
				 * */

				/* redefinir o IP da equipe salvo */
				wifi_manager_safe_update_sta_ip_string((uint32_t)0);

				/* se houvesse um temporizador para parar o AP, agora é hora de cancelar já que a conexão foi perdida! */
				if(xTimerIsTimerActive(wifi_manager_shutdown_ap_timer) == pdTRUE ){
					xTimerStop( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ){
					/* não há novas tentativas quando é uma conexão solicitada pelo usuário por design. Isso evita que o usuário se pendure muito
					 * no caso de terem digitado uma senha errada, por exemplo. Aqui, simplesmente limpamos o bit de solicitação e seguimos em frente */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_FAILED_ATTEMPT );
						wifi_manager_unlock_json_buffer();
					}

				}
				else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT){
					/* o usuário solicitou manualmente uma desconexão para que a conexão perdida seja um evento normal. Limpe a bandeira e reinicie o AP */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

					/* apagar configuração */
					if(wifi_manager_config_sta){
						memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
					}

					/* regenerar status json */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_USER_DISCONNECT );
						wifi_manager_unlock_json_buffer();
					}

					/* salvar memória NVS */
					wifi_manager_save_sta_config();

					/* iniciar SoftAP */
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}
				else{
					/* conexão perdida ? */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_LOST_CONNECTION );
						wifi_manager_unlock_json_buffer();
					}

					/* Inicie o cronômetro que tentará restaurar a configuração salva */
					xTimerStart( wifi_manager_retry_timer, (TickType_t)0 );

					/* se foi uma tentativa de restauração de conexão, limpamos o bit */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

					/* se o AP não for iniciado, verificamos se atingimos o limite de tentativa fracassada de iniciá-lo */
					if(! (uxBits & WIFI_MANAGER_AP_STARTED_BIT) ){

						/* se o número de tentativas estiver abaixo do limite para iniciar o AP, uma tentativa de reconexão é feita
						 * Desta forma, evitamos reiniciar o AP diretamente no caso de a conexão ser momentaneamente perdida */
						if(retries < WIFI_MANAGER_MAX_RETRY_START_AP){
							retries++;
						}
						else{
							/* Neste cenário, a conexão foi perdida sem possibilidade de reparo: inicie o AP! */
							retries = 0;

							/* iniciar SoftAP */
							wifi_manager_send_message(WM_ORDER_START_AP, NULL);
						}
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(wifi_event_sta_disconnected);

				break;

			case WM_ORDER_START_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_START_AP");

				ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

				/* reinicie o daemon HTTP */
				http_app_stop();
				http_app_start(true);

				/* iniciar DNS */
				dns_server_start();

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_STOP_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_STOP_AP");


				uxBits = xEventGroupGetBits(wifi_manager_event_group);

				/* antes de parar o AP, verificamos se ainda estamos conectados. Há uma chance de que uma vez que o cronômetro
				 * entra em ação, por qualquer motivo, o esp32 já está desconectado.
				 */
				if(uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT){

					/* definir apenas para STA */
					esp_wifi_set_mode(WIFI_MODE_STA);

					/* parar DNS */
					dns_server_stop();

					/* reinicie o daemon HTTP */
					http_app_stop();
					http_app_start(false);

					/* callback */
					if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
				}

				break;

			case WM_EVENT_STA_GOT_IP:
				ESP_LOGI(TAG, "WM_EVENT_STA_GOT_IP");
				ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)msg.param; 
				uxBits = xEventGroupGetBits(wifi_manager_event_group);

				/* redefinir a conexão solicita bits - não importa se foi definida ou não */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

				/* salvar o IP como uma string para o host do servidor HTTP */
				wifi_manager_safe_update_sta_ip_string(ip_event_got_ip->ip_info.ip.addr);

				/* salvar configuração wi-fi em NVS se não foi restaurada de uma conexão */
				if(uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT){
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}
				else{
					wifi_manager_save_sta_config();
				}

				/* redefinir o número de tentativas */
				retries = 0;

				/* atualize JSON com o novo IP */
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
					/* gerar as informações de conexão com sucesso */
					wifi_manager_generate_ip_info_json( UPDATE_CONNECTION_OK );
					wifi_manager_unlock_json_buffer();
				}
				else { abort(); }

				/* derrubar sequestro de DNS */
				dns_server_stop();

				/* iniciar o cronômetro que eventualmente desligará o ponto de acesso
				 * Verificamos primeiro se ele está realmente em execução porque, no caso de uma conexão de inicialização e restauração
				 * o AP nem começou para começar.
				 */
				if(uxBits & WIFI_MANAGER_AP_STARTED_BIT){
					TickType_t t = pdMS_TO_TICKS( WIFI_MANAGER_SHUTDOWN_AP_TIMER );

					/* se por algum motivo o usuário configurou o temporizador de desligamento para menos de 1 tick, o AP é interrompido imediatamente */
					if(t > 0){
						xTimerStart( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
					}
					else{
						wifi_manager_send_message(WM_ORDER_STOP_AP, (void*)NULL);
					}

				}

				/* retorno de chamada e memória livre alocada para o parâmetro void * */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(ip_event_got_ip);

				break;

			case WM_ORDER_DISCONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_DISCONNECT_STA");

				/* preciso, isso vem de uma solicitação do usuário */
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

				/* pedir disconect wi-fi */
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			default:
				break;

			} /* end of switch/case */
		} /* end of if status=pdPASS */
	} /* end of for loop */

	vTaskDelete( NULL );

}


