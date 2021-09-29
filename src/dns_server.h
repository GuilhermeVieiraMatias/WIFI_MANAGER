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

@file dns_server.h
@author Tony Pottier
@brief Define um servidor DNS extremamente básico para a funcionalidade do portal cativo.

Contém a tarefa freeRTOS para o servidor DNS que processa as solicitações.

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
@see http://www.zytrax.com/books/dns/ch15
*/

#ifndef MAIN_DNS_SERVER_H_
#define MAIN_DNS_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif


/** Cabeçalho de 12 bytes, nome de domínio de 64 bytes, qtype / qclass de 4 bytes. NÃO é compatível com o RFC, mas é bom o suficiente para um portal cativo
 * se uma consulta DNS for muito grande, ela simplesmente não será processada. */
#define	DNS_QUERY_MAX_SIZE 80

/** Query + 2 byte ptr, 2 byte type, 2 byte class, 4 byte TTL, 2 byte len, 4 byte data */
#define	DNS_ANSWER_MAX_SIZE (DNS_QUERY_MAX_SIZE+16)


/**
 * @brief RCODE valores usados ​​em uma mensagem de cabeçalho DNS
 */
typedef enum dns_reply_code_t {
	DNS_REPLY_CODE_NO_ERROR = 0,
	DNS_REPLY_CODE_FORM_ERROR = 1,
	DNS_REPLY_CODE_SERVER_FAILURE = 2,
	DNS_REPLY_CODE_NON_EXISTANT_DOMAIN = 3,
	DNS_REPLY_CODE_NOT_IMPLEMENTED = 4,
	DNS_REPLY_CODE_REFUSED = 5,
	DNS_REPLY_CODE_YXDOMAIN = 6,
	DNS_REPLY_CODE_YXRRSET = 7,
	DNS_REPLY_CODE_NXRRSET = 8
}dns_reply_code_t;



/**
 * @brief OPCODE valores usados ​​em uma mensagem de cabeçalho DNS
 */
typedef enum dns_opcode_code_t {
	DNS_OPCODE_QUERY = 0,
	DNS_OPCODE_IQUERY = 1,
	DNS_OPCODE_STATUS = 2
}dns_opcode_code_t;



/**
 * @brief Representa um cabeçalho DNS de 12 bytes.
 * __packed__ é necessário para prevenir alinhamentos de memória indesejáveis ​​potenciais
 */
typedef struct __attribute__((__packed__)) dns_header_t{
  uint16_t ID;         // número de identificação
  uint8_t RD : 1;      // recursão desejada
  uint8_t TC : 1;      // mensagem truncada
  uint8_t AA : 1;      // resposta autoritária
  uint8_t OPCode : 4;  // message_type
  uint8_t QR : 1;      // sinalizador de consulta / resposta
  uint8_t RCode : 4;   // Código de resposta
  uint8_t Z : 3;       // its z! reserved
  uint8_t RA : 1;      // recursão disponível
  uint16_t QDCount;    // número de entradas de perguntas
  uint16_t ANCount;    // número de entradas de resposta
  uint16_t NSCount;    // número de entradas de autoridade
  uint16_t ARCount;    // número de entradas de recursos
}dns_header_t;



typedef enum dns_answer_type_t {
	DNS_ANSWER_TYPE_A = 1,
	DNS_ANSWER_TYPE_NS = 2,
	DNS_ANSWER_TYPE_CNAME = 5,
	DNS_ANSWER_TYPE_SOA = 6,
	DNS_ANSWER_TYPE_WKS = 11,
	DNS_ANSWER_TYPE_PTR = 12,
	DNS_ANSWER_TYPE_MX = 15,
	DNS_ANSWER_TYPE_SRV = 33,
	DNS_ANSWER_TYPE_AAAA = 28
}dns_answer_type_t;

typedef enum dns_answer_class_t {
	DNS_ANSWER_CLASS_IN = 1
}dns_answer_class_t;



typedef struct __attribute__((__packed__)) dns_answer_t{
	uint16_t NAME;	/* por uma questão de simplicidade, apenas ponteiros de 16 bits são suportados */
	uint16_t TYPE; /* Valor de 16 bits sem sinal. Os tipos de registro de recurso - determina o conteúdo do campo RDATA. */
	uint16_t CLASS; /* Classe de resposta. */
	uint32_t TTL; /* O tempo em segundos que o registro pode ser armazenado em cache. Um valor de 0 indica que o registro não deve ser armazenado em cache. */
	uint16_t RDLENGTH; /* Valor de 16 bits sem sinal que define o comprimento em bytes do registro RDATA. */
	uint32_t RDATA; /* Por razões de simplicidade, apenas ipv4 é compatível e, como tal, é um 32 bits não assinado */
}dns_answer_t;

void dns_server(void *pvParameters);
void dns_server_start();
void dns_server_stop();



#ifdef __cplusplus
}
#endif


#endif /* MAIN_DNS_SERVER_H_ */
