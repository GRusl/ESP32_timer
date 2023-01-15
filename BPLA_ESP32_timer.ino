#include <ArduinoOTA.h>

#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>

#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

#include <EEPROM.h>

#include <ESP32_Servo.h>

#define usi unsigned short int
#define usl unsigned long int

/* Системные переменные */
usi programm_status = 0;
usl work_time, real_time;
bool end_work = false;

/* Настройка серво */
const usi SERVO_COUNT = 3;  // Число подключенных серво
// Порты подключенных серво (кол-во должно совпадать с SERVO_COUNT)
const usi SERVO_PINS[SERVO_COUNT] = {2, 4, 5};
// Начальные положения подключенных серво (кол-во должно совпадать с SERVO_COUNT)
const usi SERVO_START_DEG[SERVO_COUNT] = {0};

Servo servo_arr[SERVO_COUNT];  // Массив серво
usi servo_data[2][SERVO_COUNT] = {{0, 0}};

/* Настройка сети/сервера */
const char* WIFI_NAME = "ESP_TIMER";  // Название сети
const char* WIFI_PASSWORD = "12345678";  // Код доступа к сети (минимум 8 символов)
const usi PORT = 8000;  // Порт сервера

AsyncWebServer server(PORT);  // Порт сервера


/* Функции */
// Обьеденение списка arr (длинны SERVO_COUNT) в вид строки: "[v_0, v_1, ..., v_n]"
String join_servo_arr(usi arr[SERVO_COUNT])
{
  String out = "[";
  for (usi i = 0; i < SERVO_COUNT; i++)
  {
    out += String(arr[i]);
    if (i + 1 != SERVO_COUNT)
    {
      out += ", ";
    }
  }
  return out + "]";
}


/* Настройка контекст-процессоров */
String processor_index(const String& var)  // Для главной страницы
{
  String out = "";
  if (var == "PINS")
  {
    // Обьеденение списка SERVO_PINS в вид строки: "[pin_1, pin_2, ..., pin_n]"
    String out = "[";
    for (usi i = 0; i < SERVO_COUNT; i++)
    {
      out += String(SERVO_PINS[i]);
      if (i + 1 != SERVO_COUNT)
      {
        out += ", ";
      }
    }
    return out + "]";
  } else if (var == "DEGS")
  {
    return join_servo_arr(servo_data[0]);
  } else if (var == "TIMES")
  {
    return join_servo_arr(servo_data[1]);
  }

  return "NOT_DATA";
}


/* Настройка обработчиков */
void setup_handlers()
{
  server.on(  // Главная страница
    "/",  // Адрес
    HTTP_GET,  // Обрабатываемый тип запроса
    [](AsyncWebServerRequest *request)
    {  // Скрипт ответа
      request -> send(SPIFFS, "/index.html", "text/html", false, processor_index);
    }
  );

  server.on(  // Обработка формы страта
    "/start",  // Адрес
    HTTP_GET,  // Обрабатываемый тип запроса
    [](AsyncWebServerRequest *request)
    {  // Скрипт ответа
      for (usi i = 0; i < SERVO_COUNT; i++)
      {
        for (usi j = 0; j < 2; j++)
        {
          // Генерация имени обрабатываемого параметра
          String param_name = String(i) + "_" + ((j == 0)?"deg":"time");
          if (request -> hasParam(param_name))  // Если существует такой параметр
          {
            // Запись значений в память
            servo_data[j][i] = (request -> getParam(param_name) -> value()).toInt();
          }
        }
      }

      work_time = millis();
      programm_status = 1;  // Перевод программы в режим ожидания отжатия кнопки
      
      request -> redirect("/");  // Редирект на главную страницу
    }
  );
}


/* Инициализация программы */
void setup()
{
  Serial.begin(115200);

  // Проверка файловой системы на доступность (и инициализация файловой системы)
  if (!SPIFFS.begin())
  {
    // Сбой
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  /* Инициализация серво */
  for (usi i = 0; i < SERVO_COUNT; i++)
  {
    servo_arr[i].attach(SERVO_PINS[i]);  // Инициализация порта сервопривода
    servo_arr[i].write(SERVO_START_DEG[i]);  // Установка в начальное положение
  }

  /* Инициализация сети */
  WiFi.softAP(WIFI_NAME, WIFI_PASSWORD);  // Установка имени и кода доступа к сети
  WiFi.begin();  // Инициализация сети

  /* Вывод адреса для подключения */
  IPAddress IP = WiFi.softAPIP();  // Получение активного IP
  // Вывод данных
  Serial.print("Address: http://"); 
  Serial.print(IP);
  Serial.print(":");
  Serial.println(PORT);

  /* Инициализация сервера */
  setup_handlers();
  server.begin();
}


/* Цикл программы */
void loop()
{
  real_time = (millis() - work_time) / 1000;
  if (programm_status == 1) 
  {
    end_work = true;
    for (usi i = 0; i < SERVO_COUNT; i++)
    {
      if (real_time >= servo_data[1][i])
      {
        // Перевод серво в рабочее состояние
        servo_arr[i].write(servo_data[0][i]);
      } else 
      {
        // Один из сервоприводов еще не завершил свою работу
        end_work = false;
      }
    }

    if (end_work)  // Если все сервоприводы отработали
    {
      programm_status = 0;  // Программа переходит в режим ожидания команд
      real_time = millis();
    }
  } else if (real_time >= 20 && end_work)  // Если прошло 20с после завершения работы
  {
    end_work = false;
    // Установка серво в начальное положение
    for (usi i = 0; i < SERVO_COUNT; i++)
    {
      servo_arr[i].write(SERVO_START_DEG[i]);
    }
  }
}
