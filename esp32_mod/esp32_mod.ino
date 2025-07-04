#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <Ticker.h>
#include <Preferences.h>       // Use Preferences instead of EEPROM (NVS storage)
#include <ezButton.h>
#include <IRremote.hpp>        // v4.x+ of IRremote

//---------------------------------------------------
const char *service_name = "Prov_Tahidultech";
const char *pop = "12345678";

Node my_node;

// Virtual Alarm (no GPIO attached)
static Param param_ldr("LDR", "esp.param.int", value(0), PROP_FLAG_READ);
// Device Names
char device1[] = "Switch1";
char device2[] = "Switch2";
char device3[] = "Switch3";
char device4[] = "Switch4";

//---------------------------------------------------
// GPIO mapping
static uint8_t RELAY_1 = 5;  //D5
static uint8_t RELAY_2 = 18; //D18
static uint8_t RELAY_3 = 19; //D19
static uint8_t RELAY_4 = 21; //D21

ezButton button1(27);   //D27
ezButton button2(14);   //D14
ezButton button3(12);   //D12
ezButton button4(13);   //D13

static uint8_t WIFI_LED    = 4;  //D4
static uint8_t gpio_reset = 0;

const byte IR_RECEIVE_PIN = 22; //D22

#define LDR_PIN 34  //D34
Ticker ldrTicker;
int ldr_value = 0;
int ldr_low_threshold = 500;
int ldr_high_threshold = 3500;
bool security_enabled = false;

static Device virtual_alarm("LDR Alarm", "esp.device.sensor");
static Param param_security("SecurityMode", "esp.param.switch", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
static Param param_high_thresh("HighThresh", "esp.param.slider", value(3500), PROP_FLAG_READ | PROP_FLAG_WRITE);
static Param param_low_thresh("LowThresh", "esp.param.slider", value(500), PROP_FLAG_READ | PROP_FLAG_WRITE);


//---------------------------------------------------
// Relay State
bool STATE_RELAY_1 = LOW;
bool STATE_RELAY_2 = LOW;
bool STATE_RELAY_3 = LOW;
bool STATE_RELAY_4 = LOW;

bool last_wifi_status = false;

//---------------------------------------------------
// RainMaker device objects
static Switch my_switch1(device1, &RELAY_1);
static Switch my_switch2(device2, &RELAY_2);
static Switch my_switch3(device3, &RELAY_3);
static Switch my_switch4(device4, &RELAY_4);

// Extra switches for All ON and All OFF
Switch all_on_device("All On");
Switch all_off_device("All Off");

// Preferences for NVS storage
Preferences preferences;

/****************************************************************************************************
 * sysProvEvent Function
*****************************************************************************************************/
void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {      
        case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
        printQR(service_name, pop, "ble");
#else
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
        printQR(service_name, pop, "softap");
#endif        
        break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.printf("\nConnected to Wi-Fi!\n");
        break;
    }
}

/****************************************************************************************************
 * write_callback Function
*****************************************************************************************************/
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();
    
    // Inside write_callback function
    if (strcmp(device_name, "Node") == 0) { // Node-level params
        if (strcmp(param_name, "SecurityMode") == 0) {
            security_enabled = val.val.b;
            Serial.printf("🔐 Security Mode: %s\n", security_enabled ? "ON" : "OFF");
        }
        else if (strcmp(param_name, "HighThresh") == 0) {
            ldr_high_threshold = val.val.i;
            Serial.printf("🔼 High Threshold Set: %d\n", ldr_high_threshold);
        }
        else if (strcmp(param_name, "LowThresh") == 0) {
            ldr_low_threshold = val.val.i;
            Serial.printf("🔽 Low Threshold Set: %d\n", ldr_low_threshold);
        }
    }

    if(strcmp(param_name, "Power") == 0) {
        bool incoming_state = val.val.b;

        if(strcmp(device_name, device1) == 0) {
            STATE_RELAY_1 = incoming_state;
            digitalWrite(RELAY_1, STATE_RELAY_1);
            preferences.begin("relays", false);
            preferences.putBool("relay1", STATE_RELAY_1);
            preferences.end();
        }
        else if(strcmp(device_name, device2) == 0) {
            STATE_RELAY_2 = incoming_state;
            digitalWrite(RELAY_2, STATE_RELAY_2);
            preferences.begin("relays", false);
            preferences.putBool("relay2", STATE_RELAY_2);
            preferences.end();
        }
        else if(strcmp(device_name, device3) == 0) {
            STATE_RELAY_3 = incoming_state;
            digitalWrite(RELAY_3, STATE_RELAY_3);
            preferences.begin("relays", false);
            preferences.putBool("relay3", STATE_RELAY_3);
            preferences.end();
        }
        else if(strcmp(device_name, device4) == 0) {
            STATE_RELAY_4 = incoming_state;
            digitalWrite(RELAY_4, STATE_RELAY_4);
            preferences.begin("relays", false);
            preferences.putBool("relay4", STATE_RELAY_4);
            preferences.end();
        }
        else if (strcmp(device_name, "All On") == 0) {
        STATE_RELAY_1 = STATE_RELAY_2 = STATE_RELAY_3 = STATE_RELAY_4 = HIGH;

        digitalWrite(RELAY_1, STATE_RELAY_1);
        digitalWrite(RELAY_2, STATE_RELAY_2);
        digitalWrite(RELAY_3, STATE_RELAY_3);
        digitalWrite(RELAY_4, STATE_RELAY_4);

        preferences.begin("relays", false);
        preferences.putBool("relay1", STATE_RELAY_1);
        preferences.putBool("relay2", STATE_RELAY_2);
        preferences.putBool("relay3", STATE_RELAY_3);
        preferences.putBool("relay4", STATE_RELAY_4);
        preferences.end();

        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
        my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
        my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
        my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);
        Serial.println("All Relays ON from App");

    }
    else if (strcmp(device_name, "All Off") == 0) {
        STATE_RELAY_1 = STATE_RELAY_2 = STATE_RELAY_3 = STATE_RELAY_4 = LOW;

        digitalWrite(RELAY_1, STATE_RELAY_1);
        digitalWrite(RELAY_2, STATE_RELAY_2);
        digitalWrite(RELAY_3, STATE_RELAY_3);
        digitalWrite(RELAY_4, STATE_RELAY_4);

        preferences.begin("relays", false);
        preferences.putBool("relay1", STATE_RELAY_1);
        preferences.putBool("relay2", STATE_RELAY_2);
        preferences.putBool("relay3", STATE_RELAY_3);
        preferences.putBool("relay4", STATE_RELAY_4);
        preferences.end();

        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
        my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
        my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
        my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);
        Serial.println("All Relays OFF from App");
    }
    }
}

/****************************************************************************************************
 * setup Function
*****************************************************************************************************/
void setup() {
    Serial.begin(115200);
    uint32_t chipId = 0;

    // Initialize Preferences (NVS)
    preferences.begin("relays", false);

    // Load previous relay states from Preferences
    STATE_RELAY_1 = preferences.getBool("relay1", LOW);
    STATE_RELAY_2 = preferences.getBool("relay2", LOW);
    STATE_RELAY_3 = preferences.getBool("relay3", LOW);
    STATE_RELAY_4 = preferences.getBool("relay4", LOW);

    // Set relay pins as OUTPUT and write initial states
    pinMode(RELAY_1, OUTPUT);
    pinMode(RELAY_2, OUTPUT);
    pinMode(RELAY_3, OUTPUT);
    pinMode(RELAY_4, OUTPUT);
    digitalWrite(RELAY_1, STATE_RELAY_1);
    digitalWrite(RELAY_2, STATE_RELAY_2);
    digitalWrite(RELAY_3, STATE_RELAY_3);
    digitalWrite(RELAY_4, STATE_RELAY_4);

    // Set button debounce time
    button1.setDebounceTime(100);
    button2.setDebounceTime(100);
    button3.setDebounceTime(100);
    button4.setDebounceTime(100);

    // Other GPIOs
    pinMode(gpio_reset, INPUT);
    pinMode(WIFI_LED, OUTPUT);

    // IRremote initialization
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

    // Ticker to check LDR alarm every 3 seconds
    ldrTicker.attach(3, checkLdrAlarm);

    // Initialize RainMaker node
    my_node = RMaker.initNode("TAHIDUL'S Smart Home");

    // Add LDR device
    Device ldr_sensor("LDR Monitor", "esp.device.sensor", (void*)"ldr");  
    ldr_sensor.addParam(param_ldr);
    my_node.addDevice(ldr_sensor);

    // Add virtual alarm device

    virtual_alarm.addParam(param_security);
    virtual_alarm.addParam(param_high_thresh);
    virtual_alarm.addParam(param_low_thresh);
    virtual_alarm.addParam(Param("esp.param.icon", "esp.param.icon", value("alert"), PROP_FLAG_READ));
    virtual_alarm.addCb(write_callback);
    my_node.addDevice(virtual_alarm);

    // Add main switches with icons and callbacks
    my_switch1.addCb(write_callback);
    my_switch2.addCb(write_callback);
    my_switch3.addCb(write_callback);
    my_switch4.addCb(write_callback);

    my_switch1.addParam(Param("ui", "esp.param.ui", value("icon-device-lightbulb"), PROP_FLAG_READ));
    my_switch2.addParam(Param("ui", "esp.param.ui", value("icon-device-fan"), PROP_FLAG_READ));
    my_switch3.addParam(Param("ui", "esp.param.ui", value("icon-device-plug"), PROP_FLAG_READ));
    my_switch4.addParam(Param("ui", "esp.param.ui", value("icon-device-outlet"), PROP_FLAG_READ));

    my_node.addDevice(my_switch1);
    my_node.addDevice(my_switch2);
    my_node.addDevice(my_switch3);
    my_node.addDevice(my_switch4);

    // Add all ON/OFF virtual switches
    all_on_device.addCb(write_callback);
    all_off_device.addCb(write_callback);
    all_on_device.addParam(Param("ui", "esp.param.ui", value("alarm-light"), PROP_FLAG_READ));
    all_off_device.addParam(Param("ui", "esp.param.ui", value("power"), PROP_FLAG_READ));

    my_node.addDevice(all_on_device);
    my_node.addDevice(all_off_device);

    // Enable RainMaker services
    RMaker.enableTZService();
    RMaker.enableSchedule();
    // RMaker.enableOTA(OTA_USING_PARAMS);  // Optional OTA

    // Generate Chip ID
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }

    Serial.printf("\nChip ID:  %d Service Name: %s\n", chipId, service_name);
    Serial.println("Starting ESP-RainMaker");

    RMaker.start();

    // Provisioning
    WiFi.onEvent(sysProvEvent);
    #if CONFIG_IDF_TARGET_ESP32
        WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE,
                                NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
                                NETWORK_PROV_SECURITY_1,
                                pop, service_name);
    #else
        WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP,
                                NETWORK_PROV_SCHEME_HANDLER_NONE,
                                NETWORK_PROV_SECURITY_1,
                                pop, service_name);
    #endif

    // Report initial state
    my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
    my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
    my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
    my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);

    Serial.printf("Relay1 is %s\n", STATE_RELAY_1 ? "ON" : "OFF");
    Serial.printf("Relay2 is %s\n", STATE_RELAY_2 ? "ON" : "OFF");
    Serial.printf("Relay3 is %s\n", STATE_RELAY_3 ? "ON" : "OFF");
    Serial.printf("Relay4 is %s\n", STATE_RELAY_4 ? "ON" : "OFF");
}

// ---------- Inside checkLdrAlarm() ----------
void checkLdrAlarm() {
  int ldr_value = analogRead(34);
  Serial.printf("LDR Value: %d\n", ldr_value);

  param_ldr.updateAndReport(value(ldr_value));  // ✅ REPORT TO APP

  if (security_enabled) {
    if (ldr_value > ldr_high_threshold) {
        virtual_alarm.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, true);
        Serial.println("🚨 LDR HIGH Alarm Triggered!");
    } else if (ldr_value < ldr_low_threshold) {
        virtual_alarm.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, true);
        Serial.println("⚠️ LDR LOW Alarm Triggered!");
    } else {
        virtual_alarm.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
    }
  }
}

/****************************************************************************************************
 * loop Function
*****************************************************************************************************/
void loop()
{
    // Read GPIO0 (external button to reset device)
    if(digitalRead(gpio_reset) == LOW) { //Push button pressed
        Serial.printf("Reset Button Pressed!\n");
        delay(100);
        int startTime = millis();
        while(digitalRead(gpio_reset) == LOW) delay(50);
        int endTime = millis();

        if ((endTime - startTime) > 10000) {
            // If key pressed for more than 10secs, reset all
            Serial.printf("Reset to factory.\n");
            RMakerFactoryReset(2);
        } 
        else if ((endTime - startTime) > 3000) {
            Serial.printf("Reset Wi-Fi.\n");
            // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
            RMakerWiFiReset(2);
        }
    }

    delay(50);
    
    bool current_wifi_status = (WiFi.status() == WL_CONNECTED);

    if (current_wifi_status != last_wifi_status) {
    digitalWrite(WIFI_LED, current_wifi_status ? HIGH : LOW);
    last_wifi_status = current_wifi_status;
    }

    button_control();
    remote_control();
}

/*******************************************************************************
 * button_control function:
 ******************************************************************************/
void button_control(){
    button1.loop();
    if(button1.isPressed()){
        control_relay(1, RELAY_1, STATE_RELAY_1);
        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
    }
    button2.loop();
    if(button2.isPressed()){
        control_relay(2, RELAY_2, STATE_RELAY_2);
        my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
    }
    button3.loop();
    if(button3.isPressed()){
        control_relay(3, RELAY_3, STATE_RELAY_3);
        my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
    }
    button4.loop();
    if(button4.isPressed()){
        control_relay(4, RELAY_4, STATE_RELAY_4);
        my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);
    }
}

/****************************************************************************************************
 * control_relay Function
*****************************************************************************************************/
void control_relay(int relay_no, int relay_pin, boolean &status){
    status = !status;
    digitalWrite(relay_pin, status);
    preferences.begin("relays", false);
    switch(relay_no) {
        case 1: preferences.putBool("relay1", status); break;
        case 2: preferences.putBool("relay2", status); break;
        case 3: preferences.putBool("relay3", status); break;
        case 4: preferences.putBool("relay4", status); break;
        default: break;
    }
    preferences.end();
    String text = (status)? "ON" : "OFF";
    Serial.println("Relay"+String(relay_no)+" is "+text);
}

/****************************************************************************************************
 * remote_control Function
*****************************************************************************************************/
void remote_control()
{
    if (IrReceiver.decode()) {
        uint64_t raw_code = IrReceiver.decodedIRData.decodedRawData;
        Serial.print("Protocol: ");
        Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
        Serial.print("Raw Code: 0x");
        Serial.println(raw_code, HEX);

        switch (raw_code) {
            case 0x3B04FDC611C:  // Switch1
                control_relay(1, RELAY_1, STATE_RELAY_1);
                my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
                break;

            case 0x3BC43DC611C:  // Switch2 (update with real value)
                control_relay(2, RELAY_2, STATE_RELAY_2);
                my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
                break;

            case 0x3A05FDC611C:  // Switch3 (update with real value)
                control_relay(3, RELAY_3, STATE_RELAY_3);
                my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
                break;

            case 0x3AC53DC611C:  // Switch4 (update with real value)
                control_relay(4, RELAY_4, STATE_RELAY_4);
                my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);
                break;

            case 0x3C43BDC611C:  // All ON (update with real value)
                STATE_RELAY_1 = STATE_RELAY_2 = STATE_RELAY_3 = STATE_RELAY_4 = HIGH;
                digitalWrite(RELAY_1, STATE_RELAY_1);
                digitalWrite(RELAY_2, STATE_RELAY_2);
                digitalWrite(RELAY_3, STATE_RELAY_3);
                digitalWrite(RELAY_4, STATE_RELAY_4);

                preferences.begin("relays", false);
                preferences.putBool("relay1", STATE_RELAY_1);
                preferences.putBool("relay2", STATE_RELAY_2);
                preferences.putBool("relay3", STATE_RELAY_3);
                preferences.putBool("relay4", STATE_RELAY_4);
                preferences.end();

                my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
                my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
                my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
                my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);

                Serial.println("All Relays ON");
                break;

            case 0x3B44BDC611C:  // All OFF (update with real value)
                STATE_RELAY_1 = STATE_RELAY_2 = STATE_RELAY_3 = STATE_RELAY_4 = LOW;
                digitalWrite(RELAY_1, STATE_RELAY_1);
                digitalWrite(RELAY_2, STATE_RELAY_2);
                digitalWrite(RELAY_3, STATE_RELAY_3);
                digitalWrite(RELAY_4, STATE_RELAY_4);

                preferences.begin("relays", false);
                preferences.putBool("relay1", STATE_RELAY_1);
                preferences.putBool("relay2", STATE_RELAY_2);
                preferences.putBool("relay3", STATE_RELAY_3);
                preferences.putBool("relay4", STATE_RELAY_4);
                preferences.end();

                my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
                my_switch2.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_2);
                my_switch3.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_3);
                my_switch4.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_4);

                Serial.println("All Relays OFF");
                break;

            default:
                Serial.println("Unknown IR code");
        }

        IrReceiver.resume();
    }
}
