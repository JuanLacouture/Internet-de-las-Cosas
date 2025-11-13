#ifndef CONFIG_H
#define CONFIG_H

// ========================= AP (sin Internet) =========================
#define AP_SSID      "Equipo3-ESP32"
#define AP_PASS      "123456789"
#define AP_CH        6
#define AP_MAX_CONN  4

// ========================= Pines L298N ===============================
#define IN1_PIN 25
#define IN2_PIN 26
#define IN3_PIN 32
#define IN4_PIN 33
#define ENA_PIN 27
#define ENB_PIN 13

// ========================= PWM ==============================
#define PWM_FREQ       2000
#define PWM_RESOLUTION 8

// ========================= MQTT =============================
#define MQTT_BROKER    "test.mosquitto.org"
#define MQTT_PORT      8883
#define MQTT_TOPIC     "carro/movimiento"
#define MQTT_RETRY_MS  5000

// ========================= Sensor Ultrasonido =============================
#define TRIG_PIN 4
#define ECHO_PIN 2
#define MQTT_TOPIC_DIST "carro/distancia"   // nuevo tópico de publicación
#define DIST_INTERVAL_MS 5000               // intervalo de lectura (5 s)

#endif
