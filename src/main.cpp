/*
Ecap temperature controller

Temperature controller with light weight web server client.

created 8 Feb 2018
by Adam Luptak
modified 18 March 2018

Table with PID values obtain from experiments
+----------------------------------+------+------------------------+----------+----------------+--+
|              range               |  kp  |           ki           |    kd    | minOutput[ms]  |  |
+----------------------------------+------+------------------------+----------+----------------+--+
| 0-149                            | 2.4  | 4                      |        0 |              0 |  |
| 150-200                          | 2.4  | 6                      |        0 |            100 |  |
| 200-250                          | 2.4  | 6                      |        0 |            200 |  |
| 250-300                          | 15   | 30                     |        0 |            300 |  |
| 330-350                          | 15   | 300                    |        0 |            350 |  |
+----------------------------------+------+------------------------+----------+----------------+--+
 */

#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Timer.h>
#include <ArduinoJson.h>
#include <PID_v1.h>
#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>

#define RELAY_PIN 13

// Pid setup
double kp = 2.4, ki = 2, kd = 0;
double setPoint, Input, Output;
PID pid(&Input, &Output, &setPoint, kp, ki, kd, DIRECT);
boolean isPidActive = false;
boolean isPidInAutomaticMode = false;
double minOutput = 0;
double predictTemperature = 0;

enum Actuator {
    RELAY,
    HW_TIMMER
};

Actuator actuator = HW_TIMMER;

// HW timer actuator
long maxPowerInterval = 1000;
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

const char indexHtml[] PROGMEM = {"<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style type=\"text/css\">body{text-align:center;opacity:0;font-size:calc(.75em + 1vmin)}@media screen and (max-width:800px){button,input{padding:15px 32px}}@media only screen and (min-device-width:768px) and (max-device-width:1024px){button,input{padding:15px 32px}}.control,button,input{margin:0 0 20px 0}.hide{display:none}</style></head><body><div class=\"square\"><div class=\"content\"><div><h1 id=\"header\">ECAP Temperature Controller</h1><h3 id=\"controller-state\">#</h3><h3 id=\"tk3\">#</h3><h3 id=\"tk2\">#</h3><h2 id=\"tk1\">#</h2><h2 id=\"set-point\">#</h2></div><div id=\"control\"><form onsubmit=\"changeSetPoint();return false;\"><input type=\"text\" id=\"set-point-input\" value=\"Change Setpoint\" pattern=\"[0-9]|[1-8][0-9]|9[0-9]|[1-4][0-9]{2}|400\" title=\"Only numbers <0,400>\"><br><input type=\"submit\" value=\"Change SetPoint\"></form><button id=\"startButton\" onclick=\"toggleRegulator(true)\">Start regulator</button> <button id=\"stopButton\" onclick=\"toggleRegulator(false)\">Stop regulator</button><br><button onclick=\"saveDataToExcel()\">Save to excel</button><br><input type=\"checkbox\" id=\"save-to-excel-after-close\" hidden>Save to excel after close<br><div id=\"pid-setup\"><form onsubmit=\"changePid(event);return false;\"><label id=\"kp\">kp: #</label><span><input name=\"kp\" id=\"kp\" type=\"text\" value=\"new kp\" pattern=\"^\\d*\\.?\\d*$\" title=\"only numbers\"></span><br><label id=\"ki\">ki: #</label><span><input name=\"ki\" id=\"ki\" type=\"text\" value=\"new ki\" pattern=\"^\\d*\\.?\\d*$\" title=\"only numbers\"></span><br><label id=\"kd\">kd: #</label><span><input name=\"kd\" id=\"kd\" type=\"text\" value=\"new kd\" pattern=\"^\\d*\\.?\\d*$\" title=\"only numbers\"></span><br><label id=\"min-output\">min-output: #</label><span><input name=\"min-output\" id=\"min-output\" type=\"text\" value=\"new min-output\" pattern=\"^\\d*\\.?\\d*$\" title=\"only numbers\"></span><br><input type=\"submit\" value=\"Change Pid\"></form></div></div></div></div><script type=\"text/javascript\">\"use strict\";var CONTROLLER_BASE_URL=\"http://192.168.1.177\",CONTROLLER_URL=CONTROLLER_BASE_URL+\"/controller-data\",PID_URL=CONTROLLER_BASE_URL+\"/pid\",SET_POINT_URL=CONTROLLER_BASE_URL+\"/pid/set-point\",ACTIVATE_URL=CONTROLLER_BASE_URL+\"/pid/activate\",FETCH_CONTROLLER_DATA_INTREVAL=1e3,temperatures=[],HEADER=\"tk1,tk2,tk3\\n\",TEMPERATURES_KEY=\"temperatures\",LOCAL_STORAGE_LIMIT=2e5,MAX_ALLOW_TEMPERATURE=500;function changeSetPoint(){console.log(\"Start Ecap Controller\");var e=new XMLHttpRequest,t=document.getElementById(\"set-point-input\").value;e.open(\"POST\",SET_POINT_URL+\"?setPoint=\"+t),e.setRequestHeader(\"Content-Type\",\"application/json\"),e.onload=function(){200===e.status?console.log(\"Set point udpated\"):200!==e.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},e.send()}function changePid(e){var t=\"?\";t=(t=(t=(t=t+\"kp=\"+e.currentTarget[0].value+\"&\")+\"ki=\"+e.currentTarget[1].value+\"&\")+\"kd=\"+e.currentTarget[2].value+\"&\")+\"minOutput=\"+e.currentTarget[3].value;var n=new XMLHttpRequest;n.open(\"POST\",PID_URL+t),n.setRequestHeader(\"Content-Type\",\"application/json\"),n.onload=function(){200===n.status?console.log(\"Pid change\"):200!==n.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},n.send()}function saveDataToExcel(){console.log(\"Saving data to excel\");var e=window.document.createElement(\"a\"),t=JSON.parse(localStorage.getItem(TEMPERATURES_KEY));t=HEADER+t.join(\"\\n\");var n=new Blob([t],{type:\"application/csv\"});e.href=window.URL.createObjectURL(n),e.download=\"measurement.csv\",document.body.appendChild(e),e.click()}function confirmExit(){document.getElementById(\"save-to-excel-after-close\").checked&&saveDataToExcel()}function fetchControllerData(){console.log(\"Fetching data from: \"+CONTROLLER_URL);var e=new XMLHttpRequest;e.onload=function(e){var t=e.target.response;updateHtmlData(t),saveData(t)},e.onerror=function(){document.getElementById(\"header\").innerHTML=\"An error occurred during the transaction<br>GET:\"+CONTROLLER_URL},e.open(\"GET\",CONTROLLER_URL,!0),e.setRequestHeader(\"Accept\",\"application/json\"),e.responseType=\"json\",e.send()}function saveData(e){for(var t=e.temperatures,n=\"\",o=0;o<t.length;o++)n=n.concat(t[o].value),o<t.length-1&&(n=n.concat(\",\"));temperatures.push(n),upsertDataLocalStorage(n)}function toggleRegulator(e){console.log(e),e&&localStorage.removeItem(TEMPERATURES_KEY);var t=new XMLHttpRequest;t.open(\"POST\",ACTIVATE_URL+\"?activate=\"+e),t.onload=function(){200===t.status?console.log(\"Regulator state change to: \"+e):200!==t.status&&console.log(\"An error occurred during the transaction POST new setPoint\")},t.send(JSON.stringify({activate:e}))}function upsertDataLocalStorage(e){if(TEMPERATURES_KEY in localStorage){var t=localStorage.getItem(TEMPERATURES_KEY),n=JSON.parse(t);n.length>=LOCAL_STORAGE_LIMIT&&(localStorage.removeItem(TEMPERATURES_KEY),n=[]),n.push(e),localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(n))}else{var o=Array(1).fill(e);localStorage.setItem(TEMPERATURES_KEY,JSON.stringify(o))}}function updateHtmlData(e){e.pid.activate?(document.getElementById(\"startButton\").style.display=\"none\",document.getElementById(\"stopButton\").style.display=\"inline\"):(document.getElementById(\"stopButton\").style.display=\"none\",document.getElementById(\"startButton\").style.display=\"inline\"),document.getElementById(\"controller-state\").innerHTML=\" STATE: \"+(e.pid.activate?\"ON\":\"OFF\"),document.getElementById(\"header\").innerHTML=\"ECAP Temperature Controller\",document.getElementById(\"set-point\").innerHTML=\"Setpoint: \"+e.pid.setPoint+\"°C\",document.getElementById(\"kp\").innerHTML=\"kp: \"+e.pid.kp,document.getElementById(\"ki\").innerHTML=\"ki: \"+e.pid.ki,document.getElementById(\"kd\").innerHTML=\"kd: \"+e.pid.kd,document.getElementById(\"min-output\").innerHTML=\"minOutput: \"+e.pid.minOutput;for(var t=e.temperatures,n={},o=document.getElementById(\"header\"),a=0;a<t.length;a++){var r=t[a];r.value>MAX_ALLOW_TEMPERATURE&&(n=r),document.getElementById(r.name).innerHTML=r.name+\": \"+r.value+\"°C\"}if(null!=n.name){document.getElementById(\"control\").style.display=\"none\",o.style.color=\"red\";var l=\"SYSTEM IS IN DANGER STATE </br>TEMPERATURE \"+n.name+\" is: \"+n.value+\"°C </br>MAX ALLOW TEMPERATURE IS: \"+MAX_ALLOW_TEMPERATURE+\" °C\";o.innerHTML=l}else\"red\"===o.style.color&&(document.getElementById(\"control\").style.display=\"inline\",o.style.color=\"black\")}window.onload=function(){setTimeout(function(){document.body.style.opacity=\"100\"},1e3)},window.onbeforeunload=confirmExit;var fetchControllerDataTimer=setInterval(fetchControllerData,FETCH_CONTROLLER_DATA_INTREVAL);</script></body></html>"};
const char indexPath[] = "/ ";
const char controllerDataPath[] = "/controller-data";
const char pidPath[] = "/pid";
const char pidSetPointPath[] = "/pid/set-point";
const char pidActivatePath[] = "/pid/activate";

static const int REQUEST_LIMIT = 100;
static const int SECURITY_PROTECTION_INTERVAL = 15000;
int protectionCounter = 0;
int prevProtectionCounter = 0;
const int MEASURING_TEMPERATURE_INTERVAL = 1000;
static const int PROTECTION_COUNTER_MAX = 10;
static const int TK_1_PIN = 0;
static const int TK_2_PIN = 1;
static const int TK_3_PIN = 2;

char request[REQUEST_LIMIT];

// Serial setup
String requestFromClient = "";
boolean isRequestCompleted = false;

Adafruit_ADS1115 ads(0x48);
double tk1;
double tk1Predict;
double tk2;
double tk3;

int MAX_TEMEPRATURE = 400;

Timer protectionTimer;
Timer measurmentTimer;
double prediction;

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

void handleComunication();

void handleRelayOutput();

void turnOffController();

void printWithTab(double d);

double readTemperatureADS1115(const int externalModulAnalogPin);

void setupADS1115();

void adaptPidParamters();

void computePrediction();

void controllProcess();

void analyzeProcess();

double readTemperatureADS1115Samo(const int pin);

void setupMinOutput();

void initialSignalization();

void setupControllinoOutputs();

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
        PORTD = PORTD | B01110000;
    } else if (actuator == HW_TIMMER) {
        PORTD = PORTD & B10001111;
    }
    timeCounter = timeCounter == maxPowerInterval ? 0 : timeCounter + 1;
}

void setupPid() {
    windowStartTime = millis();
    setPoint = 0;
    pid.SetOutputLimits(0, WindowSize);
    pid.SetSampleTime(maxPowerInterval);
    pid.SetMode(AUTOMATIC);
}

void securityEvaluation() {

    if (protectionCounter > prevProtectionCounter) {
        Serial.println("CLIENT IS CONNECTED OK");
        prevProtectionCounter = protectionCounter;
        isClientOnline = true;
    } else {
        Serial.println("STOP EVERYTHING No client is connected for 15 seconds");
        isClientOnline = false;
    }

    if (protectionCounter > PROTECTION_COUNTER_MAX) {
        prevProtectionCounter = 0;
        protectionCounter = 0;
    }
}

float readTemperatureSamoNMLSG(uint8_t analogPin) {
    float voltageCoeficient = (float) 12.0 / (float) 1023.0;
    float voltage = analogRead(analogPin) * voltageCoeficient * 1.260;
    voltage = voltage - 2.06;
    return voltage / 0.0156;
}

int predictionCounter = 0;

void measureTemperatures() {
    StaticJsonBuffer<80> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    JsonArray &temperaturesArray = root.createNestedArray("tempratures");
    tk1 = readTemperatureADS1115(TK_1_PIN);
    tk2 = readTemperatureADS1115(TK_2_PIN);
    tk3 = readTemperatureADS1115Samo(TK_3_PIN);
    temperaturesArray.add(tk1);
    temperaturesArray.add(tk2);
    temperaturesArray.add(tk3);
    root.printTo(Serial);
    Serial.println();
    Serial.print(pid.GetKp());
    Serial.print(",");
    Serial.print(pid.GetKi());
    Serial.print(",");
    Serial.println(pid.GetKd());

    computePrediction();
    tk1Predict = tk1 + predictTemperature;

    if (tk1 > MAX_TEMEPRATURE || tk2 > MAX_TEMEPRATURE || tk3 > MAX_TEMEPRATURE) {
        turnOffController();
    }
}

double readTemperatureADS1115Samo(const int pin) {
    int16_t adc0;  // we read from the ADC, we have a sixteen bit integer as a result

    adc0 = ads.readADC_SingleEnded(pin);
    double voltage = (adc0 * 0.1875) / 1000.0;
    double temperature = 125.7 * voltage - 123.1;
    return temperature;
}

double readTemperatureADS1115(const int externalModulAnalogPin) {
    int16_t adc0;  // we read from the ADC, we have a sixteen bit integer as a result

    adc0 = ads.readADC_SingleEnded(externalModulAnalogPin);
    double voltage = (adc0 * 0.1875) / 1000.0;
    double temperature = voltage / 0.005;
    return temperature;
}

void printWithTab(double d) {
    Serial.print(d);
    Serial.print("\t");
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
    Input = tk1Predict;
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

/**
 * Compute prediction of system temperature using
 * empiric constants
 * PidOutput * EmpiricTemperature / SampleTime
 *
 * for 50[°C] 5[s] -> 2.88[°C]
 * PidOutput * 2.88[°C] / 5.0[s]
 * PidOutput 0.4[°C] / 1000[ms] -> WHY ARE USING THIS ONE
 */
void computePrediction() {
    // Delete sum of prediction when Output Reach minOutput
    //TODO for more sophisticated solution use prediction for cooling as well
    if (Output == minOutput) {
        predictTemperature = 0;
    }
    predictTemperature += (Output * 0.4 / 1000.0);
    Serial.print("Actual temperature tk1: ");
    Serial.print(tk1, 4);
    Serial.print(" Predict temeperature: ");
    Serial.print(tk1 + predictTemperature);
    Serial.print(" For Pid output[ms]: ");
    Serial.println(Output);
}

void handleRelayOutput() {
    if (millis() - windowStartTime >= WindowSize) { //time to shift the Relay Window
        windowStartTime += WindowSize;
    }
    if (Output > millis() - windowStartTime) {
        PORTD = PORTD | B00000000;
    } else {
        PORTD = PORTD & B10001111;
    }
}


void handleRequest() {
    if (isRequestCompleted) {
        Serial.println(requestFromClient);

        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &requestFromClientJson = jsonBuffer.parseObject(requestFromClient);

        if (requestFromClientJson.success()) {
            int32_t tempSetpoint = requestFromClientJson["setPoint"];
            if (tempSetpoint <= 100 && tempSetpoint >= 0) {
                Serial.print("Set point from client change to: ");
                Serial.println(tempSetpoint);
                setPoint = tempSetpoint;
            }
        } else {
            Serial.print("Power from client will not be change can't parse request ");
        }

        requestFromClient = "";
        isRequestCompleted = false;
    }
}

/**
 * This is event for interaction with USB devices
 */
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
            setupMinOutput();
        } else if (strstr(keyname, "kp") != NULL) {
            kp = atof(val);
        } else if (strstr(keyname, "kd") != NULL) {
            kd = atof(val);
        } else if (strstr(keyname, "ki") != NULL) {
            ki = atof(val);
        } else if (strstr(keyname, "activate") != NULL) {
            isPidActive = strstr(val, "true") != NULL;
        } else if (strstr(keyname, "minOutput") != NULL) {
            minOutput = atof(val);
            pid.SetOutputLimits(minOutput, maxPowerInterval);

        }
        pch = strtok(NULL, "&");
    }
}

void setupMinOutput() {

    if (setPoint >= 330) {
        minOutput = 350;
    } else if (setPoint >= 300) {
        minOutput = 300;
    } else if (setPoint >= 250) {
        minOutput = 300;
    } else if (setPoint >= 200) {
        minOutput = 200;
    } else if (setPoint >= 150) {
        minOutput = 100;
    } else {
        minOutput = 0;
    }
    pid.SetOutputLimits(minOutput, maxPowerInterval);
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
                        //setup pid constants
                        pid.SetTunings(kp, ki, kd);
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
        protectionCounter++;
        isClientOnline = true;
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
    pidJson["minOutput"] = minOutput;
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
    PORTD = PORTD & B10001111;
    power = 0;
}

void turnOnController() {
    PORTD = PORTD | B01110000;
}



void setupControllinoOutputs() {
    DDRD = DDRD | B01110000; // set D20 D21 D22 controllino Digital pins as output, on mega board PCB
}

void initialSignalization() {
    pinMode(CONTROLLINO_PIN_HEADER_DIGITAL_OUT_08, Output);
    digitalWrite(CONTROLLINO_PIN_HEADER_DIGITAL_OUT_08, HIGH);
    delay(2000);
    digitalWrite(CONTROLLINO_PIN_HEADER_DIGITAL_OUT_08, LOW);
}

void setupADS1115() {
    ads.begin();
}

void setup() {
    initialSignalization();
    //setup watchdog
    wdt_enable(WDTO_8S);
    setupSerial();
    setupEthernet();
    setupPid();
    //timers setup
    setupHwTimer();
    setupADS1115();
    protectionTimer.every(SECURITY_PROTECTION_INTERVAL, securityEvaluation);
    measurmentTimer.every(MEASURING_TEMPERATURE_INTERVAL, measureTemperatures);
    setupControllinoOutputs();
}

void loop() {
    // for quick analyzing turn on
    boolean isControllProcess = true;
    if (isControllProcess) {
        controllProcess();
    } else {
        analyzeProcess();
    }
}

void analyzeProcess() {
    unsigned long waitForActionDelay = 5000;
    long setPoint = 100;
    double tk1 = readTemperatureADS1115(TK_1_PIN);
    Serial.println("Start analyze process");
    Serial.println("Start temperature:");
    Serial.println(tk1, 4);
    Serial.print("Turn on the actuator and wait: ");
    Serial.print(waitForActionDelay);
    turnOnController();
    delay(waitForActionDelay);
    turnOffController();
    Serial.println("Waiting for temperature start to decrease");
    long prevTemperature = 0;
    int counterSeconds = 0;
    while (true) {
        wdt_reset();
        tk1 = readTemperatureADS1115(TK_1_PIN);
        Serial.println(tk1, 4);
        Serial.print("seconds: ");
        Serial.println(counterSeconds);
        delay(1000);
        counterSeconds++;
    }
}

void controllProcess() {
    handleComunication();
    //Measure process values
    measurmentTimer.update();
    //PID controller process
    if (isPidActive) {
        if (pid.GetMode() == MANUAL) {
            Serial.println("Setting automatic mode");
            pid.SetMode(AUTOMATIC);
        }
        if (isPidInAutomaticMode) {
            adaptPidParamters();
        }
        controlProcess();
    } else {
        turnOffController();
    }

    if (isClientOnline) {
        wdt_reset();
    }
    //Security checks
    protectionTimer.update();
}

void changePidParamters(int parameterRange) {
    switch (parameterRange) {
        case 1:
            kp = (setPoint / 150.000) * 13;
            ki = (setPoint / 150.000) * 35;
            minOutput = (setPoint / 150.000) * 16;
            break;
        case 2:
            kp = (setPoint / 150.000) * 20;
            ki = (setPoint / 150.000) * 55;
            minOutput = (setPoint / 150.000) * 16;
            break;
        case 3:
            kp = (setPoint / 200.000) * 23;
            ki = (setPoint / 200.000) * 80;
            minOutput = (setPoint / 200.000) * 16;
            break;
        case 4:
            kp = (setPoint / 200.000) * 21;
            ki = ((setPoint / 200.000) * 80) - 40.00;
            minOutput = (setPoint / 200.000) * 17;
            break;
        case 5:
            kp = (setPoint / 200.000) * 22;
            ki = ((setPoint / 200.000) * 80) - 40.00;
            minOutput = (setPoint / 200.000) * 19;
            break;
        case -1:
            kp = 3.8150;
            ki = 38.2546;
            minOutput = (setPoint / 150) * 16;
            break;
    }
    pid.SetOutputLimits(minOutput, maxPowerInterval);
}

void adaptPidParamters() {
    if (setPoint >= 0 && setPoint < 100) {
        changePidParamters(1);
    } else if (setPoint >= 100 && setPoint <= 150) {
        changePidParamters(2);
    } else if (setPoint > 150 && setPoint < 200) {
        changePidParamters(3);
    } else if (setPoint >= 200 && setPoint <= 329) {
        changePidParamters(4);
    } else if (setPoint > 330 && setPoint <= 350) {
        changePidParamters(5);
    } else {
        changePidParamters(-1);
    }
}



