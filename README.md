# Estação Meteorológica com Interface Web e Calibração Avançada
**Por: Leonardo Bonifácio Vieira Santos**

Este projeto utiliza o microcontrolador **RP2040 (Raspberry Pi Pico W)**, sensores digitais de umidade/temperatura (**AHT20**), pressão/temperatura (**BMP280**), LED RGB e matriz de LEDs para sinalização, além de um buzzer para alertas sonoros. Ele provê **monitoramento ambiental**, **calibração de sensores** e **interface web responsiva**.

---

## Funcionalidades Principais

### Leitura Contínua de Sensores
- Temperatura e umidade: AHT20 (I2C1)
- Temperatura e pressão: BMP280 (I2C0)


### Terminal Serial (Debug/Log)
- Dados contínuos e mensagens de depuração.

### Interface Web (HTTP Server)
- Servidor web embarcado na Raspberry Pi Pico W.
- **Página de Gráficos**: dados em tempo real com gráficos interativos (atualização a cada 2 segundos).
- **Página de Configuração de Limites**: formulário para definir limites de temperatura, umidade e pressão.
- **Formulário de Offsets de Calibração**: permite aplicar offsets diretamente pela interface.
- Comunicação via AJAX (JSON).

### Alertas Visuais e Sonoros
- **LED RGB**: status geral do sistema e alertas.
- **Matriz de LEDs**: visualização de alertas específicos.
- **Buzzer**: alarmes sonoros para limites excedidos ou erro de leitura.

### Controle por Botões Físicos (BitDogLab)
- **Botão A (GPIO 5)**: alterna entre páginas da interface web.
- **Botão B (GPIO 6)**: ativa/desativa alertas visuais e sonoros.

---

## Hardware Necessário

- Raspberry Pi Pico W (RP2040 com Wi-Fi)
- Sensor AHT20 — I2C
- Sensor BMP280 — I2C
- LED RGB (integrado na BitDogLab)
- Matriz de LEDs (integrado na BitDogLab)
- Buzzer (integrado na BitDogLab)
- Cabos de conexão

---

## Conexões (GPIO)

| Dispositivo         | Barramento | SDA | SCL | Endereço I2C | Observações                                        |
|---------------------|------------|-----|-----|----------------|----------------------------------------------------|
| AHT20               | i2c1       | 2   | 3   | 0x38          | Conectado na I2C1                                 |
| BMP280              | i2c0       | 0   | 1   | 0x76 ou 0x77  | Conectado na I2C0                                 |
| Display SSD1306     | i2c1       | 14  | 15  | 0x3C          | Conectado na I2C1                                 |
| Botão A             | -          | 5   | -   | -             | Alterna a página web                              |
| Botão B (BOOTSEL)   | -          | 6   | -   | -             | Entra no modo de gravação                         |
| Buzzer              | -          | 21 | - | -             | Acionado por alertas                              |
| LED RGB             | -          | 11 12 13 | - | -             | Sinalização de status/alertas                     |
| Matriz de LEDs      | -          | 7 | - | -             | Sinalização visual de alertas                     |

---

## Como Funciona

### Inicialização
- Configura I2C, LEDs, buzzer, matriz de LEDs e botões físicos.

### Conexão Wi-Fi
- Conecta-se usando `WIFI_SSID` e `WIFI_PASSWORD`.
- LED RGB indica status da conexão.

### Servidor Web
- HTTP Server na porta 80.
- Página de gráficos servida por padrão.
- Botão A alterna entre páginas HTML (gráficos e configuração).

### Leitura e Processamento de Dados
- Leitura contínua dos sensores AHT20 e BMP280.
- Aplicação de offsets definidos via web.
- Envio de dados corrigidos para o terminal serial.


### Sinalização de Alertas
- Comparação com limites definidos.
- Se alertas estiverem ativados, LEDs e buzzer são acionados.

### Comunicação Web (AJAX)
- GET `/system_state` a cada 2s retorna dados atualizados.
- POST `/set_offsets` e `/set_limits` recebem dados enviados pelo usuário.

---

## Dependências e Compilação

### SDK e Bibliotecas
- **Raspberry Pi Pico SDK**:  
  https://github.com/raspberrypi/pico-sdk

### Bibliotecas customizadas (na pasta `lib/` do projeto):
- `aht20.h` — Driver AHT20
- `bmp280.h` — Driver BMP280
- `matriz.h` — Matriz de LEDs
- `led.h` — LED RGB
- `buzzer.h` — Buzzer
- `index_html.h` — Página principal (gráficos e offsets)
- `html_limits_config.h` — Página de limites

> Recomendado: utilize o **VS Code** com a extensão oficial do Raspberry Pi Pico.

---

## Exemplo de Uso

1. Compile e grave o firmware no Raspberry Pi Pico W.
2. Conecte sensores e display conforme a tabela de GPIO.
3. Acesse o terminal serial para depuração.
4. No navegador, acesse o IP exibido após conexão Wi-Fi.
5. Visualize os gráficos em tempo real.
6. Pressione **Botão A (GPIO 5)** e recarregue a página para acessar configurações de limite.
7. Pressione **Botão B (GPIO 6)** para ativar/desativar os alertas.
8. Use o formulário de "Ajuste de Offsets" para calibrar sensores.
9. Use a página de "Configuração de Limites" para definir faixas válidas.

---

## Observações

- Offsets e limites **não são persistentes**. Serão perdidos após reinício.
- Para persistência, utilize armazenamento em Flash (`pico_nvm`, `pico_unique_id`, etc).

---
# meterelogicaInterfaceWeb
