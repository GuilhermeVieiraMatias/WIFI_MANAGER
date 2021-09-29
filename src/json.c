/*
@file json.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"


bool json_print_string(const unsigned char *input, unsigned char *output_buffer)
{
	const unsigned char *input_pointer = NULL;
	unsigned char *output = NULL;
	unsigned char *output_pointer = NULL;
	size_t output_length = 0;
	/* número de caracteres adicionais necessários para escapar */
	size_t escape_characters = 0;

	if (output_buffer == NULL)
	{
		return false;
	}

	/* string vazia */
	if (input == NULL)
	{
		//output = ensure(output_buffer, sizeof("\"\""), hooks);
		if (output == NULL)
		{
			return false;
		}
		strcpy((char*)output, "\"\"");

		return true;
	}

	/* defina "flag" como 1 se algo precisar ser escapado */
	for (input_pointer = input; *input_pointer; input_pointer++)
	{
		if (strchr("\"\\\b\f\n\r\t", *input_pointer))
		{
			/* sequência de escape de um caractere */
			escape_characters++;
		}
		else if (*input_pointer < 32)
		{
			/* Sequência de escape UTF-16 uXXXX */
			escape_characters += 5;
		}
	}
	output_length = (size_t)(input_pointer - input) + escape_characters;

	/* no cJSON original é possível realocar aqui no caso do buffer de saída ser muito pequeno.
	 * Isso é um exagero para um sistema embarcado. */
	output = output_buffer;

	/* nenhum caractere precisa ser escapado */
	if (escape_characters == 0)
	{
		output[0] = '\"';
		memcpy(output + 1, input, output_length);
		output[output_length + 1] = '\"';
		output[output_length + 2] = '\0';

		return true;
	}

	output[0] = '\"';
	output_pointer = output + 1;
	/* copy the string */
	for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
	{
		if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
		{
			/* personagem normal, cópia */
			*output_pointer = *input_pointer;
		}
		else
		{
			/* personagem precisa ser escapado */
			*output_pointer++ = '\\';
			switch (*input_pointer)
			{
			case '\\':
				*output_pointer = '\\';
				break;
			case '\"':
				*output_pointer = '\"';
				break;
			case '\b':
				*output_pointer = 'b';
				break;
			case '\f':
				*output_pointer = 'f';
				break;
			case '\n':
				*output_pointer = 'n';
				break;
			case '\r':
				*output_pointer = 'r';
				break;
			case '\t':
				*output_pointer = 't';
				break;
			default:
				/* escapar e imprimir como ponto de código Unicode */
				sprintf((char*)output_pointer, "u%04x", *input_pointer);
				output_pointer += 4;
				break;
			}
		}
	}
	output[output_length + 1] = '\"';
	output[output_length + 2] = '\0';

	return true;
}

