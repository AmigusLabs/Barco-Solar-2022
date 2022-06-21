#include <Streaming.h> //para la salida de print y endl http://arduiniana.org/libraries/streaming/
#include "current.h"
#include "Config.h"
#include <Servo.h>
#include "Arduino.h"
#include "ListLib.h"

//---------------DEBUG---------------
//#define PIN_DEBUG 8
bool DEBUG = true;
//-------------------P&O SETTINGS--------------


#define PO_ENABLE_SPEED 30
#define PO_TIMER 150
#define PO_STEP 1
#define po_samples 20

List<float> listVol;
List<float> listAmp;

float v = 0;
float i = 0;
float p = 0;
float Av = 0; //Delta Volts
float Ap = 0; //Delta Power
float p_prev = 0;
float v_prev = 0;
float ms = 0;
float velo = 0;

float i_media = 0;
float v_media = 0;
int cnt_po = 1;
int cnt_po_ina_samples = 0;
int po_speed = 0; //valor d po_speed cuando entramos en po
bool po_enable = false;
void PO_timer_callback();

//-----------------ENTRADAS----------------------
#define RECEPTOR_CH3_PIN 0 // Channel 3 velocity
#define RECEPTOR_CH6_PIN 9 // Channel 6 control Auto/Manual
#define RECEPTOR_CH7_PIN 8 // Channel 7

#define SALIDA_ESC_PIN 10 // to servo or ESC
#define initpulse   1000 //algunos ESC se inician con un pulso de 1000 otros cn 1500 otros cn 2000
//#define inc_steps   1

Servo ESC;

int vel = initpulse;

//-----------------ESC----------------------
#define sensibilidad 3
int velocidad = 0;

//-----------------INA219---------------
#define Ki 270
#define Kv

//--------------SWITCH--------------
int dato_switch = 0;
unsigned long timeout_pwm_micros = 50000;

void setup() {
  Serial.begin(115200);
  pinMode(RECEPTOR_CH3_PIN, INPUT);
  pinMode(RECEPTOR_CH6_PIN, INPUT);
  pinMode(RECEPTOR_CH7_PIN, INPUT);
  ESC.attach(SALIDA_ESC_PIN);
  delay(10);
  delay(1000);

  init_INA219(0);

  po_enable = false; //indicamos que iniciamos en speed

  for (int i = 0; i <= po_samples; i++)
  {
    listVol.Add(0);
    listAmp.Add(0);
  }

}

bool switchActivado()
{
  unsigned long value1 = pulseIn(RECEPTOR_CH6_PIN, HIGH, timeout_pwm_micros);
  unsigned long value2 = pulseIn(RECEPTOR_CH6_PIN, HIGH, timeout_pwm_micros);
  if ((value1 > 1200) && (value2 > 1200))
  {
    Serial.println("Modo Manual");
    return false;
  }
  Serial.println("Modo Automático");
  return true;
}


void loop() {
  velo = pulseIn(RECEPTOR_CH3_PIN, HIGH);
  velocidad = map(velo, 1000, 2000, 0, 100);
  //speed = speed/sensibilidad;

  Serial.print("Velocidad: ");
  Serial.println(velocidad);
  Serial.print("Voltaje: ");
  Serial.print(get_LoadVolts());
  Serial.print(" - Potencia: ");
  Serial.println(get_LoadCurrent());

  if (velocidad >= 100) {
    velocidad = 100;
  }


  listVol.RemoveFirst();
  listAmp.RemoveFirst();

  listVol.Add(get_LoadVolts());
  listAmp.Add(get_LoadCurrent() / Ki);


  if (switchActivado())
  {

    if (velocidad < 0) velocidad = 0;
    if (velocidad < PO_ENABLE_SPEED) { //Si po_speed <30, vel fijada x mando
      po_enable = false; //indicamos que estamos en modo speed
      cnt_po = 1; //reseteamos indicador de P&O para que cuando comience se resetee en ciclo 1 y pille speed
    } else {

      if (cnt_po == 1) {
        for (int i = 0; i <= po_samples; i++)
        {
          listVol.Add(get_LoadVolts());
          listAmp.Add(get_LoadCurrent() / Ki);
        }
        cnt_po == 0;

        po_enable = true;
        po_speed = PO_ENABLE_SPEED;
        po_speed += PO_STEP;


        v_media = 0;
        i_media = 0;
      }
    }


    for (int i = 0; i < po_samples; i++)
    {
      v_media += listVol[i];
      i_media += listVol[i];
    }

    v = v_media / po_samples;
    i = i_media / po_samples;

    p = i * v;

    Av = v - v_prev;
    Ap = p - p_prev;

    //DEBUG = digitalRead(PIN_DEBUG);
    //Si po_speed >50 entra P&0
    /*
      ----------COMIENZA P&O--------------------
    */

    if (Ap == 0) {
      po_speed += PO_STEP;
      //Serial.println("AP=0");
    } else {
      if (Ap > 0) {
        //Serial.println("AP>0");
        if (Av < 0) {
          po_speed += PO_STEP;
          velocidad = po_speed;
          //Serial.println("B2");

        } else if (Av >= 0) {
          po_speed -= PO_STEP;
          velocidad = po_speed;
          //Serial.println("B1");
        }
      } else {
        //Serial.println("AP<0");
        if (Av > 0) {
          po_speed += PO_STEP;
          velocidad = po_speed;
          //Serial.println("A2");
        } else if (Av <= 0) {
          po_speed -= PO_STEP;
          velocidad = po_speed;
          //Serial.println("A1");
        }
      }
    }

  }

  p_prev = p;
  v_prev = v;

  v_media = 0;
  i_media = 0;

  if (po_speed >= 100) {
    po_speed = 100;
  }

  if (DEBUG) {
    Serial.println("TX_PPM,Mode,ESC_PPM,Voltage,Current,Power");
    Serial.println();
    Serial.print(velocidad);
    Serial.print(",");
    if (po_enable) {
      Serial.print("---Activo---,"); //modo P
      Serial.print(po_speed);
    } else {
      Serial.print("---Desactivado---,"); // modo s
      Serial.print(velocidad);
    }
    Serial.print(",");
    Serial.print(v);
    Serial.print(",");
    Serial.print(i);
    Serial.println(",");

  }



  // Serial.print("Velocidad procesada: ");
  // Serial.println(velocidad);

  ms = map(velocidad, 0, 100, 1000, 2000);
  ESC.writeMicroseconds(ms);
  delay(15);

}
