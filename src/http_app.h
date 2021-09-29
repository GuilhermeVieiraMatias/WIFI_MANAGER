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

@file http_app.h
@author Tony Pottier
@brief Define todas as funções necessárias para a execução do servidor HTTP.

Contém a tarefa freeRTOS para o ouvinte HTTP e todo o suporte necessário
função para processar solicitações, decodificar URLs, servir arquivos, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef HTTP_APP_H_INCLUDED
#define HTTP_APP_H_INCLUDED

#include <stdbool.h>
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Define o URL onde o gerenciador de wi-fi está localizado
 *  Por padrão, está na raiz do servidor (ou seja, "/"). Se você deseja adicionar suas próprias páginas da web
 *  você pode querer realocar o gerenciador de wi-fi para outro URL, por exemplo / wi-fimanager
 */
#define WEBAPP_LOCATION 					CONFIG_WEBAPP_LOCATION


/** 
 * @brief gera o servidor http 
 */
void http_app_start(bool lru_purge_enable);

/**
 * @brief pára o servidor http 
 */
void http_app_stop();

/** 
 * @brief define um gancho para os manipuladores de URI do gerenciador de wi-fi. Definir o manipulador como NULL desativa o gancho.
 * @return ESP_OK em caso de sucesso, ESP_ERR_INVALID_ARG se o método não for compatível.
 */
esp_err_t http_app_set_handler_hook( httpd_method_t method,  esp_err_t (*handler)(httpd_req_t *r)  );


#ifdef __cplusplus
}
#endif

#endif
