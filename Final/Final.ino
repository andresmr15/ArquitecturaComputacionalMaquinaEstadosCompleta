#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include "DHTStable.h"
#include <LiquidCrystal.h>
#include <Keypad.h>

// Declaración de constantes para los pines de los LEDs, el buzzer y los sensores
// Los pines LED_RED, LED_GREEN y LED_BLUE están asociados a los pines digitales 49, 51 y 53 respectivamente
// El pin BUZZER está asociado al pin digital 5
// El pin DHT11_PIN está asociado al pin analógico A1
// El pin PHOTOCELL_PIN está asociado al pin analógico A0
// Los pines METAL, PROXIMIDAD y HALL están asociados a los pines digitales 20, 21 y 19 respectivamente
// DHT es una instancia de la clase DHTStable
#define LED_RED 49
#define LED_GREEN 51
#define LED_BLUE 53
#define BUZZER 5
#define DHT11_PIN A1
#define PHOTOCELL_PIN A0
#define METAL 20
#define PROXIMIDAD 21
#define HALL 19
DHTStable DHT;

// Declaración de funciones
void fnInicioSesion();
void fnMonitoreo();
void outputC();
void fnDHC();
void photocell();

// Declaración de variables globales
char password[6] = {'3','2','1','3','2','1'};
char inPassword[6];
short int count = 0;
short int trycount = 0;
const uint8_t ROWS = 4;
const uint8_t COLS = 4;
short int luz = 0.0;
float temp = 0.0;
int valorSensor=1;
unsigned int startTime=0;
unsigned int actualTime=0;
boolean estado;

// Definición del teclado (keypad) y su configuración
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

uint8_t colPins[COLS] = { 26, 27, 28, 29 }; // Pines conectados a C1, C2, C3, C4
uint8_t rowPins[ROWS] = { 22, 23, 24, 25 }; // Pines conectados a R1, R2, R3, R4
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Configuración de la pantalla LCD (LiquidCrystal)
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

// Enumeración para representar los estados del programa
enum State
{
  inicioSesion = 0, // Estado de inicio de sesión
  puertasVentanas = 1, // Estado de control de puertas y ventanas
  monitoreo = 2, // Estado de monitoreo
  alarmaAmbiental = 3, // Estado de alarma ambiental
  alertaSeguridad = 4 // Estado de alerta de seguridad
};

// Enumeración para representar las entradas del programa
enum Input
{
  clvCorrecta = 0, // Entrada para una clave correcta
  temp32 = 1, // Entrada de temperatura 32°C
  temp30 = 2, // Entrada de temperatura 30°C
  timeOut2 = 3, // Entrada de tiempo de espera 2 segundos
  timeOut10 = 4, // Entrada de tiempo de espera 10 segundos
  timeOut6 = 5, // Entrada de tiempo de espera 6 segundos
  actPuertasVentanas = 6, // Entrada de activación de puertas y ventanas
  temp32_5 = 7, // Entrada de temperatura 32.5°C
  Unknown = 8 // Entrada desconocida
};

// Creación de una nueva instancia de la máquina de estados
StateMachine stateMachine(5, 8);

// Variable para almacenar la última entrada de usuario
Input input;

// Función para mostrar la temperatura en la pantalla LCD
void pcdTemp()
{
  lcd.setCursor(2,1);
  DHT.read11(DHT11_PIN);
  temp = DHT.getTemperature();
  lcd.print(temp);

  digitalWrite(LED_GREEN, HIGH);
  // Activar el zumbador, tono de alerta
  tone(BUZZER, 1000, 200);
  delay(200);
  noTone(BUZZER);  
}

// Función para mostrar la humedad y temperatura en la pantalla LCD
void pcdDHT()
{
  digitalWrite(LED_GREEN, LOW);

  lcd.setCursor(4,0);
  lcd.print("                ");
  lcd.setCursor(4,0);
  DHT.read11(DHT11_PIN);
  lcd.print(DHT.getHumidity());
  lcd.print(" ");
  temp = DHT.getTemperature();
  lcd.print(temp);
}

// Función para mostrar la cantidad de luz en la pantalla LCD
void pcdPhoto()
{
  lcd.setCursor(4,1);
  lcd.print("                ");
  lcd.setCursor(4,1);
  luz = analogRead(PHOTOCELL_PIN);
  lcd.print(luz);
}


// Creación de tareas asincrónicas para actualizar los datos de temperatura, humedad y luz en la pantalla LCD
AsyncTask asyncTaskTemp(2000, true, pcdTemp);
AsyncTask asyncTaskDHT(2000, true, pcdDHT);
AsyncTask asyncTaskPhoto(2000, true, pcdPhoto);

// Configuración de la máquina de estados
void setupStateMachine()
{
  // Agregar transiciones
  stateMachine.AddTransition(inicioSesion, puertasVentanas, []() { return input == clvCorrecta; });

  stateMachine.AddTransition(puertasVentanas, monitoreo, []() { return input == timeOut2; });
 
  stateMachine.AddTransition(puertasVentanas, alertaSeguridad, []() { return input == actPuertasVentanas; });

  stateMachine.AddTransition(monitoreo, alarmaAmbiental, []() { return input == temp32; });

  stateMachine.AddTransition(monitoreo, puertasVentanas, []() { return input == timeOut10; });
 
  stateMachine.AddTransition(alarmaAmbiental, alertaSeguridad, []() { return input == temp32_5; });

  stateMachine.AddTransition(alarmaAmbiental, monitoreo, []() { return input == temp30; });

  stateMachine.AddTransition(alertaSeguridad, puertasVentanas, []() { return input == timeOut6; });

  // Asignar acciones
  stateMachine.SetOnEntering(inicioSesion, pcdInicioSesion);
  stateMachine.SetOnEntering(monitoreo, pcdMonitoreo);
  stateMachine.SetOnEntering(alarmaAmbiental, pcdAlarmaAmbiental);
  stateMachine.SetOnEntering(alertaSeguridad, pcdAlertaSeguridad);
  stateMachine.SetOnEntering(puertasVentanas, pcdPuertasVentanas);
}

// Función para reiniciar la pantalla LCD
void reiniciarLCD(){
    delay(300);
    lcd.setCursor(0,0);
    lcd.print("                ");
    lcd.setCursor(0,1);
    lcd.print("                ");
    lcd.setCursor(0,0);
}

 // Función para manejar el caso de una clave correcta
void claveCorrecta(){
  trycount=0;
  digitalWrite(LED_RED, HIGH);
  lcd.setCursor(0,1);
  reiniciarLCD();
  lcd.print("Correcta :)");

  // Reproducir tono de confirmación
  tone(BUZZER, 1800, 100);
  delay(100);
  tone(BUZZER, 2000, 100);
  delay(100);
  tone(BUZZER, 2200, 100);
  delay(100);
  tone(BUZZER, 2500, 350);
  delay(350);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_RED, LOW);
  reiniciarLCD();
}

// Función para manejar el caso de una clave incorrecta
void claveIncorrecta(){
  digitalWrite(LED_BLUE, HIGH);
  lcd.setCursor(0,1);
  lcd.print("Incorrecta >:(");

  // Reproducir tono de error
  tone(BUZZER, 1000, 200);
  delay(200);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_BLUE, LOW);
  reiniciarLCD();
  lcd.print("Clave:");
  trycount++;
}

// Función para manejar el caso de tiempo agotado
void tiempoAgotado(){
  digitalWrite(LED_BLUE, HIGH);
  lcd.setCursor(0,1);
  lcd.print("Tiempo agotado...");
 
  // Reproducir tono de aviso
  tone(BUZZER, 200, 150);
  delay(200);
  tone(BUZZER, 200, 150);
  delay(150);
  noTone(BUZZER);

  delay(1000);
  digitalWrite(LED_BLUE, LOW);
  reiniciarLCD();
  lcd.print("Clave:");
  trycount++;
}

// Función para comprobar si la clave ingresada es correcta
boolean comprobarClave(){
    for(int i=0; i<6; i++){
      if(inPassword[i] != password[i]){
        return false;
      }
    }
    return true;
}
bool updateDisplay(){
    if(trycount==3){
      // Caso de bloqueo del sistema por intentos fallidos
      digitalWrite(LED_GREEN, HIGH);
      lcd.setCursor(0,1);
      lcd.print("Sis.bloqueado XD");
      if(trycount==3){
        // Reproducir tono de bloqueo
        tone(BUZZER, 100, 150);
        delay(150);
        noTone(BUZZER);
        trycount++;
      }
    }
    else if(count==6){
      // Caso de ingreso completo de la clave
      count = 0;
      estado = comprobarClave();
      if(estado == true){
        // Clave correcta
        claveCorrecta();
        delay(100);
        input=static_cast<Input>(Input::clvCorrecta);
        return true;
      }
      else{
        // Clave incorrecta
        claveIncorrecta();
        return false;
      }
    }
}

void setup()
{
  // Inicialización del puerto serie
  Serial.begin(9600);
 
  // Configuración de pines
  lcd.print("Clave:");
  pinMode(LED_RED, OUTPUT); // LED indicador de sistema bloqueado
  pinMode(LED_BLUE, OUTPUT); // LED indicador de clave incorrecta
  pinMode(LED_GREEN, OUTPUT); // LED indicador de clave correcta
  pinMode(BUZZER, OUTPUT); // Buzzer
  pinMode(HALL, INPUT); // Sensor Hall
  attachInterrupt(digitalPinToInterrupt(HALL), pcdHall, FALLING); // Asignar interrupción al Sensor Hall
  pinMode(METAL, INPUT); // Sensor de metal
  attachInterrupt(digitalPinToInterrupt(METAL), pcdMetal, FALLING); // Asignar interrupción al Sensor de metal
  pinMode(PROXIMIDAD, INPUT); // Sensor de proximidad
  attachInterrupt(digitalPinToInterrupt(PROXIMIDAD), pcdProximidad, FALLING); // Asignar interrupción al Sensor de proximidad

  // Iniciar tareas asíncronas
  asyncTaskTemp.Start();
  asyncTaskDHT.Start();
  asyncTaskPhoto.Start();
 
  // Configurar la máquina de estados
  setupStateMachine();  

  // Estado inicial
  stateMachine.SetState(inicioSesion, false, true);

  // Inicializar pantalla LCD
  lcd.begin(16, 2);
}

// Función auxiliar para leer el sensor HALL
void pcdHall(){
    valorSensor = digitalRead(HALL);
}

// Función auxiliar para leer el sensor METAL
void pcdMetal(){
    valorSensor = digitalRead(METAL);
}

// Función auxiliar para leer el sensor de proximidad
void pcdProximidad(){
    valorSensor = digitalRead(PROXIMIDAD);
}


void loop()
{
  // Actualizar la máquina de estados
  stateMachine.Update();

  // Obtener el estado actual
  int estado = stateMachine.GetState();  

  // Imprimir el valor del sensor en el puerto serie
  Serial.println(valorSensor);

  // Realizar acciones según el estado actual
  switch(estado){
    case puertasVentanas:
         
          if(valorSensor == 0){
            input = static_cast<Input>(Input::actPuertasVentanas);
          }
          actualTime = millis();
          if(actualTime - startTime >= 2000){
            input = static_cast<Input>(Input::timeOut2);
          }  
          break;
    case monitoreo:
          asyncTaskDHT.Update();
          asyncTaskPhoto.Update();
          if(temp > 26.00){
            asyncTaskTemp.Update();  
            delay(2000);
            input = static_cast<Input>(Input::temp32);
          }
          actualTime = millis();
          if(actualTime - startTime >= 10000){
            input = static_cast<Input>(Input::timeOut10);
          }  
          break;
    case alarmaAmbiental:
          asyncTaskTemp.Update();
          if(temp < 26.00){
            reiniciarLCD();
            input = static_cast<Input>(Input::temp30);
          }
          actualTime = millis();
          if(temp > 26.00 && actualTime - startTime >= 5000){
            reiniciarLCD();
            input = static_cast<Input>(Input::temp32_5);
          }
          break;
    case alertaSeguridad:
          actualTime = millis();
          if(actualTime - startTime >= 6000){
            input = static_cast<Input>(Input::timeOut6);
          }  
          break;
  }
}


// Función de salida auxiliar para el estado "inicioSesion"
void pcdInicioSesion()
{
  bool estado;
  while(true){
   
    // Mostrar el cursor
    if(millis() / 250 % 2 == 0 && trycount < 3)
    {
      lcd.cursor();
    }
    else{
      lcd.noCursor();
    }
    estado = updateDisplay();
    if(estado == true){
      break;
    }
   
    // Si se presiona una tecla, se muestra un asterisco (*)
    char key = keypad.getKey();
    if (key) {
      // Guardar el tiempo en el que se presiona una tecla
      startTime = millis();
      lcd.print("*");
      inPassword[count] = key;
      count++;
    }
    // Actualizar el tiempo actual
    actualTime = millis();
 
    if(actualTime - startTime >= 4000 && count > 0){
      tiempoAgotado();
      startTime = 0; // Reiniciar el tiempo actual
      count = 0;
    }
  }
}

// Función de salida auxiliar para el estado "monitoreo"
void pcdMonitoreo()
{
  reiniciarLCD();
  lcd.print("H/T:");
  lcd.setCursor(0,1);
  lcd.print("Luz:");
  startTime = millis();
}

// Función de salida auxiliar para el estado "alarmaAmbiental"
void pcdAlarmaAmbiental()
{
  reiniciarLCD();
  lcd.print("ALARMA AMBIENTAL");
  lcd.setCursor(0,1);
  lcd.print("T:");
  startTime = millis();
}

// Función de salida auxiliar para el estado "alertaSeguridad"
void pcdAlertaSeguridad(){
  reiniciarLCD();
  lcd.print("ALARMA DE SEGURIDAD");
  startTime = millis();
}

// Función de salida auxiliar para el estado "puertasVentanas"
void pcdPuertasVentanas(){
  reiniciarLCD();
  lcd.print("PUERTAS Y VENTANAS");
  startTime = millis();
  valorSensor = 1;
}
