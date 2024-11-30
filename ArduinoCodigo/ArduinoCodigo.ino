/**
* @file ArduinoProyecto.ino
* @brief Implementación del sistema de control ambiental y de eventos.
*
* Este archivo contiene el código fuente para el sistema de control que 
* gestiona sensores ambientales, eventos de seguridad, y la interacción 
* con los usuarios mediante un teclado matricial y una pantalla LCD. 
* El sistema permite la lectura de temperatura, luz, y humedad, además 
* de manejar estados como alerta y alarma en respuesta a condiciones específicas.
*
* @details
* Las principales funcionalidades del sistema incluyen:
* - Lectura de valores de sensores (temperatura, luz, humedad).
* - Actualización de las pantallas LCD para mostrar datos en tiempo real.
* - Control de estados mediante una máquina de estados.
* - Gestión de alarmas y alertas basadas en eventos de seguridad.
* - Interacción del usuario mediante un teclado matricial para validar accesos.
*
* @author Paula Andrea Muñoz Delgado
* @author Juan Diego Perez Martinez
* @author Ana Sofia Arango Yanza
*
* @date 29/11/2024
*/

// Inclusión de bibliotecas necesarias.

#include <Keypad.h>                 
#include <LiquidCrystal.h>          
#include "StateMachineLib.h"        
#include "AsyncTaskLib.h"           
#include "DHT.h"                    

// Definición de pines y constantes del sistema.

#define PIN_TEMP A0                 /**< Pin para el sensor de temperatura */
#define PIN_PHOTOCELL A1            /**< Pin para la fotocelda */
#define PIN_IR A2                   /**< Pin para el sensor de infrarrojos */
#define PIN_SH A3                   /**< Pin para el sensor de efecto Hall */
#define DHTPIN A4                   /**< Pin para el sensor DHT11 */
#define PIN_RED 46                  /**< Pin para el LED rojo */
#define PIN_BLUE 42                 /**< Pin para el LED azul */
#define PIN_GREEN 44                /**< Pin para el LED verde */
#define PIN_BUZZER 48               /**< Pin para el buzzer */

#define DHTTYPE DHT11               /**< Tipo de sensor DHT */
#define TH_TEMP_HIGH 32.0           /**< Límite superior de temperatura */
#define BETA 4090                   /**< Constante beta para cálculo de la temperatura */

// Configuración de hardware y periféricos del sistema.

DHT dht(DHTPIN, DHTTYPE);                   /**< Objeto para el sensor DHT11 */

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);      /**< Configuración de pines para el LCD */

const byte ROWS = 4;                        /**< Número de filas del teclado matricial */
const byte COLS = 4;                        /**< Número de columnas del teclado matricial */

char keys[ROWS][COLS] = {                   /**< Matriz de teclas para el teclado */
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {24, 26, 28, 30};      /**< Pines de las filas del teclado */
byte colPins[COLS] = {32, 34, 36, 38};      /**< Pines de las columnas del teclado */

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);   /**< Objeto para manejar el teclado matricial */

// Variables globales utilizadas para la gestión del sistema.

const char password[6] = "12345";           /**< Contraseña del sistema */
char entrada[6];                            /**< Entrada del usuario para comparación con la contraseña */
byte digitosEntrada = 0;                    /**< Número de dígitos ingresados por el usuario */
byte intentosSeguridad = 0;                 /**< Contador de intentos fallidos de contraseña */

float valorTemperatura = 0.0;               /**< Valor de temperatura leído del sensor */
int valorLuz = 0;                           /**< Valor de luz ambiental leído del fototransistor */
float valorHumedad = 0.0;                   /**< Valor de humedad leído del sensor DHT11 */
byte intentosSensores = 0;                  /**< Contador de intentos fallidos de lectura de sensores */

bool claveIncorrecta = false;               /**< Indicador de si la clave ingresada es incorrecta */
bool claveCorrecta = false;                 /**< Indicador de si la clave ingresada es correcta */

// Declaración de funciones principales del sistema.

/**
* @brief Lee la temperatura y la luz ambiental
*/
void lecturaAmbiental();

/**
* @brief Lee los valores de humedad del sensor DHT
*/
void lecturaHumedad();

/**
* @brief Actualiza la información mostrada en el LCD ambiental
*/
void actualizarDisplayAmbiental();

/**
* @brief Actualiza la información mostrada en el LCD eventos
*/
void actualizarDisplayEventos();

/**
* @brief Controla el tiempo y los eventos relacionados
*/
void controlTiempo();

/**
* @brief Controla la entrada del sistema
*/
void controlEntrada();

// Tareas asíncronas para el control del sistema.

/// @brief Tareas asíncronas para el control del sistema.
AsyncTask taskControlAmbiental(200, true, lecturaAmbiental);            ///< Tarea para control temperatura y luz
AsyncTask taskControlHumedad(1, true, lecturaHumedad);                  ///< Tarea para control de humedad
AsyncTask taskControlTiempo(100, false, controlTiempo);                 ///< Tarea para control del tiempo
AsyncTask taskControlTiempoEntrada(5000, false, controlEntrada);        ///< Tarea para control de tiempo de entrada
AsyncTask taskDisplayAmbiental(1000, true, actualizarDisplayAmbiental); ///< Tarea para actualizar el display ambiental
AsyncTask taskDisplayEventos(100, true, actualizarDisplayEventos);      ///< Tarea para actualizar el display de eventos

/// @brief Tareas asíncronas para controlar los LEDs.
AsyncTask taskActivarLedRed500(500, []() { digitalWrite(PIN_RED, HIGH); });
AsyncTask taskDesactivarLedRed500(500, []() { digitalWrite(PIN_RED, LOW); });
AsyncTask taskActivarLedRed150(150, []() { digitalWrite(PIN_RED, HIGH); });
AsyncTask taskDesactivarLedRed150(150, []() { digitalWrite(PIN_RED, LOW); });
AsyncTask taskActivarLedBlue(800, []() { digitalWrite(PIN_BLUE, HIGH); });
AsyncTask taskDesactivarLedBlue(800, []() { digitalWrite(PIN_BLUE, LOW); });
AsyncTask taskActivarLedBlue300(300, []() { digitalWrite(PIN_BLUE, HIGH); });
AsyncTask taskDesactivarLedBlue300(300, []() { digitalWrite(PIN_BLUE, LOW); });
AsyncTask taskActivarLedGreen(800, []() { digitalWrite(PIN_GREEN, HIGH); });
AsyncTask taskDesactivarLedGreen(800, []() { digitalWrite(PIN_GREEN, LOW); });

/// @brief Tareas asíncronas para generar sonido en diferentes eventos.
AsyncTask taskSonidoBloqueo(500, []() { tone(PIN_BUZZER, 440); });  ///< Sonido para bloqueo
AsyncTask taskNoToneBloqueo(500, []() { noTone(PIN_BUZZER); });     ///< Silenciar tono de bloqueo
AsyncTask taskSonidoAlerta(800, []() { tone(PIN_BUZZER, 660); });   ///< Sonido para alerta
AsyncTask taskNoToneAlerta(800, []() { noTone(PIN_BUZZER); });      ///< Silenciar tono de alerta
AsyncTask taskSonidoAlarma(150, []() { tone(PIN_BUZZER, 880); });   ///< Sonido para alarma
AsyncTask taskNoToneAlarma(150, []() { noTone(PIN_BUZZER); });      ///< Silenciar tono de alarma

/**
* @brief Definición de estados y entradas para la máquina de estados.
*
* Este bloque define los estados de la máquina de estados que gestiona el flujo de 
* control del sistema, así como las entradas posibles que determinan las transiciones 
* entre estos estados. Cada estado corresponde a una fase del sistema, como control 
* ambiental, manejo de eventos, alerta o bloqueo, y cada entrada puede activar una 
* transición a un estado diferente.
*/

/// @brief Estados de la máquina de estados
enum State {
  INICIO = 0,         ///< Estado inicial del sistema
  AMBIENTAL = 1,      ///< Estado para control ambiental
  BLOQUEADO = 2,      ///< Estado bloqueado
  EVENTOS = 3,        ///< Estado de manejo de eventos
  ALERTA = 4,         ///< Estado de alerta
  ALARMA = 5          ///< Estado de alarma
};

/// @brief Entradas posibles para la máquina de estados
enum Input {
  Sign_Tiempo = 0,    ///< Señal relacionada con el tiempo
  Sign_General = 1,   ///< Señal general
  Sign_Block = 2,     ///< Señal para bloqueo
  Sign_Key = 3,       ///< Señal de tecla
  Unknow = 4          ///< Entrada desconocida
};

/// @brief Máquina de estados para manejar las transiciones del sistema
StateMachine stateMachine(6, 16);

/// @brief Entrada actual para la máquina de estados
Input input;

/// @brief Entrada actual inicializada como desconocida
Input currentInput = Input::Unknow;

/**
* @brief Configura las transiciones y acciones de la máquina de estados
* 
* Esta función agrega las transiciones entre estados y las acciones a realizar
* al entrar o salir de un estado específico
*/
void setupStateMachine()
{
	// Add transitions
	stateMachine.AddTransition(INICIO, BLOQUEADO, []() { return input == Sign_Block; });
  stateMachine.AddTransition(INICIO, AMBIENTAL, []() { return input == Sign_General; });
	stateMachine.AddTransition(INICIO, INICIO, []() { return input == Unknow; });
 
  stateMachine.AddTransition(AMBIENTAL, EVENTOS, []() { return input == Sign_Tiempo; });
	stateMachine.AddTransition(AMBIENTAL, ALARMA, []() { return input == Sign_General; });
	stateMachine.AddTransition(AMBIENTAL, AMBIENTAL, []() { return input == Unknow; });

  stateMachine.AddTransition(EVENTOS, AMBIENTAL, []() { return input == Sign_Tiempo; });
	stateMachine.AddTransition(EVENTOS, ALERTA, []() { return input == Sign_General; });
  stateMachine.AddTransition(EVENTOS, EVENTOS, []() { return input == Unknow; });

	stateMachine.AddTransition(ALARMA, INICIO, []() { return input == Sign_Key; });
  stateMachine.AddTransition(ALARMA, ALARMA, []() { return input == Unknow; });

  stateMachine.AddTransition(ALERTA, EVENTOS, []() { return input == Sign_Tiempo; });
	stateMachine.AddTransition(ALERTA, ALARMA, []() { return input == Sign_Block; });
  stateMachine.AddTransition(ALERTA, ALERTA, []() { return input == Unknow; });

  stateMachine.AddTransition(BLOQUEADO, INICIO, []() { return input == Sign_Tiempo; });
  stateMachine.AddTransition(BLOQUEADO, BLOQUEADO, []() { return input == Unknow; });

	// Add actions
	stateMachine.SetOnEntering(INICIO, funct_Init_Inicio);
	stateMachine.SetOnEntering(AMBIENTAL, funct_Init_Ambiental);
	stateMachine.SetOnEntering(BLOQUEADO, funct_Init_Bloqueado);
	stateMachine.SetOnEntering(EVENTOS, funct_Init_Eventos);
  stateMachine.SetOnEntering(ALERTA, funct_Init_Alerta);
  stateMachine.SetOnEntering(ALARMA, funct_Init_Alarma);

	stateMachine.SetOnLeaving(INICIO, funct_Fin_Inicio);
	stateMachine.SetOnLeaving(AMBIENTAL, funct_Fin_Ambiental);
	stateMachine.SetOnLeaving(BLOQUEADO, funct_Fin_Bloqueado);
	stateMachine.SetOnLeaving(EVENTOS, funct_Fin_Eventos);
  stateMachine.SetOnLeaving(ALERTA, funct_Fin_Alerta);
  stateMachine.SetOnLeaving(ALARMA, funct_Fin_Alarma);
}

/**
* @brief Configura los pines, inicializa dispositivos y establece el estado inicial de la máquina de estados
* 
* Configura los pines de entrada y salida, inicializa la comunicación serial, 
* el display LCD y el sensor DHT. También prepara la máquina de estados para 
* comenzar en el estado `INICIO` y arranca la tarea de control de tiempo de entrada
*/
void setup() 
{
  pinMode(PIN_RED, OUTPUT);                   ///< Configura el pin del LED rojo como salida
  pinMode(PIN_BLUE, OUTPUT);                  ///< Configura el pin del LED azul como salida
  pinMode(PIN_GREEN, OUTPUT);                 ///< Configura el pin del LED verde como salida
  pinMode(PIN_BUZZER, OUTPUT);                ///< Configura el pin del buzzer como salida
  pinMode(PIN_IR, INPUT);                     ///< Configura el pin del sensor infrarrojo como entrada
  pinMode(PIN_SH, INPUT);                     ///< Configura el pin del sensor Hall como entrada

  Serial.begin(9600);                         ///< Inicializa la comunicación serial a 9600 bps
  lcd.begin(16, 2);                           ///< Configura el display LCD con 16 columnas y 2 filas
  dht.begin();                                ///< Inicializa el sensor DHT

  setupStateMachine();                        ///< Configura las transiciones y acciones de la máquina de estados
  stateMachine.SetState(INICIO, false, true); ///< Establece el estado inicial como INICIO
}

/**
* @brief Bucle principal del programa que maneja la entrada, la máquina de estados y las tareas asíncronas
* 
* En cada iteración:
* - Se lee la entrada del sistema y se actualiza la máquina de estados
* - Se gestionan las tareas asíncronas, incluyendo el control de LEDs, el sensor ambiental, 
*   los displays y el buzzer
*/
void loop() 
{
  input = static_cast<Input>(readInput());                /// Convierte la entrada leída al tipo Input
  stateMachine.Update();                                  /// Actualiza el estado de la máquina de estados basado en la entrada

  // Actualización de tareas asíncronas de LEDs
  taskActivarLedRed500.Update(taskDesactivarLedRed500);   /// Controla el encendido del LED rojo a 500 ms
  taskDesactivarLedRed500.Update(taskActivarLedRed500);   /// Controla el apagado del LED rojo a 500 ms
  taskActivarLedRed150.Update(taskDesactivarLedRed150);   /// Controla el encendido del LED rojo a 150 ms
  taskDesactivarLedRed150.Update(taskActivarLedRed150);   /// Controla el apagado del LED rojo a 150 ms
  taskActivarLedBlue.Update(taskDesactivarLedBlue);       /// Controla el encendido del LED azul a 800 ms
  taskDesactivarLedBlue.Update(taskActivarLedBlue);       /// Controla el apagado del LED azul a 800 ms
  taskActivarLedBlue300.Update(taskDesactivarLedBlue300); /// Controla el encendido del LED azul a 300 ms
  taskDesactivarLedBlue300.Update(taskActivarLedBlue300); /// Controla el apagado del LED azul a 300 ms
  taskActivarLedGreen.Update(taskDesactivarLedGreen);     /// Controla el encendido del LED verde a 800 ms
  taskDesactivarLedGreen.Update(taskActivarLedGreen);     /// Controla el apagado del LED verde a 800 ms

  // Actualización de tareas relacionadas con sensores y displays
  taskControlAmbiental.Update();                          /// Actualiza la lectura del sensor ambiental
  taskControlHumedad.Update();                            /// Actualiza la lectura del sensor de humedad
  taskDisplayAmbiental.Update();                          /// Actualiza el display ambiental
  taskDisplayEventos.Update();                            /// Actualiza el display de eventos

  // Actualización de tareas relacionadas con el buzzer
  taskSonidoBloqueo.Update(taskNoToneBloqueo);            /// Activa el tono de bloqueo
  taskNoToneBloqueo.Update(taskSonidoBloqueo);            /// Desactiva el tono de bloqueo
  taskSonidoAlerta.Update(taskNoToneAlerta);              /// Activa el tono de alerta
  taskNoToneAlerta.Update(taskSonidoAlerta);              /// Desactiva el tono de alerta
  taskSonidoAlarma.Update(taskNoToneAlarma);              /// Activa el tono de alarma
  taskNoToneAlarma.Update(taskSonidoAlarma);              /// Desactiva el tono de alarma

  // Actualización de tareas de control de tiempo
  taskControlTiempo.Update();                             /// Actualiza la tarea de control del tiempo
  taskControlTiempoEntrada.Update();                      /// Actualiza la tarea de control del tiempo de entrada

}

/**
* @brief Lee la entrada actual del sistema
* 
* Esta función retorna el valor de la entrada actual que está siendo procesada por el sistema
* y se usa para actualizar el estado de la máquina de estados
* 
* @return int El valor de la entrada actual, representada como un entero
*/
int readInput()
{
  return currentInput; // Retorna el valor de la entrada actual
}


/**
* @brief Inicializa el proceso de autenticación del usuario en el sistema
* 
* Esta función gestiona la entrada de la clave del usuario a través de un teclado
* Si la clave es correcta se otorga acceso al sistema, activando las respectivas tareas y mostrando un mensaje en la pantalla LCD
* Si la clave es incorrecta, se muestra un mensaje de error y se incrementan los intentos de seguridad
* Si el número de intentos de seguridad llega a 3, se bloquea el acceso temporalmente
*
* @return void
* 
* @see taskControlTiempoEntrada Para el control del tiempo de entrada
* @see taskControlTiempo Para el control del tiempo entre intentos
* @see taskActivarLedGreen Para la activación del LED verde al acceder
* @see taskActivarLedBlue300 Para la activación del LED azul al fallar la clave
* @see Input Para los valores de entrada procesados por la máquina de estados
*/
void funct_Init_Inicio(void) {
  // Si la clave ya ha sido ingresada correctamente o incorrectamente, no se hace nada
  if (claveIncorrecta || claveCorrecta) {
    return; 
  }

  // Se prepara la pantalla LCD para mostrar el mensaje 
  lcd.setCursor(0, 0);
  lcd.print("Clave: ");

  // Se obtiene la tecla presionada en el teclado
  char key = keypad.getKey();

  // Si se presionó una tecla y los intentos de seguridad son menos de 3, se procesa la clave
  if (key && intentosSeguridad < 3) {
    taskControlTiempoEntrada.Stop();
    // Almacena la tecla presionada en el arreglo de entrada
    entrada[digitosEntrada] = key;
    // Muestra un '*' en la pantalla LCD para cada carácter de la clave ingresada
    lcd.setCursor(digitosEntrada, 1);
    lcd.print('*');

    // Incrementa el índice de la entrada para la siguiente tecla
    digitosEntrada++;
    
    // Si se han ingresado 5 caracteres (longitud de la clave), se valida la clave
    if (digitosEntrada == 5) {
      entrada[5] = '\0';  // Se asegura de que la cadena de entrada termine con un '\0'

      // Si la clave ingresada es correcta, se da acceso al sistema
      if (strcmp(entrada, password) == 0) {
        claveCorrecta = true; // Se marca la clave como correcta
        
        // Se configura el temporizador para que se ejecute durante 3 segundos
        taskControlTiempo.SetIntervalMillis(3000); 
        taskControlTiempo.Start();

        // Se enciende el LED verde y se muestra el mensaje de acceso concedido
        taskActivarLedGreen.Start();
        lcd.clear();
        lcd.setCursor(3, 0);
        lcd.print("ACCEDIENDO");
        lcd.setCursor(3, 1);
        lcd.print("AL SISTEMA");

        // Se cambia el estado de entrada para que no se procese más
        currentInput = Input::Unknow;
      } 
      else {
        // Si la clave es incorrecta, se limpia la entrada y se incrementan los intentos de seguridad
        limpiarEntrada();
        lcd.clear();
        intentosSeguridad++;

        // Si se alcanzan los 3 intentos incorrectos, se bloquea el acceso temporalmente
        if (intentosSeguridad >= 3) {
          currentInput = Input::Sign_Block;
        }
        else {
          // Si la clave es incorrecta, se muestra un mensaje de error y se activa el LED azul
          claveIncorrecta = true;
          taskControlTiempo.SetIntervalMillis(3000); // Intervalo para el temporizador de error
          taskControlTiempo.Start();
          taskActivarLedBlue300.Start(); // Activa el LED azul durante 300ms
          lcd.setCursor(5, 0);
          lcd.print("CLAVE");
          lcd.setCursor(3, 1);
          lcd.print("INCORRECTA");

          // Se restaura el estado de entrada
          currentInput = Input::Unknow;
        }
      }
    }
    else {
      currentInput = Input::Unknow;
      taskControlTiempoEntrada.Start();
    }
  }
}


/**
* @brief Finaliza el proceso de inicio dependiendo del estado de la máquina de estados.
* 
* Esta función ejecuta diferentes tareas en función del valor de `currentInput`:
* - Si el estado es `Sign_General`, se activan tareas relacionadas con el control ambiental, la humedad y la visualización.
* - Si el estado es `Sign_Block`, se activa un temporizador, un LED rojo y un sonido de bloqueo.
* 
* @return void
* 
* @see taskControlTiempo Para el control de los intervalos de tiempo y la gestión de temporizadores.
* @see taskControlAmbiental Para iniciar el control ambiental y monitoreo de parámetros ambientales.
* @see taskControlHumedad Para iniciar el control y lectura del sensor de humedad.
* @see taskDisplayAmbiental Para iniciar la visualización ambiental en el display.
* @see taskActivarLedRed500 Para activar el LED rojo durante 500 ms como parte de la indicación de bloqueo.
* @see taskSonidoBloqueo Para iniciar el sonido de bloqueo a una frecuencia determinada.
*/
void funct_Fin_Inicio(void) {
  // Si el estado es Sign_General
  if(currentInput == Input::Sign_General) { 
    // Inicia las tareas relacionadas con el control ambiental y la visualización
    taskControlTiempo.SetIntervalMillis(5000);  // Establece un intervalo de 5000 ms para el control de tiempo.
    taskControlTiempo.Start();                   // Inicia el control de tiempo.
    taskControlAmbiental.Start();                // Inicia el control ambiental.
    taskControlHumedad.Start();                  // Inicia la lectura del sensor de humedad.
    taskDisplayAmbiental.Start();                // Inicia la visualización ambiental en el display.
  }
  // Si el estado es Sign_Block
  else if(currentInput == Input::Sign_Block) {
    // Inicia las tareas relacionadas con el bloqueo
    taskControlTiempo.SetIntervalMillis(7000);  // Establece un intervalo de 7000 ms para el control de tiempo en el estado de bloqueo.
    taskControlTiempo.Start();                   // Inicia el control de tiempo.
    taskActivarLedRed500.Start();                // Inicia la activación del LED rojo durante 500 ms.
    taskSonidoBloqueo.Start();                   // Inicia el sonido de bloqueo.

    lcd.clear();                                 // Limpia la pantalla LCD.
  }
}

/**
* @brief Inicializa el estado de bloqueo del sistema.
* 
* Esta función cambia el valor de currentInput y muestra un mensaje en la pantalla LCD 
* indicando que el sistema está bloqueado.
* 
* @return void
*/
void funct_Init_Bloqueado(void) {
  currentInput = Input::Unknow;      // Establece el estado de entrada como desconocido.

  lcd.setCursor(3, 0);               // Establece el cursor en la fila 0, columna 3.
  lcd.print("SISTEMA EN");           // Imprime "SISTEMA EN" en la pantalla LCD, fila 0.
  lcd.setCursor(4, 1);               // Establece el cursor en la fila 1, columna 4.
  lcd.print("BLOQUEO");              // Imprime "BLOQUEO" en la pantalla LCD, fila 1.
}


/**
* @brief Finaliza el estado de bloqueo del sistema.
* 
* Esta función se ejecuta cuando el sistema detecta que ha pasado el tiempo de bloqueo. 
* Detiene varias tareas relacionadas con el bloqueo, apaga los LEDs y el sonido de bloqueo, 
* limpia la pantalla LCD, reinicia los intentos de seguridad y prepara el sistema para volver a un estado normal.
* 
* @return void
* 
* @see taskControlTiempo Para controlar los intervalos de tiempo de bloqueo y desactivación de tareas.
* @see taskActivarLedRed500 Para activar el LED rojo durante 500 ms durante el estado de bloqueo.
* @see taskDesactivarLedRed500 Para desactivar el LED rojo durante 500 ms.
* @see taskSonidoBloqueo Para emitir un sonido de bloqueo a una frecuencia de 440 Hz.
* @see taskNoToneBloqueo Para desactivar el sonido de bloqueo.
*/
void funct_Fin_Bloqueado(void) {
  // Verifica si el estado actual es Sign_Tiempo
  if (currentInput == Input::Sign_Tiempo) {
    // Detiene las tareas relacionadas con el bloqueo
    taskActivarLedRed500.Stop();          // Detiene la tarea que activa el LED rojo durante 500 ms.
    taskDesactivarLedRed500.Stop();       // Detiene la tarea que desactiva el LED rojo durante 500 ms.
    taskSonidoBloqueo.Stop();             // Detiene el sonido de bloqueo.
    taskNoToneBloqueo.Stop();             // Detiene la tarea que desactiva el tono de bloqueo.
    taskControlTiempo.Stop();             // Detiene la tarea de control de tiempo relacionada con el bloqueo.

    noTone(PIN_BUZZER);                   // Detiene el sonido del zumbador.
    digitalWrite(PIN_RED, LOW);           // Apaga el LED rojo.

    lcd.clear();                          // Limpia la pantalla LCD.
    limpiarEntrada();                     // Limpia la entrada de datos de la pantalla.

    intentosSeguridad = 0;                // Reinicia los intentos de seguridad.

    currentInput = Input::Unknow;         // Establece el estado de entrada como desconocido.
  }
}

/**
* @brief Inicializa el estado ambiental.
* 
* Esta función se ejecuta para inicializar el estado relacionado con las condiciones ambientales.
* En este estado, se realiza una verificación de la temperatura actual, y si esta es superior al 
* umbral de temperatura definido, el sistema cambia al estado Sign_General. Si la temperatura 
* está por debajo del umbral, el sistema mantiene el estado actual Unknow.
* 
* @return void
*/
void funct_Init_Ambiental(void) { 
  currentInput = Input::Unknow;  // Establece el estado de entrada actual como desconocido.
}

/**
* @brief Finaliza el estado ambiental.
* 
* Esta función se ejecuta para finalizar el estado ambiental, deteniendo y reiniciando tareas 
* dependiendo del estado actual de la entrada, lo que permite gestionar el flujo entre 
* los estados de tiempo y general. 
* 
* - Si el estado es Sign_Tiempo, se detienen las tareas relacionadas con el control ambiental y 
*   se reinician las tareas de control del tiempo y visualización de eventos.
* - Si el estado es Sign_General, se detienen las tareas de visualización ambiental y los LEDs, 
*   y se inician las tareas de alarma.
* 
* @return void
* 
* @see taskControlAmbiental Para gestionar los intervalos de tiempo de lectura de parámetros ambientales.
* @see taskControlHumedad Para gestionar la lectura del sensor de humedad.
* @see taskControlTiempo Para gestionar las tareas relacionadas con el control del tiempo.
* @see taskActivarLedGreen Para activar el LED verde durante 800 ms.
* @see taskActivarLedRed150 Para activar el LED rojo durante 150 ms.
* @see taskSonidoAlarma Para iniciar el sonido de alarma a una frecuencia de 880 Hz.
* @see taskDisplayAmbiental Para actualizar la visualización ambiental en el display.
* @see taskDisplayEventos Para actualizar la visualización de eventos en el display.
*/
void funct_Fin_Ambiental(void){
  // Verificar si el estado actual es Sign_Tiempo
  if (currentInput == Input::Sign_Tiempo) {
    // Detiene las tareas relacionadas con el control ambiental
    taskControlAmbiental.Stop();      // Detiene el control de los parámetros ambientales.
    taskControlHumedad.Stop();        // Detiene la lectura del sensor de humedad.
    taskControlTiempo.Stop();         // Detiene el control de tiempo.
    taskDisplayAmbiental.Stop();      // Detiene la visualización ambiental.

    // Inicia las tareas relacionadas con el control de eventos
    taskControlTiempo.SetIntervalMillis(3000);  // Establece el intervalo para el control de tiempo.
    taskDisplayEventos.Start();                 // Inicia la visualización de eventos.
    taskControlTiempo.Start();                  // Inicia el control de tiempo.

    intentosSensores = 0;  // Reinicia el contador de intentos de sensores.
  }
  // Verificar si el estado actual es Sign_General
  else if(currentInput == Input::Sign_General){
    // Detiene las tareas relacionadas con el control ambiental
    taskActivarLedGreen.Stop();    // Detiene la activación del LED verde.
    taskDesactivarLedGreen.Stop(); // Detiene la desactivación del LED verde.
    taskControlAmbiental.Stop();   // Detiene el control ambiental.
    taskControlHumedad.Stop();     // Detiene la lectura del sensor de humedad.
    taskControlTiempo.Stop();      // Detiene el control de tiempo.
    taskDisplayAmbiental.Stop();   // Detiene la visualización ambiental.

    digitalWrite(PIN_GREEN, LOW);  // Apaga el LED verde.
    
    // Inicia las tareas relacionadas con el estado alarma
    taskActivarLedRed150.Start();  // Inicia el LED rojo durante 150 ms.
    taskSonidoAlarma.Start();      // Inicia el sonido de alarma a una frecuencia de 880 Hz.

    lcd.clear();  // Limpia la pantalla LCD para actualizar la información.
  }
}

/**
 * @brief Inicializa el estado de eventos.
 * 
 * Esta función se ejecuta para inicializar el estado de eventos, asignando el valor Unknow al estado 
 * actual de la entrada, lo que indica que el sistema no ha determinado el tipo de entrada aún.
 * 
 * @return void
 */
void funct_Init_Eventos(void){
  currentInput = Input::Unknow;  // Establece el estado de la entrada como desconocido al iniciar el estado de eventos.
}

/**
  * @brief Finaliza el estado de eventos y gestiona el inicio de nuevos estados.
  * 
  * Esta función se ejecuta para finalizar el estado de eventos y manejar la transición hacia 
  * otros estados dependiendo del valor de currentInput. Si el estado actual es Sign_Tiempo, 
  * se reinicia el control de tiempo y las tareas ambientales. Si el estado es Sign_General, 
  * se detienen las tareas de visualización de eventos y se inician las tareas de alerta.
  * 
  * @return void
  * 
  * @see taskControlTiempo Para gestionar las tareas relacionadas con el control de tiempo, como establecer intervalos.
  * @see taskDisplayEventos Para detener la visualización de eventos en el display cuando se sale del estado de eventos.
  * @see taskActivarLedGreen Para detener el LED verde durante 800 ms en el contexto de tareas generales.
  * @see taskDesactivarLedGreen Para detener el LED verde durante 800 ms en el contexto de tareas generales.
  * @see taskControlAmbiental Para iniciar los intervalos de tiempo de lectura de parámetros ambientales, como la temperatura y humedad.
  * @see taskControlHumedad Para iniciar el intervalo de lectura del sensor de humedad.
  * @see taskDisplayAmbiental Para iniciar la visualización de parámetros ambientales en el display.
  * @see taskSonidoAlerta Para iniciar el sonido de alerta a una frecuencia de 660 Hz durante el estado de alerta.
  * @see taskActivarLedBlue Para iniciar el LED azul durante 800 ms en el estado de alerta.
  */
void funct_Fin_Eventos(void){
  // Verificar si el estado actual es Sign_Tiempo
  if (currentInput == Input::Sign_Tiempo) {
    // Detiene las tareas relacionadas con el control de eventos
    taskDisplayEventos.Stop();  // Detiene la tarea de visualización de eventos en el display.
    taskControlTiempo.Stop();   // Detiene la tarea de control de tiempo.

    // Inicia las tareas relacionadas con el control ambiental
    taskControlTiempo.SetIntervalMillis(5000);  // Establece el intervalo de tiempo para el control ambiental.
    taskControlTiempo.Start();  // Inicia el control de tiempo con el nuevo intervalo.
    taskControlAmbiental.Start();  // Inicia el control de parámetros ambientales (temperatura, humedad).
    taskControlHumedad.Start();   // Inicia la tarea de lectura del sensor de humedad.
    taskDisplayAmbiental.Start(); // Inicia la visualización de los parámetros ambientales en el display.
  }
  // Verificar si el estado actual es Sign_General
  else if(currentInput == Input::Sign_General){
    // Detiene las tareas relacionadas con la visualización de eventos y control de tiempo
    taskDisplayEventos.Stop();  // Detiene la tarea de visualización de eventos en el display.
    taskControlTiempo.Stop();   // Detiene el control de tiempo en el estado general.
    taskActivarLedGreen.Stop(); // Detiene la tarea de activar el LED verde.
    taskDesactivarLedGreen.Stop(); // Detiene la tarea de desactivar el LED verde.

    digitalWrite(PIN_GREEN, LOW);  // Apaga el LED verde de forma manual.

    // Inicia las tareas relacionadas con el estado de alerta
    taskControlTiempo.SetIntervalMillis(3000);  // Establece el intervalo de tiempo para el control de tiempo en estado alerta.
    taskControlTiempo.Start();  // Inicia el control de tiempo con el nuevo intervalo.
    taskActivarLedBlue.Start(); // Inicia el LED azul durante 800 ms en el estado de alerta.
    taskSonidoAlerta.Start();   // Inicia el sonido de alerta a una frecuencia de 660 Hz.

    lcd.clear();  // Limpia la pantalla LCD al cambiar de estado.
  }
}

/**
* @brief Inicializa el estado de alerta y evalúa el número de intentos de sensores.
* 
* Esta función se ejecuta al inicio del estado de alerta, mostrando el mensaje "SISTEMA EN ALERTA" 
* en la pantalla LCD. Además, evalúa el número de intentos fallidos de los sensores, y si el número 
* de intentos alcanza o supera el límite de 3, cambia el estado a `Sign_Block` para bloquear el sistema 
* y evitar futuros intentos. Este mecanismo se utiliza como una medida de seguridad.
* 
* @return void
*/
void funct_Init_Alerta(void){
  
  // Restablecer el estado de la entrada al valor desconocido
  currentInput = Input::Unknow;  // Se restablece el estado de la entrada a "desconocido".

  // Mostrar el mensaje "SISTEMA EN ALERTA" en la pantalla LCD
  lcd.setCursor(3, 0);      // Establece la posición del cursor en la fila 0, columna 3.
  lcd.print("SISTEMA EN");  // Imprime "SISTEMA EN" en la primera línea de la pantalla LCD.
  lcd.setCursor(5, 1);      // Establece la posición del cursor en la fila 1, columna 5.
  lcd.print("ALERTA");      // Imprime "ALERTA" en la segunda línea de la pantalla LCD.

  // Verificar si el número de intentos de sensores es mayor o igual a 3
  if (intentosSensores >= 3) { 
    currentInput = Input::Sign_Block;  // Si los intentos superan el umbral, cambiar el estado a Sign_Block para bloquear el sistema.
  }
}

/**
* @brief Finaliza el estado de alerta y gestiona las tareas asociadas según el estado actual.
* 
* Esta función se ejecuta para finalizar el estado de alerta, realizando la detención de tareas activas 
* y la inicialización de nuevas tareas en función del estado actual del sistema (`Sign_Tiempo` o `Sign_Block`).
* 
* - Si el estado es `Sign_Tiempo`, la función detiene las tareas relacionadas con la alerta y reinicia 
*   el control de tiempo, además de activar la visualización de eventos.
* - Si el estado es `Sign_Block`, la función detiene las tareas de la alerta y activa tareas relacionadas 
*   con la alarma, incluyendo el LED rojo y el sonido de la alarma.
* 
* La función asegura una transición adecuada entre los diferentes estados del sistema, garantizando que 
* los recursos se gestionen de manera eficiente según el estado en el que se encuentre el sistema.
* 
* @return void No devuelve ningún valor. 
* 
* @see taskControlTiempo Para gestionar las tareas relacionadas con el control del tiempo.
* @see taskActivarLedBlue Para detener el LED azul durante 800 ms.
* @see taskDesactivarLedBlue Para detener el LED azul durante 800 ms.
* @see taskSonidoAlerta Para detener el sonido de alerta a una frecuencia de 660 Hz.
* @see taskNoToneAlerta Para detener el tono de alerta.
* @see taskActivarLedGreen Para iniciar el LED verde durante 800 ms.
* @see taskDisplayEventos Para iniciar la visualización de eventos en el display LCD.
* @see taskActivarLedRed150 Para iniciar el LED rojo durante 150 ms.
* @see taskSonidoAlarma Para iniciar el sonido de alarma a una frecuencia de 880 Hz.
*/
void funct_Fin_Alerta(void) {
  // Verificar si el estado actual es Sign_Tiempo
  if (currentInput == Input::Sign_Tiempo) {
    // Detiene las tareas relacionadas con la alerta
    taskActivarLedBlue.Stop();        // Detiene el LED azul.
    taskDesactivarLedBlue.Stop();     // Detiene la desactivación del LED azul.
    taskSonidoAlerta.Stop();          // Detiene el sonido de alerta (660 Hz).
    taskNoToneAlerta.Stop();          // Detiene cualquier tono de alerta.
    taskControlTiempo.Stop();         // Detiene el temporizador de control de tiempo.

    // Apagar buzzer y LED azul
    noTone(PIN_BUZZER);               // Apaga el buzzer.
    digitalWrite(PIN_BLUE, LOW);      // Apaga el LED azul.

    // Inicia las tareas de control de eventos
    taskControlTiempo.SetIntervalMillis(3000); // Configura el intervalo para el control de tiempo.
    taskControlTiempo.Start();          // Inicia el control de tiempo.
    taskActivarLedGreen.Start();       // Inicia el LED verde durante 800 ms.
    taskDisplayEventos.Start();        // Inicia la visualización de eventos en el display LCD.
  }
  // Verificar si el estado actual es Sign_Block
  else if (currentInput == Input::Sign_Block) {
    // Detiene las tareas relacionadas con la alerta
    taskActivarLedBlue.Stop();        // Detiene el LED azul.
    taskDesactivarLedBlue.Stop();     // Detiene la desactivación del LED azul.
    taskSonidoAlerta.Stop();          // Detiene el sonido de alerta (660 Hz).
    taskNoToneAlerta.Stop();          // Detiene cualquier tono de alerta.
    taskControlTiempo.Stop();         // Detiene el temporizador de control de tiempo.

    // Apagar buzzer y LED azul
    noTone(PIN_BUZZER);               // Apaga el buzzer.
    digitalWrite(PIN_BLUE, LOW);      // Apaga el LED azul.

    // Inicia las tareas relacionadas con el estado de bloqueo
    taskActivarLedRed150.Start();     // Inicia el LED rojo durante 150 ms.
    taskSonidoAlarma.Start();         // Inicia el sonido de alarma a 880 Hz.

    // Limpiar la pantalla LCD
    lcd.clear();                      // Borra la pantalla LCD.
  }
}

/**
* @brief Inicializa y muestra el estado de alarma en el sistema.
* 
* Esta función se ejecuta al inicio del estado de alarma. Su objetivo es notificar al usuario que el sistema ha 
* entrado en estado de alarma. Para ello, se muestra un mensaje en la pantalla LCD y se espera a que el usuario 
* presione una tecla para cambiar de estado. En particular, si se presiona el carácter '*' en el teclado, el 
* sistema cambia al estado Sign_Key, permitiendo la transición hacia otras acciones del sistema.
* 
* Esta función está diseñada para interactuar con el usuario, proporcionando una forma de iniciar un proceso 
* de validación o autenticación (dependiendo del sistema) al detectar una entrada específica del usuario.
* 
* @return void 
*/
void funct_Init_Alarma(void) {
  // Restablecer el estado de la entrada al valor desconocido
  currentInput = Input::Unknow;

  // Mostrar el mensaje "SISTEMA EN ALARMA" en la pantalla LCD
  lcd.setCursor(3, 0);      // Establece la posición del cursor en la fila 0, columna 3.
  lcd.print("SISTEMA EN");  // Imprime "SISTEMA EN" en la primera línea de la pantalla LCD.
  lcd.setCursor(5, 1);      // Establece la posición del cursor en la fila 1, columna 5.
  lcd.print("ALARMA");      // Imprime "ALARMA" en la segunda línea de la pantalla LCD.

  // Esperar que el usuario presione '*' para cambiar el estado
  char key = keypad.getKey();  // Obtener la tecla presionada desde el teclado.

  if (key == '*') {
    // Si se presiona '*' se cambia el estado a Sign_Key
    currentInput = Input::Sign_Key;  // Cambiar el estado a Sign_Key para iniciar la validación.
  }
}

/**
* @brief Finaliza el estado de alarma y restablece el sistema a un estado seguro.
* 
* Esta función se ejecuta cuando es necesario finalizar el estado de alarma. Se detienen todas las tareas relacionadas con la alarma, 
* se apagan los componentes de la alarma como el LED rojo y el sonido del buzzer, y se limpia la pantalla LCD. 
* Si el estado actual es `Sign_Key`, se detienen las tareas específicas de la alarma y se reinician las tareas relacionadas con 
* el control del tiempo de entrada. Además, se restablecen las variables y el sistema vuelve al estado desconocido.
* 
* @return void No devuelve ningún valor.
* 
* @see taskActivarLedRed150 Para activar el LED rojo en la alarma.
* @see taskSonidoAlarma Para generar el sonido de la alarma.
* @see taskDesactivarLedRed150 Para desactivar el LED rojo.
* @see taskNoToneAlarma Para detener el sonido de la alarma.
*/
void funct_Fin_Alarma(void) {
  // Verificar si el estado actual es Sign_Key
  if (currentInput == Input::Sign_Key) {
    // Detener las tareas asociadas al estado de alarma
    taskActivarLedRed150.Stop();     // Detener tarea de activación del LED rojo.
    taskDesactivarLedRed150.Stop();  // Detener tarea de desactivación del LED rojo.
    taskSonidoAlarma.Stop();         // Detener tarea que genera el sonido de la alarma.
    taskNoToneAlarma.Stop();         // Detener tarea para desactivar el tono de la alarma.

    // Apagar el buzzer y el LED rojo
    noTone(PIN_BUZZER);              // Detener el sonido del buzzer.
    digitalWrite(PIN_RED, LOW);      // Apagar el LED rojo.

    // Limpiar la pantalla LCD y restablecer los valores
    lcd.clear();                     // Limpiar la pantalla LCD.
    limpiarEntrada();                // Limpiar los valores de entrada previos.
    intentosSeguridad = 0;           // Restablecer el contador de intentos de seguridad.

    // Cambiar el estado del sistema a desconocido
    currentInput = Input::Unknow;   // Establecer el estado como desconocido.
  }
}

/**
* @brief Realiza la lectura de los valores ambientales, como la temperatura y la luz.
* 
* Esta función lee los valores de los sensores de temperatura y luz. La temperatura se calcula utilizando una fórmula basada en la resistencia del termistor, 
* y la luz se mide directamente mediante una fotocélula. Los resultados se almacenan en las variables globales `valorTemperatura` y `valorLuz`. 
* Además, si la temperatura supera un umbral definido por `TH_TEMP_HIGH`, se cambia el estado del sistema a `Sign_General`.
* 
* @return void
*/
void lecturaAmbiental() {
  // Lee el valor del sensor de temperatura (PIN_TEMP) y calcula la temperatura en grados Celsius
  long a = 1023 - analogRead(PIN_TEMP);  // Lee el valor de la temperatura y ajusta el valor para la fórmula.
  valorTemperatura = BETA / (log((1025.0 * 10 / a - 10) / 10) + BETA / 298.0) - 273.0;  // Calcula la temperatura en grados Celsius usando la fórmula.

  // Lee el valor del sensor de luz (PIN_PHOTOCELL)
  valorLuz = analogRead(PIN_PHOTOCELL);  // Lee el valor del sensor de luz.
  
  // Verifica si la temperatura supera el umbral alto y cambia el estado si es necesario
  if (valorTemperatura > TH_TEMP_HIGH) {
    currentInput = Input::Sign_General;  // Cambia el estado del sistema a Sign_General si la temperatura es alta.
  }
}

/**
* @brief Lee el valor de la humedad ambiental usando un sensor DHT.
* 
* Esta función utiliza la librería DHT para obtener el valor de la humedad ambiental desde el sensor DHT conectado al sistema.
* - Si la lectura del sensor es exitosa, el valor de la humedad se almacena en la variable valorHumedad.
* - Si la lectura del sensor falla (es decir, el valor de la humedad es NaN), se muestra un mensaje de error en la pantalla LCD.
* 
* @note La función dht.readHumidity() devuelve un valor NaN si no puede leer correctamente el valor de humedad, lo cual indica un fallo en la lectura del sensor.
* 
* @see dht Para la configuración y lectura del sensor DHT.
* @see valorHumedad Para el almacenamiento del valor de la humedad leída.
* @see lcd Para la visualización del mensaje de error en la pantalla LCD.
*/
void lecturaHumedad() {
  valorHumedad = dht.readHumidity();  // Realizar la lectura de la humedad desde el sensor DHT.
  // Verificar si la lectura ha fallado (NaN)
  if (isnan(valorHumedad)) {  
    lcd.print(F("Error "));           // Mostrar mensaje de error en la pantalla LCD si la lectura falló.
    return;                           // Salir de la función en caso de error en la lectura.
  }
}

/**
* @brief Actualiza la pantalla LCD con los valores de temperatura, luz y humedad.
* 
* Esta función limpia la pantalla LCD y luego imprime los valores actuales de temperatura, luz y humedad 
*/
void actualizarDisplayAmbiental() {
  lcd.clear();

  // Imprimir el valor de la temperatura
  lcd.print("T:");
  lcd.print(valorTemperatura, 1);  
  lcd.print(" C");

  // Imprimir el valor de la luz
  lcd.setCursor(0, 1); 
  lcd.print("L:");
  lcd.print(valorLuz);  

  // Imprimir el valor de la humedad
  lcd.setCursor(8, 1);  
  lcd.print("H:");
  lcd.print(valorHumedad);  
}

/**
* @brief Actualiza el estado de los sensores de movimiento en la pantalla LCD.
* 
* Esta función lee el estado de dos sensores de movimiento (infrarrojo y Hall) y muestra sus resultados en la pantalla LCD.
* Si ambos sensores detectan movimiento simultáneamente, se incrementa un contador de intentos y se cambia el estado del sistema a Sign_General.
* 
* - **Sensor IR**: El sensor infrarrojo (IR) detecta movimiento en su rango de detección. Si detecta movimiento, se muestra "IR: ON" en la pantalla LCD.
* - **Sensor Hall**: El sensor Hall detecta variaciones magnéticas. Si detecta un campo magnético, se muestra "Hall: ON" en la pantalla LCD.
* 
* Si el sensor infrarrojo detecta movimiento, el estado del sistema cambia a Sign_General, lo que indica que se ha detectado un evento importante. 
* Además, se incrementa el contador intentosSensores para llevar un registro de los eventos de detección.
* 
* @note Si el sensor IR detecta movimiento (varIR == 0), se asume que el sistema debe cambiar al estado Sign_General, independientemente de lo que haga el sensor Hall.
*/
void actualizarDisplayEventos() {
  lcd.clear();  // Limpiar la pantalla LCD antes de mostrar nuevos datos.

  // Leer los valores de los sensores de infrarrojo (IR) y Hall
  bool varIR = digitalRead(PIN_IR);   // Lectura del sensor infrarrojo (IR).
  bool varHall = digitalRead(PIN_SH); // Lectura del sensor Hall.

  // Imprimir el estado del sensor IR en la primera línea de la pantalla LCD
  lcd.setCursor(0, 0);  
  if(varIR == 0){
    lcd.print("IR: ON");  // El sensor IR está activado (movimiento detectado).
  }
  else{
    lcd.print("IR: OFF"); // El sensor IR está desactivado (sin movimiento).
  }

  // Imprimir el estado del sensor Hall en la segunda línea de la pantalla LCD
  lcd.setCursor(0, 1);
  if(varHall == 1){
    lcd.print("Hall: ON"); // El sensor Hall está activado (campo magnético detectado).
  }
  else{
    lcd.print("Hall: OFF"); // El sensor Hall está desactivado (sin campo magnético).
  }

  // Si el sensor infrarrojo detecta movimiento (IR detectado), cambiar el estado y aumentar el conteo de intentos
  if((varIR == 0)) {
    currentInput = Input::Sign_General;  // Cambiar el estado a Sign_General debido al movimiento detectado por el sensor IR.
    intentosSensores++;                  // Incrementar el contador de intentos de detección de sensores.
  }
}

/**
* @brief Limpia el contenido de la entrada de usuario
* 
* Esta función restablece la entrada a todos los elementos a cero y reinicia el contador de dígitos de entrada a cero
*/
void limpiarEntrada() {
  memset(entrada, 0, sizeof(entrada));  // Limpiar el array de entrada
  digitosEntrada = 0;                   // Reiniciar el contador de dígitos
}

/**
* @brief Controla las transiciones basadas en la validez de la clave ingresada
* 
* Esta función maneja las distintas situaciones que pueden ocurrir durante el proceso de validación de una clave de acceso:
* - Si la clave es incorrecta:
*   - Se apagan los LEDs.
*   - Se limpia la pantalla LCD.
*   - Se inicia el temporizador de entrada.
* - Si la clave es correcta, el sistema cambia al estado `Sign_General`.
* - Si no se ha ingresado ninguna clave, el sistema mantiene el estado `Sign_Tiempo`.
* 
* @see taskControlTiempo Para controlar el temporizador de tiempo.
* @see taskActivarLedBlue300 Para activar el LED azul durante 300 ms en caso de clave incorrecta.
* @see taskDesactivarLedBlue300 Para desactivar el LED azul después de que se haya mostrado el error.
*/
void controlTiempo() {
  if(claveIncorrecta){
    claveIncorrecta = false;          // Cambia el estado de la clave incorrecta a falso

    taskControlTiempo.Stop();         // Detiene el temporizador de tiempo
    taskActivarLedBlue300.Stop();     // Detiene la tarea de activar el LED azul
    taskDesactivarLedBlue300.Stop();  // Detiene la tarea de desactivar el LED azul

    digitalWrite(PIN_BLUE, LOW);      // Apaga el LED azul
    lcd.clear();                      // Limpia la pantalla LCD

    currentInput = Input::Unknow;     // Cambia el estado de la entrada a desconocido
  }
  else if(claveCorrecta){
    claveCorrecta = false;                // Cambia el estado de la clave correcta a falso

    taskControlTiempo.Stop();             // Detiene el temporizador de tiempo

    currentInput = Input::Sign_General;   // Cambia al estado Sign_General cuando la clave es correcta
  }
  else{
    currentInput = Input::Sign_Tiempo;    // Cambia al estado Sign_Tiempo si no se ha ingresado clave
  } 
}

/**
* @brief Controla el proceso de validación de la clave de acceso y maneja los intentos de seguridad.
* 
* Esta función verifica los intentos de entrada de una clave. Si se alcanza el número máximo de intentos incorrectos (3 intentos),
* bloquea temporalmente el acceso. Si la clave es incorrecta, muestra un mensaje de error en el LCD y activa el LED azul como 
* indicativo de error. Si el número de intentos es menor a 3, se reinicia el contador de intentos y se espera otro intento.
* Además, gestiona el intervalo de tiempo entre los intentos incorrectos mediante la tarea taskControlTiempo, y muestra
* un mensaje de error en el display LCD.
* 
* @see taskControlTiempoEntrada Para controlar el intervalo de tiempo de entrada.
* @see taskControlTiempo Para controlar el intervalo entre intentos incorrectos.
* @see taskActivarLedBlue300 Para iniciar el LED azul durante 300 ms en caso de clave incorrecta.
*/
void controlEntrada() {
  taskControlTiempoEntrada.Stop();              // Detiene la tarea de control de tiempo de entrada.
  limpiarEntrada();                             // Limpia la pantalla de entrada y restablece el estado.
  lcd.clear();                                  // Limpia la pantalla LCD.
  intentosSeguridad++;                          // Incrementa el contador de intentos de seguridad.

  // Si se alcanzan los 3 intentos incorrectos, se bloquea el acceso temporalmente
  if (intentosSeguridad >= 3) {
    currentInput = Input::Sign_Block;           // Bloquea el acceso temporalmente al detectar 3 intentos incorrectos.
  }
  else {
    // Si la clave es incorrecta, se muestra un mensaje de error y se activa el LED azul
    claveIncorrecta = true;                     // Marca que la clave es incorrecta.
    taskControlTiempo.SetIntervalMillis(3000);  // Establece el intervalo de 3 segundos para el temporizador de error.
    taskControlTiempo.Start();                  // Inicia el temporizador de error.
    taskActivarLedBlue300.Start();              // Activa el LED azul durante 300 ms para indicar error de clave.

    // Muestra un mensaje de error en la pantalla LCD
    lcd.setCursor(5, 0);             // Posiciona el cursor en la primera fila.
    lcd.print("CLAVE");              // Imprime "CLAVE" en la pantalla LCD.
    lcd.setCursor(3, 1);             // Posiciona el cursor en la segunda fila.
    lcd.print("INCORRECTA");         // Imprime "INCORRECTA" en la pantalla LCD.

    // Se restaura el estado de entrada para permitir nuevos intentos.
    currentInput = Input::Unknow;    // Resetea la entrada actual a desconocida.
  }
}

