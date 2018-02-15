/*
 Ecap temperature controller

 Temperature controller with light weight web server client.

 created 8 Feb 2018
 by Adam Luptak
 modified 8 Apr 2018
 */

#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>
#include <Timer.h>
#include <stdio.h>
#include "Dispatcher.h"
#include <ArduinoJson.h>
#include <PID_v1.h>

#define PIN_INPUT 0
#define RELAY_PIN 13

double setPoint, Input, Output;

boolean isClientOnline = false;
double kp = 2, ki = 5, kd = 1;
PID pid(&Input, &Output, &setPoint, kp, ki, kd, DIRECT);

boolean activatePid = false;

int WindowSize = 5000;
unsigned long windowStartTime;

byte mac[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 1, 177);

EthernetServer server(80);

const char indexHtml[] PROGMEM = {"<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"><meta charset=\"UTF-8\"><style type=\"text/css\">body{text-align:center;opacity:0}.control,button,input{margin:0 0 20px 0}</style></head><body><div><h1 id=\"header\">ECAP Temperature Controller</h1><h3 id=\"controller-state\">#</h3><h3 id=\"tk3\">#</h3><h3 id=\"tk2\">#</h3><h2 id=\"tk1\">#</h2><h2 id=\"set-point\">#</h2></div><div id=\"control\"><form onsubmit=\"changeSetPoint();return false;\"><input type=\"text\" id=\"set-point-input\" value=\"Change Setpoint\" pattern=\"[0-9]|[1-8][0-9]|9[0-9]|[1-4][0-9]{2}|500\" title=\"Only numbers <0,500>\"><br><input type=\"submit\" value=\"Change SetPoint\"></form><button id=\"startButton\" onclick=\"toggleRegulator(true)\">Start regulator</button> <button id=\"stopButton\" onclick=\"toggleRegulator(false)\">Stop regulator</button><br><button onclick=\"saveDataToExcel()\">Save to excel</button><br><input type=\"checkbox\" id=\"save-to-excel-after-close\" checked=\"checked\">Save to excel after close<br></div><script type=\"text/javascript\">\"use strict\";var CONTROLLER_BASE_URL=\"http://192.168.1.177\",CONTROLLER_URL=CONTROLLER_BASE_URL+\"/controller-data\",PID_URL=CONTROLLER_BASE_URL+\"/pid\",SET_POINT_URL=CONTROLLER_BASE_URL+\"/pid/set-point\",ACTIVATE_URL=CONTROLLER_BASE_URL+\"/pid/activate\",FETCH_CONTROLLER_DATA_INTREVAL=1e3,temperatures=[],HEADER=\"tk1,tk2,tk3\\n\",TEMPERATURES_KEY=\"temperatures\",LOCAL_STORAGE_LIMIT=2e5;function changeSetPoint(){console.log(\"Start Ecap Controller\");var e=new XMLHttpRequest;e.open(\"POST\",SET_POINT_URL),e.setRequestHeader(\"Content-Type\",\"application/json\"),e.onload=function(){200===e.status?console.log(\"Set point udpated\"):200!==e.status&&console.log(\"An error occurred during the transaction POST new setPoint\")};var t=document.getElementById(\"set-point-input\").value;e.send(JSON.stringify({setPoint:t}))}function saveDataToExcel(){console.log(\"Saving data to excel\");var e=window.document.createElement(\"a\"),t=JSON.parse(localStorage.getItem(TEMPERATURES_KEY));t=HEADER+t.join(\"\\n\");var n=new Blob([t],{type:\"text/csv\"});e.href=window.URL.createObjectURL(n),e.download=\"measurement.csv\",document.body.appendChild(e),e.click()}function confirmExit(){document.getElementById(\"save-to-excel-after-close\").checked&&saveDataToExcel()}function fetchControllerData(){console.log(\"Fetching data from: \"+CONTROLLER_URL);var e=new XMLHttpRequest;e.onload=function(e){var t=e.target.response;updateHtmlData(t),saveData(t)},e.onerror=function(){document.getElementById(\"header\").innerHTML=\"An error occurred during the transaction<br>GET:\"+CONTROLLER_URL},e.open(\"GET\",CONTROLLER_URL,!0),e.setRequestHeader(\"Accept\",\"application/json\"),e.responseType=\"json\",e.send()}function saveData(e){for(var t=e.temperatures,n=\"\",o=0;o<t.length;o++)n=n.concat(t[o].value),o<t.length-1&&(n=n.concat(\",\"));temperatures.push(n),upsertDataLocalStorage(n)}function toggleRegulator(e){console.log(e),e&&localStorage.removeItem(TEMPERATURES_KEY);var t=new XMLHttpRequest;t.open(\"POST\",ACTIVATE_URL),t.setRequestHeader(\"Content-Type\",\"application/json\"),t.setRequestHeader(\"Cache-Control\",\"no-cache\"),t.onload=function(){200===t.status?console.log(\"Regulator state change to: \"+e):200!==t.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},t.send(JSON.stringify({activate:e}))}function upsertDataLocalStorage(e){if(TEMPERATURES_KEY in localStorage){var t=localStorage.getItem(TEMPERATURES_KEY),n=JSON.parse(t);n.length>=LOCAL_STORAGE_LIMIT&&(localStorage.removeItem(TEMPERATURES_KEY),n=[]),n.push(e),localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(n))}else{var o=Array(1).fill(e);localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(o))}}function updateHtmlData(e){e.pid.activate?(document.getElementById(\"startButton\").style.display=\"none\",document.getElementById(\"stopButton\").style.display=\"inline\"):(document.getElementById(\"stopButton\").style.display=\"none\",document.getElementById(\"startButton\").style.display=\"inline\"),document.getElementById(\"controller-state\").innerHTML=\" STATE: \"+(e.pid.activate?\"ON\":\"OFF\"),document.getElementById(\"header\").innerHTML=\"ECAP Temperature Controller\",document.getElementById(\"set-point\").innerHTML=\"Setpoint: \"+e.pid.setPoint+\"°C\";for(var t=e.temperatures,n=0;n<t.length;n++){var o=t[n];document.getElementById(o.name).innerHTML=o.name+\": \"+o.value+\"°C\"}}window.onload=function(){setTimeout(function(){document.body.style.opacity=\"100\"},1e3)},window.onbeforeunload=confirmExit;var fetchControllerDataTimer=setInterval(fetchControllerData,FETCH_CONTROLLER_DATA_INTREVAL);</script></body></html>"};
char myChar;

String restPaths[5] = {
        "/",
        "/controller-data",
        "/pid",
        "/pid/set-point",
        "/pid/activate"
};

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

    protectionTimer.every(10000, securityEvaluation);
    measurmentTimer.every(1000, measureTemperatures);
}

void setupPid() {
    windowStartTime = millis();
    setPoint = 100;
    pid.SetOutputLimits(0, WindowSize);
    pid.SetMode(AUTOMATIC);
}

void securityEvaluation() {
    if (isClientOnline) {
        Serial.println("Client is connected");
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

    Serial.print("tk1: ");
    Serial.println(tk1);
    Serial.print("tk2: ");
    Serial.println(tk2);
    Serial.print("tk3: ");
    Serial.println(tk3);
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

void loop() {
    handleEthernet();
//    protectionTimer.update();
    measurmentTimer.update();
    controlProcess();
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

void handleEthernet() {
    EthernetClient client = server.available();
    if (client) {
        Serial.println("new client");
        // an http request ends with a blank line
        boolean currentLineIsBlank = true;
        String httpRequest = "";
        while (client.connected()) {
            int lineBreakCount = 0;
            if (client.available()) {
                char c = client.read();
                httpRequest.concat(c);
                Serial.write(c);
                if (c == '\n' && currentLineIsBlank) {
                    while (client.available()) {
                        char c = client.read();
                        httpRequest.concat(c);
                        Serial.write(c);
                    }
                    break;
                }
                if (c == '\n') {
                    // you're starting a new line
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                }
            }

        }

        String restOperation = parseRestOperation(httpRequest);
        Serial.println(restOperation);

        String body = parseBody(httpRequest);
        Serial.println(body);

        String path = parsePath(httpRequest);
        path.trim();
        Serial.println(path);

        if (isInRestPaths(path)) {
            if (restPaths[0].compareTo(path) == 0) {
                Serial.println(restPaths[0]);
                clientPrintHtmlHeader(client);
                client.println();
                clientPrintIndexHtml(client);
                client.println();
            } else if (restPaths[1].compareTo(path) == 0) {
                Serial.println(restPaths[1]);
                clientPrintApplicationJsonHeader(client);
                client.println();
                client = printControllerDataBody(client);
                client.println();
            } else if (restPaths[2].compareTo(path) == 0) {
                Serial.println(restPaths[2]);
                // handle request
                StaticJsonBuffer<500> jsonBuffer;
                JsonObject &requestBody = jsonBuffer.parseObject(body);
                if (requestBody.containsKey("setPoint")) {
                    setPoint = requestBody["setPoint"];
                }
                if (requestBody.containsKey("kp")) {
                    kp = requestBody["kp"];
                }
                if (requestBody.containsKey("ki")) {
                    ki = requestBody["ki"];
                }
                if (requestBody.containsKey("kd")) {
                    kd = requestBody["kd"];
                }
                if (requestBody.containsKey("activate")) {
                    activatePid = requestBody["activate"];
                }
                // print response
                clientPrintApplicationJsonHeader(client);
                client.println();
                client = printPidBody(client);
                client.println();
            } else if (restPaths[3].compareTo(path) == 0) {
                Serial.println(restPaths[3]);
                // handle request
                StaticJsonBuffer<500> jsonBuffer;
                JsonObject &requestBody = jsonBuffer.parseObject(body);
                setPoint = requestBody["setPoint"];
                // print response
                clientPrintApplicationJsonHeader(client);
                client.println();
                JsonObject &root = jsonBuffer.createObject();
                root["setPoint"] = setPoint;
                root.printTo(client);
                client.println();
            } else if (restPaths[4].compareTo(path) == 0) {
                Serial.println(restPaths[4]);
                // handle request
                StaticJsonBuffer<500> jsonBuffer;
                JsonObject &requestBody = jsonBuffer.parseObject(body);
                activatePid = requestBody["activate"];
                // print response
                clientPrintApplicationJsonHeader(client);
                client.println();
                JsonObject &root = jsonBuffer.createObject();
                root["activate"] = activatePid;
                root.printTo(client);
                client.println();
            }
        } else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.println("404 Not Found");
        }

        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        isClientOnline = true;
        Serial.println("client disconnected");
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
    pidJson["activate"] = activatePid;
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

boolean isInRestPaths(String &path) {
    for (int i = 0; i < sizeof(restPaths); ++i) {
        if (restPaths[i].compareTo(path) == 0) {
            return true;
        }
    }
    return false;
}

String parsePath(const String &httpRequest) {
    const String &path = httpRequest.substring(httpRequest.indexOf(' ') + 1, httpRequest.indexOf('H'));
    return path;
}

String parseRestOperation(const String &string) {
    const String &restOperation = string.substring(0, string.indexOf(' '));
    return restOperation;
}

String parseBody(const String &string) {
    const String &substring = string.substring(string.indexOf("{"), string.lastIndexOf('}') + 1);
    return substring;
}
