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

@file nvs_sync.h
@author Tony Pottier
@brief Exposes a simple API to synchronize NVS memory read and writes

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/



#ifndef WIFI_MANAGER_NVS_SYNC_H_INCLUDED
#define WIFI_MANAGER_NVS_SYNC_H_INCLUDED

#include <stdbool.h> /* para o tipo bool */
#include <freertos/FreeRTOS.h> /* para TickType_t */
#include <esp_err.h> /* para esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Tenta obter o semáforo NVS para uma determinada quantidade de tiques.
 * @note Se você não tiver certeza sobre o número de tiques a esperar, use portMAX_DELAY.
 * @return verdadeiro em um bloqueio bem-sucedido, falso caso contrário
 */
bool nvs_sync_lock(TickType_t xTicksToWait);


/**
 * @brief Libera o semáforo NVS
 */
void nvs_sync_unlock();


/** 
 * @brief Crie o semáforo NVS
 * @return      ESP_OK: sucesso ou se o semáforo já existe
 *              ESP_FAIL: fracasso
 */ 
esp_err_t nvs_sync_create();

/**
 * @brief Libera memória associada ao semáforo NVS
 * @warning Não exclua um semáforo que contenha tarefas bloqueadas (tarefas que estão no estado Bloqueado aguardando a disponibilização do semáforo).
 */
void nvs_sync_free();


#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_NVS_SYNC_H_INCLUDED */