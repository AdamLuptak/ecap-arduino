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
#include <ArduinoJson.h>
#include <PID_v1.h>
#include <avr/wdt.h>

#define RELAY_PIN 13
int outputs[] = {13, 21, 20, 19};

// Pid setup
double kp = 2, ki = 5, kd = 1;
double setPoint, Input, Output;
PID pid(&Input, &Output, &setPoint, kp, ki, kd, DIRECT);
boolean isPidActive = false;

enum Actuator {
    RELAY,
    HW_TIMMER
};

Actuator actuator = HW_TIMMER;

// HW timer actuator
long maxPowerInterval = 400;
int volatile power = 1;
int volatile timeCounter = 0;

// Relay actuator
int WindowSize = maxPowerInterval;
unsigned long windowStartTime;

// Ethernet setup
byte mac[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
boolean isClientOnline = false;
IPAddress ip(192, 168, 1, 177);
EthernetServer server(80);

const char indexHtml[] PROGMEM = {
        "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style type=\"text/css\">body{text-align:center;opacity:0;font-size:calc(.75em + 1vmin)}@media screen and (max-width:800px){button,input{padding:15px 32px}}.control,button,input{margin:0 0 20px 0}</style></head><body><div class=\"square\"><div class=\"content\"><div><h1 id=\"header\">ECAP Temperature Controller</h1><h3 id=\"controller-state\">#</h3><h3 id=\"tk3\">#</h3><h3 id=\"tk2\">#</h3><h2 id=\"tk1\">#</h2><h2 id=\"set-point\">#</h2></div><div id=\"control\"><form onsubmit=\"changeSetPoint();return false;\"><input type=\"text\" id=\"set-point-input\" value=\"Change Setpoint\" pattern=\"[0-9]|[1-8][0-9]|9[0-9]|[1-4][0-9]{2}|500\" title=\"Only numbers <0,500>\"><br><input type=\"submit\" value=\"Change SetPoint\"></form><button id=\"startButton\" onclick=\"toggleRegulator(true)\">Start regulator</button> <button id=\"stopButton\" onclick=\"toggleRegulator(false)\">Stop regulator</button><br><button onclick=\"saveDataToExcel()\">Save to excel</button><br><input type=\"checkbox\" id=\"save-to-excel-after-close\" checked=\"checked\">Save to excel after close<br><div id=\"pid-setup\"><form onsubmit=\"changePid(event);return false;\"><label id=\"kp\">kp: #</label><span><input name=\"kp\" id=\"kp\" type=\"text\" value=\"new kp\" pattern=\"\\d.{0,1}\\d{0,1}\" title=\"only numbers\"></span><br><label id=\"ki\">ki: #</label><span><input name=\"ki\" id=\"ki\" type=\"text\" value=\"new ki\" pattern=\"\\d.{0,1}\\d{0,1}\" title=\"only numbers\"></span><br><label id=\"kd\">kd: #</label><span><input name=\"kd\" id=\"kd\" type=\"text\" value=\"new kd\" pattern=\"\\d.{0,1}\\d{0,1}\" title=\"only numbers\"></span><br><input type=\"submit\" value=\"Change Pid\"></form></div></div></div></div><script type=\"text/javascript\">\"use strict\";var CONTROLLER_BASE_URL=\"http://192.168.1.177\",CONTROLLER_URL=CONTROLLER_BASE_URL+\"/controller-data\",PID_URL=CONTROLLER_BASE_URL+\"/pid\",SET_POINT_URL=CONTROLLER_BASE_URL+\"/pid/set-point\",ACTIVATE_URL=CONTROLLER_BASE_URL+\"/pid/activate\",FETCH_CONTROLLER_DATA_INTREVAL=1e3,temperatures=[],HEADER=\"tk1,tk2,tk3\\n\",TEMPERATURES_KEY=\"temperatures\",LOCAL_STORAGE_LIMIT=2e5;function changeSetPoint(){console.log(\"Start Ecap Controller\");var e=new XMLHttpRequest,t=document.getElementById(\"set-point-input\").value;e.open(\"POST\",SET_POINT_URL+\"?setPoint=\"+t),e.setRequestHeader(\"Content-Type\",\"application/json\"),e.onload=function(){200===e.status?console.log(\"Set point udpated\"):200!==e.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},e.send()}function changePid(e){var t=\"?\";t=(t=(t=t+\"kp=\"+e.currentTarget[0].value+\"&\")+\"ki=\"+e.currentTarget[1].value+\"&\")+\"kd=\"+e.currentTarget[2].value;var n=new XMLHttpRequest;n.open(\"POST\",PID_URL+t),n.setRequestHeader(\"Content-Type\",\"application/json\"),n.onload=function(){200===n.status?console.log(\"Pid change\"):200!==n.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},n.send()}function saveDataToExcel(){console.log(\"Saving data to excel\");var e=window.document.createElement(\"a\"),t=JSON.parse(localStorage.getItem(TEMPERATURES_KEY));t=HEADER+t.join(\"\\n\");var n=new Blob([t],{type:\"text/csv\"});e.href=window.URL.createObjectURL(n),e.download=\"measurement.csv\",document.body.appendChild(e),e.click()}function confirmExit(){document.getElementById(\"save-to-excel-after-close\").checked&&saveDataToExcel()}function fetchControllerData(){console.log(\"Fetching data from: \"+CONTROLLER_URL);var e=new XMLHttpRequest;e.onload=function(e){var t=e.target.response;updateHtmlData(t),saveData(t)},e.onerror=function(){document.getElementById(\"header\").innerHTML=\"An error occurred during the transaction<br>GET:\"+CONTROLLER_URL},e.open(\"GET\",CONTROLLER_URL,!0),e.setRequestHeader(\"Accept\",\"application/json\"),e.responseType=\"json\",e.send()}function saveData(e){for(var t=e.temperatures,n=\"\",o=0;o<t.length;o++)n=n.concat(t[o].value),o<t.length-1&&(n=n.concat(\",\"));temperatures.push(n),upsertDataLocalStorage(n)}function toggleRegulator(e){console.log(e),e&&localStorage.removeItem(TEMPERATURES_KEY);var t=new XMLHttpRequest;t.open(\"POST\",ACTIVATE_URL+\"?activate=\"+e),t.onload=function(){200===t.status?console.log(\"Regulator state change to: \"+e):200!==t.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},t.send(JSON.stringify({activate:e}))}function upsertDataLocalStorage(e){if(TEMPERATURES_KEY in localStorage){var t=localStorage.getItem(TEMPERATURES_KEY),n=JSON.parse(t);n.length>=LOCAL_STORAGE_LIMIT&&(localStorage.removeItem(TEMPERATURES_KEY),n=[]),n.push(e),localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(n))}else{var o=Array(1).fill(e);localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(o))}}function updateHtmlData(e){e.pid.activate?(document.getElementById(\"startButton\").style.display=\"none\",document.getElementById(\"stopButton\").style.display=\"inline\"):(document.getElementById(\"stopButton\").style.display=\"none\",document.getElementById(\"startButton\").style.display=\"inline\"),document.getElementById(\"controller-state\").innerHTML=\" STATE: \"+(e.pid.activate?\"ON\":\"OFF\"),document.getElementById(\"header\").innerHTML=\"ECAP Temperature Controller\",document.getElementById(\"set-point\").innerHTML=\"Setpoint: \"+e.pid.setPoint+\"°C\",document.getElementById(\"kp\").innerHTML=\"kp: \"+e.pid.kp,document.getElementById(\"ki\").innerHTML=\"ki: \"+e.pid.ki,document.getElementById(\"kd\").innerHTML=\"kd: \"+e.pid.kd;for(var t=e.temperatures,n=0;n<t.length;n++){var o=t[n];document.getElementById(o.name).innerHTML=o.name+\": \"+o.value+\"°C\"}}window.onload=function(){setTimeout(function(){document.body.style.opacity=\"100\"},1e3)},window.onbeforeunload=confirmExit;var fetchControllerDataTimer=setInterval(fetchControllerData,FETCH_CONTROLLER_DATA_INTREVAL);</script></body></html>"};

const char indexPath[] = "/ ";
const char controllerDataPath[] = "/controller-data";
const char pidPath[] = "/pid";
const char pidSetPointPath[] = "/pid/set-point";
const char pidActivatePath[] = "/pid/activate";

static const int REQUEST_LIMIT = 100;
const int CONTROLLER_ACTION_INTERVAL = 1000;
char request[REQUEST_LIMIT];

// Serial setup
String requestFromClient = "";
boolean isRequestCompleted = false;

double tk1;
double tk2;
double tk3;

Timer measurmentTimer;

void setupEthernet();

void setupSerial();

void securityEvaluation();

void measureTemperatures();

void handleEthernet();

void handleRequest();

void controlProcess();

void clientPrintApplicationJsonHeader(EthernetClient &client);

void clientPrintHtmlHeader(EthernetClient &client);

void clientPrintIndexHtml(EthernetClient &client);

void setupPid();

EthernetClient &printControllerDataBody(EthernetClient &client);

void createPidJson(JsonObject &root);

EthernetClient &printPidBody(EthernetClient &client);

void setupHwTimer();

void setupOutputs(int action);

void handleComunication();

void handleRelayOutput();

void turnOffController();

void setupOutputs(int action) {
    for (int i = 0; i < sizeof(outputs) / sizeof(int *); ++i) {
        switch (action) {
            case HIGH:
                digitalWrite(outputs[i], HIGH);
                break;
            case LOW:
                digitalWrite(outputs[i], LOW);
                break;
            default:
                pinMode(outputs[i], OUTPUT);
                break;
        }
    }
}

void setupHwTimer() {// initialize Timer1
    noInterrupts(); // disable all interrupts
    //set timer1 interrupt at 1Hz
    TCCR1A = 0;// set entire TCCR1A register to 0
    TCCR1B = 0;// same for TCCR1B
    TCNT1 = 0;//initialize counter value to 0
    // set compare match register for 1hz increments
    OCR1A = 14.625;// = (16*10^6) / (1000*1024) - 1 (must be <65536) every millisecond
    // turn on CTC mode
    TCCR1B |= (1 << WGM12);
    // Set CS10 and CS12 bits for 1024 prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);
    // enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    interrupts(); // enable all interrupts
}

ISR(TIMER1_COMPA_vect) {
    if (power > 0 && timeCounter <= power && actuator == HW_TIMMER) {
        setupOutputs(HIGH);
    } else if (actuator == HW_TIMMER) {
        setupOutputs(LOW);
    }
    timeCounter = timeCounter == maxPowerInterval ? 0 : timeCounter + 1;
}

void setupPid() {
    windowStartTime = millis();
    setPoint = 0;
    pid.SetOutputLimits(0, WindowSize);
    pid.SetMode(AUTOMATIC);
}

void measureTemperatures() {
    tk1 = analogRead(0);
    tk2 = 200.10;
    tk3 = 300.025;

//    Serial.print("tk1: ");
//    Serial.println(tk1);
//    Serial.print("tk2: ");
//    Serial.println(tk2);
//    Serial.print("tk3: ");
//    Serial.println(tk3);
}

void setupSerial() {
    requestFromClient.reserve(50);
    Serial.begin(9600);
    while (!Serial) { ;
    }
}

void setupEthernet() {
    Ethernet.begin(mac, ip);
    server.begin();
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
}

void controlProcess() {
    Input = tk1;
    pid.Compute();
    /************************************************
     * turn the output pin on/off based on pid output
     ************************************************/
    switch (actuator) {
        case RELAY:
            handleRelayOutput();
            break;
        case HW_TIMMER:
            power = Output;
            // this is HW timer used for triggering the output
            break;
    }
}

void handleRelayOutput() {
    if (millis() - windowStartTime >= WindowSize) { //time to shift the Relay Window
        windowStartTime += WindowSize;
    }
    if (Output > millis() - windowStartTime) {
        setupOutputs(HIGH);
    } else {
        setupOutputs(LOW);
    }
}


void handleRequest() {
    if (isRequestCompleted) {
        Serial.println(requestFromClient);

        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &requestFromClientJson = jsonBuffer.parseObject(requestFromClient);

        if (requestFromClientJson.success()) {
            int32_t tempPower = requestFromClientJson["power"];
            if (tempPower <= 100 && tempPower >= 0) {
                Serial.print("Power from client change to: ");
                power = tempPower;
            }
        } else {
            Serial.print("Power from client will not be change can't parse request ");
        }

        requestFromClient = "";
        Serial.println(power);
        isRequestCompleted = false;
    }
}

void serialEvent() {
    while (Serial.available()) {
        char inChar = (char) Serial.read();
        requestFromClient += inChar;

        if (inChar == '\n') {
            isRequestCompleted = true;
        }
    }
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
        wdt_reset();
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
        // if no client is online it will be restarted
        wdt_reset();
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

void handleComunication() {
    handleRequest();
    handleEthernet();
}

void turnOffController() {
    pid.SetMode(MANUAL);
    setupOutputs(LOW);
    power = 0;
}

void setup() {
    //setup watchdog
    wdt_enable(WDTO_8S);
    setupSerial();
    setupEthernet();
    setupPid();
    //timer setup
    setupHwTimer();
    measurmentTimer.every(CONTROLLER_ACTION_INTERVAL, measureTemperatures);
    int pinMode = 2;
    setupOutputs(pinMode);
    setupOutputs(LOW);
}

boolean prevIsActive = false;

void loop() {
    handleComunication();
    //Measure process values
    measurmentTimer.update();
    //PID controller process
    if (isPidActive) {
        if (pid.GetMode() == MANUAL) {
            Serial.println("Setting automatic mode");
            pid.SetMode(AUTOMATIC);
        }
        controlProcess();
    } else {
        turnOffController();
    }
}



