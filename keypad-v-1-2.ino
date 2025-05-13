// Inclusão da biblioteca HID, que possibilita o envio de comandos de teclado.
#include <HID-Project.h>

// Defininção dos pinos da matriz de teclas.
#define ROWS 3
#define COLS 2
uint8_t rowPins[ROWS] = {15,14,16};
uint8_t colPins[COLS] = {9,10};

// "Debounce" ou "anti-repique" - supressão de ruido das teclas.
#define DEBOUNCE 100
unsigned long lastPress[ROWS][COLS];
bool keyState[ROWS][COLS] = {{1,1},{1,1},{1,1}};

// Definição dos pinos do encoder
#define encoderA 3
#define encoderB 2
volatile int encoderPos = 0;        // Posição atual do encoder.
volatile uint8_t prevState = 0;     // Posição anterior do encoder.
volatile int8_t pulseCount = 0;     // Quantia de pulsos contados, 2 pulsos equivalem a um clique do encoder, +2 == horário, -2 == anti-horário.
// Tabela verdade para validação de pulsos lidos, pulsos inválidos retornam 0, pulsos horários retornam 1, e pulsos anti-horários retornam -1.
const int8_t encoderTable[4][4] = {
    // Current State: 00  01  10  11
    /* Prev 00 */   {  0, -1,  1,  0 },
    /* Prev 01 */   {  1,  0,  0, -1 },
    /* Prev 10 */   { -1,  0,  0,  1 },
    /* Prev 11 */   {  0,  1, -1,  0 }
};

void encoderChange(){ // Função chamada pelos interrupts dos pinos encoderA e encoderB.
  // Ler estado atual do encoder
  bool stateA = digitalRead(encoderA);
  bool stateB = digitalRead(encoderB);
  uint8_t currentState = (stateA << 1) | stateB; // Salva o estado do encoder em binário (encoderA|encoderB), será dado à tabela verdade como posição 0, 1, 2 ou 3.
  if(currentState != prevState){ // Valida que o estado mudou, filtrando interrupts falsos.

    pulseCount += encoderTable[prevState][currentState]; // Ler tabela verdade usando os valores inteiros de 'currentState' e 'prevState', adiciona o valor da tabela a 'pulseCount'.

    if(currentState==0 || currentState==3){   // A cada clique,
      if(pulseCount>0){                         // se a rotação for horária,
        encoderPos++;                             // acrescente 1 a 'encoderPos'.
      }else if(pulseCount<0){                   // se a rotação for anti-horária,
        encoderPos--;                             // subtraia 1 de 'encoderPos'.
      }
      pulseCount=0;                           // Zerar 'pulseCount' para a próxima leitura
    }
    /* 'pulseCount' deve resultar em 2 na maioria das vezes, porque há 2 pulsos por clique, mas contar uma rotação toda vez que
    'pulseCount' termina diferente de 0 não tem nenhum efeito negativo, e deve ser mais robusto contra leituras incorretas. */
    
    prevState = currentState; // Atualiza o 'prevState' para a próxima leitura.
  }
}

// Enviar comandos do encoder.
void sendEncoder(){
  if(encoderPos>0){ // Horário:
    Consumer.write(MEDIA_VOLUME_UP); // Volume +
  }else{ // Anti-horário:
    Consumer.write(MEDIA_VOLUME_DOWN); // Volume -
  }
}

// Enviar comandos das teclas.
void sendKey(uint8_t row, uint8_t col) {
  /* "Bit-packing" é usado para otimizar comparações. Nesse caso o valor de 'row' é movido 4 bits à esquerda, e o valor de 'col' é
  inserido nos 4 bits à direita, resultando em um único valor de 8 bits, que pode ser comparado por um custo computacional menor. */
  switch ((row << 4) | col) {
    case (0 << 4) | 0: // Tecla 1:
      Consumer.write(HID_CONSUMER_MUTE); // Mutar áudio do computador
      break;
    case (0 << 4) | 1: // Tecla 2:
      Consumer.write(MEDIA_PLAY_PAUSE);
      break;
    case (1 << 4) | 0: // Tecla 3:
      Consumer.write(MEDIA_PREVIOUS); // Mídia anterior
      break;
    case (1 << 4) | 1: // Tecla 4:
      Consumer.write(MEDIA_NEXT); // Próxima mídia
      break;
    case (2 << 4) | 0: // Tecla 5:
        Keyboard.press(KEY_LEFT_GUI); // Tecla Windows
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_S);
        delay(10);               // Aguardar 10 milisegundos para garantir que o computador reconheça o clique.
        Keyboard.releaseAll();  // Soltar teclas.
      break;
    case (2 << 4) | 1: // Tecla 6:
        Keyboard.press(KEY_LEFT_CTRL);
        Keyboard.press(KEY_LEFT_SHIFT);
        Keyboard.press(KEY_M);
        delay(10);               // Aguardar 10 milisegundos para garantir que o computador reconheça o clique.
        Keyboard.releaseAll();  // Soltar teclas.
      break;
  }
}

// Inicialização do Arduino.
void setup() {
  // Inicialização dos pinos do encoder.
  pinMode(encoderA, INPUT_PULLUP);
  pinMode(encoderB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoderA), encoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB), encoderChange, CHANGE);

  // Inicialização da matriz de teclas.
  for (uint8_t i = 0; i < ROWS; i++){ // Todas as linas como saída VCC
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  for (uint8_t i = 0; i < COLS; i++){ // Todas as colunas como entrada PULLUP
    pinMode(colPins[i], INPUT_PULLUP);
  }

  // Inicialização de variaveis da matriz.
  for(uint8_t row = 0; row < ROWS; row++){    // Para cada linha:
    for(uint8_t col = 0; col < COLS; col++){    // Para cada coluna:
      lastPress[row][col] = 0;                    // Zerar tempo do ultimo clique
      keyState[row][col] = 1;
    }
  }

  Consumer.begin(); // Inicialização do protocolo HID.
  Keyboard.begin(); // Inicialização do protocolo de teclado.

}

// Código para ser repetido durante operação.
void loop() { 

  // Ler encoder.
  if(encoderPos != 0){
    sendEncoder();
    encoderPos = 0;
  }

  // Ler matriz.
  for(uint8_t row = 0; row < ROWS; row++){        // Para cada uma das linhas:
    digitalWrite(rowPins[row], LOW);                // Puxar a linha para GND.
    for(uint8_t col = 0; col < COLS; col++){          // Para cada coluna:
      bool rawRead = digitalRead(colPins[col]);         // Ler tecla.
      if(rawRead != keyState[row][col]){                // Se a tecla nao estava pressionada anteriormente:
        if(millis() - lastPress[row][col] > DEBOUNCE){    // Se a tecla nao foi pressionada há menos de DEBOUNCE milisseguindos:
          lastPress[row][col] = millis();                 // Salvar tempo do clique.
          keyState[row][col] = rawRead;                   // Atualizar estado atual da tecla.

          if(rawRead == 0){                               // Se o estado atual da tecla for "pressionada":
            sendKey(row, col);                              // Enviar comando.
          }
        }
      }
    }
    digitalWrite(rowPins[row], HIGH);                // Retornar a linha para VCC.
  }

}
