/**
Copyright (c) 2020 Tony Pottier

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

@file nvs_sync.c
@author Tony Pottier
@brief Exposes a simple API to synchronize NVS memory read and writes

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include "nvs_sync.h"


static SemaphoreHandle_t nvs_sync_mutex = NULL;

esp_err_t nvs_sync_create(){
    if(nvs_sync_mutex == NULL){

        nvs_sync_mutex = xSemaphoreCreateMutex();

		if(nvs_sync_mutex){
			return ESP_OK;
		}
		else{
			return ESP_FAIL;
		}
    }
	else{
		return ESP_OK;
	}
}

void nvs_sync_free(){
    if(nvs_sync_mutex != NULL){
        vSemaphoreDelete( nvs_sync_mutex );
        nvs_sync_mutex = NULL;
    }
}

bool nvs_sync_lock(TickType_t xTicksToWait){
	if(nvs_sync_mutex){
		if( xSemaphoreTake( nvs_sync_mutex, xTicksToWait ) == pdTRUE ) {
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

void nvs_sync_unlock(){
	xSemaphoreGive( nvs_sync_mutex );
}