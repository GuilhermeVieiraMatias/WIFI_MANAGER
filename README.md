# O que é esp32-wifi-manager?

### Build status [![Build Status](https://travis-ci.com/tonyp7/esp32-wifi-manager.svg?branch=master)](https://travis-ci.com/tonyp7/esp32-wifi-manager)

*esp32-wifi-manager* é um componente C esp-idf puro para ESP32 que permite fácil gerenciamento de redes wi-fi por meio de um portal da web.

*esp32-wifi-manager* é um scanner wi-fi completo, servidor http e daemon dns vivendo na menor quantidade de RAM possível.

*esp32-wifi-manager* tentará se reconectar automaticamente a uma rede salva anteriormente na inicialização e, se não conseguir encontrar um wi-fi salvo, iniciará seu próprio ponto de acesso por meio do qual você pode gerenciar e se conectar a redes wi-fi. Após uma conexão bem-sucedida, o software desligará o ponto de acesso automaticamente após algum tempo (1 minuto por padrão).

*esp32-wifi-manager* compila com esp-idf 4.2 e superior. Ver [Getting Started](#getting-started) para guiá-lo em sua primeira configuração.

# Content
 - [Demo](#demo)
 - [Look And Feel](#look-and-feel)
 - [Getting Started](#getting-started)
   - [Requirements](#requirements)
   - [Hello World](#hello-world)
   - [Configuring the Wifi Manager](#configuring-the-wifi-manager)
 - [Adding esp32-wifi-manager to your code](#adding-esp32-wifi-manager-to-your-code)
   - [Interacting with the manager](#interacting-with-the-manager)
   - [Interacting with the http server](#interacting-with-the-http-server)
   - [Thread safety and access to NVS](#thread-safety-and-access-to-nvs)
 - [License](#license)
   

# Demo
[![esp32-wifi-manager demo](http://img.youtube.com/vi/hxlZi15bym4/0.jpg)](http://www.youtube.com/watch?v=hxlZi15bym4)

# Look and Feel
![esp32-wifi-manager on an mobile device](https://idyl.io/wp-content/uploads/2017/11/esp32-wifi-manager-password.png "esp32-wifi-manager") ![esp32-wifi-manager on an mobile device](https://idyl.io/wp-content/uploads/2017/11/esp32-wifi-manager-connected-to.png "esp32-wifi-manager")

# Getting Started

## Requirements

435 / 5000
Resultados da tradução
Para começar, o esp32-wifi-manager precisa de:

- esp-idf ** 4.2 e superior **
- esp32 ou esp32-s2

Existem mudanças importantes e novos recursos no esp-idf 4.1 e 4.2 que tornam o esp32-wifi-manager incompatível com qualquer item inferior a 4.2. Isso inclui esp_netif (introduzido em 4.1) e esp_event_handler_instance_t (introduzido em 4.2). Recomenda-se compilar esp32-wifi-manager com a árvore mestre para evitar qualquer problema de compatibilidade. 

## Hello World

Clone o repositório onde você quiser. Se você não está familiarizado com o Git, pode usar o Github Desktop no Windows:

```bash 
git clone https://github.com/tonyp7/esp32-wifi-manager.git
```

Navegue sob o exemplo incluído:

```bash
cd esp32-wifi-manager/examples/default_demo
```

Compile o código e carregue-o em seu esp32:

```bash
idf.py build flash monitor
```

_Note: embora seja encorajado a usar o sistema de compilação mais recente com idf.py e cmake, esp32-wifi-manager ainda suporta o sistema de compilação legado. Se você estiver usando make no Linux ou make usando MSYS2 no Windows, você ainda pode usar "make build flash monitor" se preferir_

Agora, usando qualquer dispositivo compatível com wi-fi, você verá um novo ponto de acesso wi-fi chamado * esp32 *. Conecte-se a ele usando a senha padrão * esp32pwd *. Se o portal cativo não aparecer no seu dispositivo, você pode acessar o gerenciador wi-fi em seu endereço IP padrão: http://10.10.0.1. 

## Configuring the Wifi Manager

O esp32-wifi-manager pode ser configurado sem alterar seu código. No nível do projeto, use:

```bash
idf.py menuconfig
```

Navegue em "Configuração do componente" e selecione "Configuração do Wifi Manager". Você será saudado pela seguinte tela:

![esp32-wifi-manager-menuconfig](https://idyl.io/wp-content/uploads/2020/08/wifi-manager-menuconfig-800px.png "menuconfig screen")

Você pode alterar o SSID e a senha do ponto de acesso conforme sua conveniência, mas é altamente recomendável manter os valores padrão. Sua senha deve ter entre 8 e 63 caracteres, para cumprir o padrão WPA2. Se a senha for definida com um valor vazio ou tiver menos de 8 caracteres, esp32-wifi-manager criará seu ponto de acesso como uma rede wi-fi aberta.

Você também pode alterar os valores de vários temporizadores, por exemplo, quanto tempo leva para o ponto de acesso desligar quando a conexão é estabelecida (padrão: 60000). Embora possa ser tentador definir este temporizador para 0, apenas esteja avisado que, nesse caso, o usuário nunca obterá o feedback de que uma conexão foi bem-sucedida. Desligar o AP matará instantaneamente a sessão de navegação atual no portal cativo.

Finalmente, você pode escolher realocar esp32-wifi-manager para um URL diferente, alterando o valor padrão de "/" para algo diferente, por exemplo "/wifimanager/". Observe que a barra final é importante. Este recurso é particularmente útil no caso de você desejar que seu próprio webapp coexista com as próprias páginas da web do esp32-wifi-manager.

# Adding esp32-wifi-manager to your code

Para usar o esp32-wifi-manager efetivamente em seus projetos esp-idf, copie todo o repositório esp32-wifi-manager (ou clone do git) em uma subpasta de componentes.

Seu projeto deve ser assim: 

  - project_folder
    - build
    - components
      - esp32-wifi-manager
    - main
      - main.c

Sob o eclipse, é assim que se parece um projeto típico: 

![eclipse project with esp32-wifi-manager](https://idyl.io/wp-content/uploads/2020/07/eclipse-idf-project.png "eclipse project with esp32-wifi-manager")

Feito isso, você precisa editar o arquivo CMakeLists.txt na raiz do seu projeto para registrar a pasta de componentes. Isso é feito adicionando a seguinte linha:

```cmake
set(EXTRA_COMPONENTS_DIRS components/)
```

Um arquivo CmakeLists.txt típico deve ter a seguinte aparência:

```cmake
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(name_of_your_project)
```

Se você estiver usando o sistema de compilação antigo com make, você deve editar o Makefile como:

```make
PROJECT_NAME := name_of_your_project
EXTRA_COMPONENT_DIRS := components/
include $(IDF_PATH)/make/project.mk
```

Depois de fazer isso, você pode agora, em seu código de usuário, adicionar o cabeçalho:

```c
#include "wifi_manager.h"
```

Tudo que você precisa fazer agora é chamar wifi_manager_start(); em seu código. Ver [examples/default_demo](examples/default_demo) se você estiver incerto.


## Interacting with the manager

Existem três maneiras diferentes de incorporar o esp32-wifi-manager ao seu código:
* Basta esquecer e pesquisar em seu código o status de conectividade wi-fi
* Use event callbacks
* Modifique o código do gerenciador de wi-fi esp32 diretamente para atender às suas necessidades

** Event callbacks ** são a maneira mais limpa de usar o gerenciador de wi-fi e essa é a forma recomendada de fazer isso. Um caso de uso típico seria ser notificado quando o gerenciador de wi-fi finalmente obtiver uma conexão com um ponto de acesso. Para fazer isso, você pode simplesmente definir uma função de retorno de chamada:

```c
void cb_connection_ok(void *pvParameter){
	ESP_LOGI(TAG, "I have a connection!");
}
```

Em seguida, basta registrá-lo chamando:

```c
wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
```

É isso! Agora, toda vez que o evento for disparado, ele chamará esta função. O [examples/default_demo](examples/default_demo) contém código de amostra usando retornos de chamada.

### List of events

A lista de eventos possíveis aos quais você pode adicionar um retorno de chamada são definidos por message_code_t em wifi_manager.h. Eles são os seguintes: 

* WM_ORDER_START_HTTP_SERVER
* WM_ORDER_STOP_HTTP_SERVER
* WM_ORDER_START_DNS_SERVICE
* WM_ORDER_STOP_DNS_SERVICE
* WM_ORDER_START_WIFI_SCAN
* WM_ORDER_LOAD_AND_RESTORE_STA
* WM_ORDER_CONNECT_STA
* WM_ORDER_DISCONNECT_STA
* WM_ORDER_START_AP
* WM_EVENT_STA_DISCONNECTED
* WM_EVENT_SCAN_DONE
* WM_EVENT_STA_GOT_IP
* WM_ORDER_STOP_AP

Na prática, acompanhar WM_EVENT_STA_GOT_IP e WM_EVENT_STA_DISCONNECTED é a chave para saber se o esp32 tem uma conexão ou não. As outras mensagens podem ser ignoradas principalmente em um aplicativo típico usando esp32-wifi-manager.

### Events parameters

A assinatura de retorno de chamada inclui um ponteiro void *. Para a maioria dos eventos, este parâmetro adicional está vazio e é enviado como um valor NULL. Alguns eventos selecionados possuem dados adicionais que podem ser aproveitados pelo código do usuário. Eles estão listados abaixo:

* WM_EVENT_SCAN_DONE is sent with a wifi_event_sta_scan_done_t* object.
* WM_EVENT_STA_DISCONNECTED is sent with a wifi_event_sta_disconnected_t* object.
* WM_EVENT_STA_GOT_IP is sent with a ip_event_got_ip_t* object.

Esses objetos são estruturas esp-idf padrão e são documentados como tal nas [official pages](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html).

O [examples/default_demo](examples/default_demo) demonstra como você pode ler um objeto ip_event_got_ip_t para acessar o endereço IP atribuído ao esp32.

## Interacting with the http server

Como o esp32-wifi-manager gera seu próprio servidor http, você pode querer estender esse servidor para servir suas próprias páginas em seu aplicativo. É possível fazer isso registrando seu próprio manipulador de URL usando a assinatura padrão esp_http_server:

```c
esp_err_t my_custom_handler(httpd_req_t *req){
```

E então registrar o manipulador fazendo

```c
http_app_set_handler_hook(HTTP_GET, &my_custom_handler);
```

O [examples/http_hook](examples/http_hook) contém um exemplo onde uma página da web é registrada em /helloworld

## Thread safety and access to NVS

esp32-wifi-manager acessa o armazenamento não volátil para armazenar e carregar sua configuração em um namespace dedicado "espwifimgr". Se quiser ter certeza de que nunca haverá um conflito com o acesso simultâneo ao NVS, você pode incluir nvs_sync.h e usar chamadas para nvs_sync_lock e nvs_sync_unlock.

```c
nvs_handle handle;

if(nvs_sync_lock( portMAX_DELAY )){  
    if(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle) == ESP_OK){
        /* do something with NVS */
	nvs_close(handle);
    }
    nvs_sync_unlock();
}
```
nvs_sync_lock aguarda o número de ticks enviados a ele como um parâmetro para adquirir um mutex. É recomendado usar portMAX_DELAY. Na prática, nvs_sync_lock quase nunca espera.


# License
*esp32-wifi-manager* é licenciado pelo MIT. Como tal, pode ser incluído em qualquer projeto, comercial ou não, desde que você mantenha os direitos autorais originais. Certifique-se de ler o arquivo de licença.