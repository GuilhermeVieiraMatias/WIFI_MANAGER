/*
@file json.h
@brief handles very basic JSON with a minimal footprint on the system

Este código é uma versão levemente modificada do cJSON 1.4.7. cJSON é licenciado sob a licença MIT:
Copyright (c) 2009 Dave Gamble

A permissão é concedida, gratuitamente, a qualquer pessoa que obtenha uma cópia
deste software e arquivos de documentação associados (o "Software"), para lidar
no Software sem restrição, incluindo, sem limitação, os direitos
usar, copiar, modificar, mesclar, publicar, distribuir, sublicenciar e / ou vender cópias
do Software, e para permitir que as pessoas a quem o Software é fornecido façam
portanto, sujeito às seguintes condições:

O aviso de direitos autorais acima e este aviso de permissão devem ser incluídos em todos
cópias ou partes substanciais do Software.

O SOFTWARE É FORNECIDO "COMO ESTÁ", SEM QUALQUER TIPO DE GARANTIA, EXPRESSA OU IMPLÍCITA,
INCLUINDO, MAS NÃO SE LIMITANDO ÀS GARANTIAS DE COMERCIALIZAÇÃO, ADEQUAÇÃO A UM ESPECÍFICO
OBJETIVO E NÃO VIOLAÇÃO. EM NENHUMA HIPÓTESE OS AUTORES OU TITULARES DOS DIREITOS AUTORAIS SERÃO
RESPONSÁVEL POR QUALQUER RECLAMAÇÃO, DANOS OU OUTRA RESPONSABILIDADE, SEJA EM UMA AÇÃO DE CONTRATO, DELITO
OU DE OUTRA FORMA, DECORRENTE DE, FORA DE OU EM CONEXÃO COM O SOFTWARE OU O USO OU
OUTRAS NEGOCIAÇÕES NO SOFTWARE.

@see https://github.com/DaveGamble/cJSON
*/

#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Renderize o cstring fornecido para uma versão com escape JSON que pode ser impressa.
 * @param insira o buffer de entrada a ser escapado.
 * @param output_buffer o buffer de saída para escrever. Você deve garantir que seja grande o suficiente para conter a string final.
 * @see cJSON equivlaent static cJSON_bool print_string_ptr (const unsigned char * const input, printbuffer * const output_buffer)
 */
bool json_print_string(const unsigned char *input, unsigned char *output_buffer);

#ifdef __cplusplus
}
#endif

#endif /* JSON_H_INCLUDED */
