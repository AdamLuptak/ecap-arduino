/*
 Ecap temperature controller

 Temperature controller with light weight web server client.

 created 8 Feb 2018
 by Adam Luptak
 modified 8 Apr 2018
 */
#include <SPI.h>
#include <Ethernet.h>
#include <Timer.h>
#include "Dispatcher.h"
#include <ArduinoJson.h>
#include <PID_v1.h>

#define PIN_INPUT 0
#define RELAY_PIN 13

double setPoint, Input, Output;

const int DEFAULT_SET_POINT = 0;
double kp = 2, ki = 5, kd = 1;
const int CONTROLLER_ACTION_INTERVAL = 1000;
PID pid(&Input, &Output, &setPoint, kp, ki, kd, DIRECT);

boolean isPidActive = false;
boolean isClientOnline = false;

int WindowSize = 5000;
unsigned long windowStartTime;

byte mac[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 1, 177);

EthernetServer server(80);

const char indexHtml[] PROGMEM = {"html"};

const char indexPath[] = "/ ";
const char controllerDataPath[] = "/controller-data";
const char pidPath[] = "/pid";
const char pidSetPointPath[] = "/pid/set-point";
const char pidActivatePath[] = "/pid/activate";


static const int REQUEST_LIMIT = 100;
char request[REQUEST_LIMIT];

Dispatcher dispatcher();

double tk1;
double tk2;
double tk3;

Timer protectionTimer;
Timer measurmentTimer;

void startEthernet();

void startSerial();

void securityEvaluation();

void measureTemperatures();

void handleEthernet();

void controlProcess();

String parseBody(const String &string);

String parseRestOperation(const String &string);

String parsePath(const String &httpRequest);

boolean isInRestPaths(String &path);

void clientPrintApplicationJsonHeader(EthernetClient &client);

void clientPrintHtmlHeader(EthernetClient &client);

void clientPrintIndexHtml(EthernetClient &client);

void setupPid();

EthernetClient &printControllerDataBody(EthernetClient &client);

void createPidJson(JsonObject &root);

EthernetClient &printPidBody(EthernetClient &client);

void setup() {
    startSerial();
    startEthernet();
    setupPid();
    //timer setup
    protectionTimer.every(10000, securityEvaluation);
    measurmentTimer.every(CONTROLLER_ACTION_INTERVAL, measureTemperatures);
}

void setupPid() {
    windowStartTime = millis();
    setPoint = DEFAULT_SET_POINT;
    pid.SetSampleTime(CONTROLLER_ACTION_INTERVAL);
    pid.SetOutputLimits(0, WindowSize);
    pid.SetMode(AUTOMATIC);
}

void securityEvaluation() {
    if (isClientOnline) {
        Serial.println("CLIENT IS CONNECTED OK");
        isClientOnline = false;
    } else {
        Serial.println("STOP EVERYTHING No client is connected for 15 minutes");
        isClientOnline = false;
    }
}

void measureTemperatures() {
    tk1 = 250.10;
    tk2 = 200.10;
    tk3 = 300.025;

//    Serial.print("tk1: ");
//    Serial.println(tk1);
//    Serial.print("tk2: ");
//    Serial.println(tk2);
//    Serial.print("tk3: ");
//    Serial.println(tk3);
}

void startSerial() {
    Serial.begin(9600);
    while (!Serial) { ;
    }
}

void startEthernet() {
    Ethernet.begin(mac, ip);
    server.begin();
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
}

void handleControllerActuator() {
    if (isPidActive && isClientOnline) {
        controlProcess();
    }else{
        pid.SetMode(MANUAL);
        digitalWrite(RELAY_PIN, LOW);
    }
}

void loop() {
    handleEthernet();
    protectionTimer.update();
    measurmentTimer.update();
    handleControllerActuator();
}

void controlProcess() {
    Input = analogRead(PIN_INPUT);
    pid.Compute();

    /************************************************
     * turn the output pin on/off based on pid output
     ************************************************/
    if (millis() - windowStartTime > WindowSize) { //time to shift the Relay Window
        windowStartTime += WindowSize;
    }
    if (Output < millis() - windowStartTime) digitalWrite(RELAY_PIN, HIGH);
    else digitalWrite(RELAY_PIN, LOW);
}


void processRequest(const char *pch1) {
    char body[100];
    strncpy(body, pch1 + sizeof(pidPath) - 1, strlen(pch1) - 14);
    char *pch;
    pch = strtok(body, "&");
    while (pch != NULL) {
        char keyname[32];
        char val[10];

        sscanf(pch, "%32[^=]=%s", keyname, val);
        Serial.println(val);
        if (strstr(keyname, "setPoint") != NULL) {
            setPoint = atof(val);
        } else if (strstr(keyname, "kp") != NULL) {
            kp = atof(val);
        } else if (strstr(keyname, "kd") != NULL) {
            kd = atof(val);
        } else if (strstr(keyname, "ki") != NULL) {
            ki = atof(val);
        } else if (strstr(keyname, "activate") != NULL) {
            isPidActive = strstr(val, "true") != NULL;
        }
        pch = strtok(NULL, "&");
    }
}

template<typename Type>
// this is the template parameter declaration
void printReponseSingleValue(EthernetClient &client, Type value, const char *key) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root[key] = value;
    root.printTo(client);
}

void printError404(EthernetClient &client) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("404 Not Found");
}

void cleanRequestBuffer() {
    for (int i = 0; i < sizeof(request); ++i)
        request[i] = (char) 0;
}

void handleEthernet() {
    EthernetClient client = server.available();
    if (client) {
//        Serial.println("new client");
        int index = 0;
        while (client.connected()) {

            if (client.available()) {
                char c = client.read();
                request[index++] = c;

                if (index > REQUEST_LIMIT) {
                    printError404(client);
                    break;
                }

                if (c == '\n') {
                    // you're starting a new line
                    char *pch = strstr(request, indexPath);
                    if (pch != NULL) {
                        // print response
                        Serial.println(indexPath);
                        clientPrintHtmlHeader(client);
                        client.println();
                        clientPrintIndexHtml(client);
                        client.println();
                        break;
                    }
                    pch = strstr(request, controllerDataPath);
                    if (pch != NULL) {
                        // print response
                        clientPrintApplicationJsonHeader(client);
                        client.println();
                        client = printControllerDataBody(client);
                        client.println();
                        break;
                    }
                    pch = strstr(request, pidPath);
                    if (pch != NULL && (pch[4] == '?' || pch[4] == ' ')) {
                        // handle request
                        processRequest(pch);
                        // print response
                        clientPrintApplicationJsonHeader(client);
                        client.println();
                        client = printPidBody(client);
                        client.println();
                        break;
                    }
                    pch = strstr(request, pidSetPointPath);
                    if (pch != NULL) {
                        // handle request
                        processRequest(pch);
                        // print response
                        clientPrintApplicationJsonHeader(client);
                        client.println();
                        printReponseSingleValue(client, setPoint, "setPoint");
                        client.println();
                        break;
                    }
                    pch = strstr(request, pidActivatePath);
                    if (pch != NULL) {
                        // handle request
                        processRequest(pch);
                        //print response
                        clientPrintApplicationJsonHeader(client);
                        client.println();
                        printReponseSingleValue(client, isPidActive, "activate");
                        client.println();
                        break;
                    }

                    printError404(client);
                    break;

                }
            }
        }
        cleanRequestBuffer();
        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        isClientOnline = true;
//        Serial.println("client disconnected");
    }
}

EthernetClient &printPidBody(EthernetClient &client) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    createPidJson(root);
    root.printTo(client);
    return client;
}

EthernetClient &printControllerDataBody(EthernetClient &client) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    JsonArray &array = root.createNestedArray("temperatures");
    for (int i = 0; i < 3; ++i) {
        JsonObject &temperature = jsonBuffer.createObject();
        switch (i) {
            case 0:
                temperature["name"] = "tk1";
                temperature["value"] = tk1;
                break;
            case 1:
                temperature["name"] = "tk2";
                temperature["value"] = tk2;
                break;
            case 2:
                temperature["name"] = "tk3";
                temperature["value"] = tk3;
                break;
            default:
                break;
        }
        array.add(temperature);
    }
    createPidJson(root);
    root.printTo(client);
    return client;
}

void createPidJson(JsonObject &root) {
    JsonObject &pidJson = root.createNestedObject("pid");
    pidJson["setPoint"] = setPoint;
    pidJson["kp"] = kp;
    pidJson["activate"] = isPidActive;
    pidJson["kd"] = kd;
    pidJson["ki"] = ki;
}


void clientPrintIndexHtml(EthernetClient &client) {
    for (unsigned int k = 0; k < strlen_P(indexHtml); k++) {
        char c = pgm_read_byte_near(indexHtml + k);
        client.print(c);
    }
}

void clientPrintHtmlHeader(EthernetClient &client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(
            "Connection: close");
}

void clientPrintApplicationJsonHeader(EthernetClient &client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Access-Control-Allow-Origin:*");
    client.println("connection: keep-alive");
    client.println("content-type: application/json");
    client.println("Access-Control-Allow-Headers :Origin, X-Requested-With, Content-Type, Accept");
};
