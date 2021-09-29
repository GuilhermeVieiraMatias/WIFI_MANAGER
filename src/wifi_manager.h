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

@file wifi_manager.h
@author Tony Pottier
@brief Define todas as funções necessárias para o esp32 se conectar a um wi-fi / scan wifis

Contém a tarefa freeRTOS e todo o suporte necessário

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef WIFI_MANAGER_H_INCLUDED
#define WIFI_MANAGER_H_INCLUDED

#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Define o tamanho máximo de um nome SSID. 32 é o padrão IEEE.
 * @warning limit também está codificado em wifi_config_t. Nunca estenda este valor.
 */
#define MAX_SSID_SIZE						32

/**
 * @brief Define o tamanho máximo de uma chave de acesso WPA2. 64 é o padrão IEEE.
 * @warning limit também está codificado em wifi_config_t. Nunca estenda este valor.
 */
#define MAX_PASSWORD_SIZE					64


/**
 * @brief Define o número máximo de pontos de acesso que podem ser verificados.
 *
 * Para economizar memória e evitar erros desagradáveis ​​de falta de memória,
 * podemos limitar o número de APs detectados em uma varredura de wi-fi.
 */
#define MAX_AP_NUM 							15


/**
 * @brief Define o número máximo de tentativas com falha permitidas antes que o gerenciador de WiFi inicie seu próprio ponto de acesso.
 * Setting para 2, por exemplo, significa que haverá 3 tentativas no total (solicitação original + 2 tentativas)
 */
#define WIFI_MANAGER_MAX_RETRY_START_AP		CONFIG_WIFI_MANAGER_MAX_RETRY_START_AP

/**
 * @brief Time (in ms) entre cada nova tentativa
 * Define o tempo de espera antes de ser feita uma tentativa de reconectar a um wi-fi salvo após a conexão ser perdida ou outra tentativa malsucedida.
 */
#define WIFI_MANAGER_RETRY_TIMER			CONFIG_WIFI_MANAGER_RETRY_TIMER


/**
 * @brief Time (in ms) esperar antes de desligar o AP
 * Define o tempo (em ms) de espera após uma conexão bem-sucedida antes de desligar o ponto de acesso.
 */
#define WIFI_MANAGER_SHUTDOWN_AP_TIMER		CONFIG_WIFI_MANAGER_SHUTDOWN_AP_TIMER


/** @brief Define a prioridade da tarefa do wifi_manager.
 *
 * As tarefas geradas pelo gerenciador terão prioridade WIFI_MANAGER_TASK_PRIORITY-1.
 * Por este motivo específico, a prioridade mínima da tarefa é 1. É altamente não recomendado definir
 * para 1, visto que as subtarefas terão agora uma prioridade de 0 que é a prioridade
 * da tarefa ociosa do freeRTOS.
 */
#define WIFI_MANAGER_TASK_PRIORITY			CONFIG_WIFI_MANAGER_TASK_PRIORITY

/** @brief Define o modo de autenticação como um ponto de acesso
 *  O valor deve ser do tipo wifi_auth_mode_t
 *  @see esp_wifi_types.h
 *  @warning se definido como WIFI_AUTH_OPEN, passowrd me estará vazio. Consulte DEFAULT_AP_PASSWORD.
 */
#define AP_AUTHMODE 						WIFI_AUTH_WPA2_PSK

/** @brief Define a visibilidade do ponto de acesso. 0: AP visível. 1: oculto */
#define DEFAULT_AP_SSID_HIDDEN 				0

/** @brief Define o nome do ponto de acesso. Valor padrão: esp32. Execute 'make menuconfig' para configurar o seu próprio valor ou substitua aqui por uma string */
#define DEFAULT_AP_SSID 					CONFIG_DEFAULT_AP_SSID

/** @brief Define a senha do ponto de acesso.
 *	@warning No caso de um ponto de acesso aberto, a senha deve ser uma string nula "" ou "\ 0" se você quiser ser prolixo, mas desperdiçar um byte.
 *	Além disso, o AP_AUTHMODE deve ser WIFI_AUTH_OPEN
 */
#define DEFAULT_AP_PASSWORD 				CONFIG_DEFAULT_AP_PASSWORD

/** @brief Define o nome do host transmitido por mDNS */
#define DEFAULT_HOSTNAME					"esp32"

/** @brief Define a largura de banda do ponto de acesso.
 *  Value: WIFI_BW_HT20 para 20 MHz ou WIFI_BW_HT40 para 40 MHz
 *  20 MHz minimiza a interferência do canal, mas não é adequado para
 *  aplicativos com alta velocidade de dados
 */
#define DEFAULT_AP_BANDWIDTH 					WIFI_BW_HT20

/** @brief Define o canal do ponto de acesso.
 *  A seleção de canal só é eficaz quando não está conectado a outro AP.
 *  Boa prática para usar o mínimo de interferência no canal
 *  Para 20 MHz: 1, 6 ou 11 nos EUA e 1, 5, 9 ou 13 na maioria das partes do mundo
 *  Para 40 MHz: 3 nos EUA e 3 ou 11 na maioria das partes do mundo
 */
#define DEFAULT_AP_CHANNEL 					CONFIG_DEFAULT_AP_CHANNEL



/** @brief Define o endereço IP padrão do ponto de acesso. Padrão: "10.10.0.1 * /
#define DEFAULT_AP_IP						CONFIG_DEFAULT_AP_IP

/** @brief Define o gateway do ponto de acesso. Deve ser igual ao seu IP. Padrão: "10.10.0.1" */
#define DEFAULT_AP_GATEWAY					CONFIG_DEFAULT_AP_GATEWAY

/** @brief Define a máscara de rede do ponto de acesso. Padrão: "255.255.255.0" */
#define DEFAULT_AP_NETMASK					CONFIG_DEFAULT_AP_NETMASK

/** @brief Define o número máximo de clientes do ponto de acesso. Padrão: 4 */
#define DEFAULT_AP_MAX_CONNECTIONS		 	CONFIG_DEFAULT_AP_MAX_CONNECTIONS

/** @brief Define o intervalo do beacon do ponto de acesso. 100 ms é o padrão recomendado. */
#define DEFAULT_AP_BEACON_INTERVAL 			CONFIG_DEFAULT_AP_BEACON_INTERVAL

/** @brief Define se esp32 deve executar AP + STA quando conectado a outro AP.
 *  Valor: 0 terá o próprio AP sempre ligado (modo APSTA)
 *  Valor: 1 desligará o próprio AP quando conectado a outro AP (modo somente STA quando conectado)
 *  Desligar o próprio AP quando conectado a outro AP minimiza a interferência do canal e aumenta o rendimento
 */
#define DEFAULT_STA_ONLY 					1

/** @brief Define se a economia de energia wi-fi deve ser ativada.
 *  Value: WIFI_PS_NONE para potência total (modem wi-fi sempre ligado)
 *  Value: WIFI_PS_MODEM para economia de energia (modem wi-fi hiberne periodicamente)
 *  Note: A economia de energia só é eficaz quando no modo STA apenas
 */
#define DEFAULT_STA_POWER_SAVE 				WIFI_PS_NONE

/**
 * @brief Define o comprimento máximo em bytes de uma representação JSON de um ponto de acesso.
 *
 *  comprimento máximo da string ap com 32 char ssid completo: 75 + \\n + \ 0 = 77\n
 *  example: {"ssid":"abcdefghijklmnopqrstuvwxyz012345","chan":12,"rssi":-100,"auth":4},\n
 *  MAS: precisamos escapar de JSON. Imagine um SSID cheio de \"? Então tem mais 32 bytes, portanto, 77 + 32 = 99.\n
 *  este é um caso extremo, mas não acho que devemos falhar de forma catastrófica só porque
 *  alguém decidiu ter um nome wi-fi engraçado.
 */
#define JSON_ONE_APP_SIZE					99

/**
 * @brief Define o comprimento máximo em bytes de uma representação JSON das informações de IP
 * assumindo que todos os ips têm 4 * 3 dígitos e todos os caracteres no SSID precisam ter escape.
 * example: {"ssid":"abcdefghijklmnopqrstuvwxyz012345","ip":"192.168.1.119","netmask":"255.255.255.0","gw":"192.168.1.1","urc":99}
 * Execute este JS (o console do navegador é o mais fácil) para chegar à conclusão de que 159 é o pior caso.
 * ```
 * var a = {"ssid":"abcdefghijklmnopqrstuvwxyz012345","ip":"255.255.255.255","netmask":"255.255.255.255","gw":"255.255.255.255","urc":99};
 * // Substitua todos os caracteres SSID por aspas duplas que deverão ser escapadas
 * a.ssid = a.ssid.split('').map(() => '"').join('');
 * console.log(JSON.stringify(a).length); // => 158 +1 for null
 * console.log(JSON.stringify(a)); // print it
 * ```
 */
#define JSON_IP_INFO_SIZE 					159


/**
 * @brief define o comprimento mínimo de uma senha de ponto de acesso em execução em WPA2
 */
#define WPA2_MINIMUM_PASSWORD_LENGTH		8


/**
 * @brief Define a lista completa de todas as mensagens que o wifi_manager pode processar.
 *
 * Algumas dessas mensagens são eventos ("EVENTO") e algumas delas são ações ("PEDIDO")
 * Cada uma dessas mensagens pode acionar uma função de retorno de chamada e cada função de retorno é armazenada
 * em uma matriz de ponteiro de função por conveniência. Por causa desse comportamento, é extremamente importante
 * para manter uma sequência estrita e o elemento especial de nível superior 'MESSAGE_CODE_COUNT'
 *
 * @see wifi_manager_set_callback
 */
typedef enum message_code_t {
	NONE = 0,
	WM_ORDER_START_HTTP_SERVER = 1,
	WM_ORDER_STOP_HTTP_SERVER = 2,
	WM_ORDER_START_DNS_SERVICE = 3,
	WM_ORDER_STOP_DNS_SERVICE = 4,
	WM_ORDER_START_WIFI_SCAN = 5,
	WM_ORDER_LOAD_AND_RESTORE_STA = 6,
	WM_ORDER_CONNECT_STA = 7,
	WM_ORDER_DISCONNECT_STA = 8,
	WM_ORDER_START_AP = 9,
	WM_EVENT_STA_DISCONNECTED = 10,
	WM_EVENT_SCAN_DONE = 11,
	WM_EVENT_STA_GOT_IP = 12,
	WM_ORDER_STOP_AP = 13,
	WM_MESSAGE_CODE_COUNT = 14 /* important for the callback array */

}message_code_t;

/**
 * @brief códigos de motivo simplificados para uma conexão perdida.
 *
 * esp-idf mantém uma grande lista de códigos de razão que, na prática, são inúteis para a maioria das aplicações típicas.
 */
typedef enum update_reason_code_t {
	UPDATE_CONNECTION_OK = 0,
	UPDATE_FAILED_ATTEMPT = 1,
	UPDATE_USER_DISCONNECT = 2,
	UPDATE_LOST_CONNECTION = 3
}update_reason_code_t;

typedef enum connection_request_made_by_code_t{
	CONNECTION_REQUEST_NONE = 0,
	CONNECTION_REQUEST_USER = 1,
	CONNECTION_REQUEST_AUTO_RECONNECT = 2,
	CONNECTION_REQUEST_RESTORE_CONNECTION = 3,
	CONNECTION_REQUEST_MAX = 0x7fffffff /*forçar a criação deste enum como um 32 bit int */
}connection_request_made_by_code_t;

/**
 * As configurações reais de WiFi em uso
 */
struct wifi_settings_t{
	uint8_t ap_ssid[MAX_SSID_SIZE];
	uint8_t ap_pwd[MAX_PASSWORD_SIZE];
	uint8_t ap_channel;
	uint8_t ap_ssid_hidden;
	wifi_bandwidth_t ap_bandwidth;
	bool sta_only;
	wifi_ps_type_t sta_power_save;
	bool sta_static_ip;
	esp_netif_ip_info_t sta_static_ip_config;
};
extern struct wifi_settings_t wifi_settings;


/**
 * @brief Estrutura usada para armazenar uma mensagem na fila.
 */
typedef struct{
	message_code_t code;
	void *param;
} queue_message;


/**
 * @brief retorna o objeto esp_netif atual para a STAtion
 */
esp_netif_t* wifi_manager_get_esp_netif_sta();

/**
 * @brief retorna o objeto esp_netif atual para o Ponto de Acesso
 */
esp_netif_t* wifi_manager_get_esp_netif_ap();


/**
 * Alocar memória heap para o gerenciador wifi e iniciar a tarefa RTOS wifi_manager
 */
void wifi_manager_start();

/**
 * Libera toda a memória alocada pelo wifi_manager e elimina a tarefa.
 */
void wifi_manager_destroy();

/**
 * Filtra a lista de varredura de AP para SSIDs exclusivos
 */
void filter_unique( wifi_ap_record_t * aplist, uint16_t * ap_num);

/**
 * Tarefa principal para o wifi_manager
 */
void wifi_manager( void * pvParameters );


char* wifi_manager_get_ap_list_json();
char* wifi_manager_get_ip_info_json();


void wifi_manager_scan_async();


/**
 * @brief salva a configuração atual do STA wifi no armazenamento de memória flash.
 */
esp_err_t wifi_manager_save_sta_config();

/**
 * @brief buscar uma configuração Wi-Fi STA anterior no armazenamento de memória flash.
 * @return verdadeiro se uma configuração salva anteriormente for encontrada, falso caso contrário.
 */
bool wifi_manager_fetch_wifi_sta_config();

wifi_config_t* wifi_manager_get_wifi_sta_config();


/**
 * @brief solicita uma conexão a um ponto de acesso que será processado no thread de tarefa principal.
 */
void wifi_manager_connect_async();

/**
 * @brief solicita uma verificação de wi-fi
 */
void wifi_manager_scan_awifi_manager_send_messagesync();

/**
 * @brief solicitações para desconectar e esquecer o ponto de acesso.
 */
void wifi_manager_disconnect_async();

/**
 * @brief Tenta obter acesso a json buffer mutex.
 *
 * O servidor HTTP pode tentar acessar o json para atender clientes enquanto o thread do gerenciador de wi-fi pode tentar
 * para atualizá-lo. Essas duas tarefas são sincronizadas por meio de um mutex.
 *
 * O mutex é usado pela lista de pontos de acesso json e pelo status da conexão json.\n
 * Esses dois recursos deveriam ter tecnicamente seu próprio mutex, mas perdemos alguma flexibilidade para economizar
 * na memória.
 *
 * Este é um invólucro simples em torno da função xSemaphoreTake do freeRTOS.
 *
 * @param xTicksToWait O tempo em ticks de espera para que o semáforo fique disponível.
 * @return verdadeiro em caso de sucesso, falso caso contrário.
 */
bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait);

/**
 * @brief Libera o mutex do buffer json.
 */
void wifi_manager_unlock_json_buffer();

/**
 * @brief Gera o status de conexão json: ssid e endereços IP.
 * @note Isso não é seguro para thread e deve ser chamado apenas se a chamada wifi_manager_lock_json_buffer for bem-sucedida.
 */
void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code);
/**
 * @brief Limpa o json de status de conexão.
 * @note Isso não é seguro para thread e deve ser chamado apenas se a chamada wifi_manager_lock_json_buffer for bem-sucedida.
 */
void wifi_manager_clear_ip_info_json();

/**
 * @brief Gera a lista de pontos de acesso após uma verificação de wi-fi.
 * @note Isso não é seguro para thread e deve ser chamado apenas se a chamada wifi_manager_lock_json_buffer for bem-sucedida.
 */
void wifi_manager_generate_acess_points_json();

/**
 * @brief Limpe a lista de pontos de acesso.
 * @note Isso não é seguro para thread e deve ser chamado apenas se a chamada wifi_manager_lock_json_buffer for bem-sucedida.
 */
void wifi_manager_clear_access_points_json();


/**
 * @brief Inicie o serviço mDNS
 */
void wifi_manager_initialise_mdns();


bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait);
void wifi_manager_unlock_sta_ip_string();

/**
 * @brief obtém a representação em string do endereço IP da STA, e.g.: "192.168.1.69"
 */
char* wifi_manager_get_sta_ip_string();

/**
 * @brief representação char segura de thread da atualização do STA IP
 */
void wifi_manager_safe_update_sta_ip_string(uint32_t ip);


/**
 * @brief Registre um retorno de chamada para uma função personalizada quando um evento específico message_code acontecer.
 */
void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) );


BaseType_t wifi_manager_send_message(message_code_t code, void *param);
BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H_INCLUDED */
