#include <Arduino.h>
#include <WiFi.h>
#include "driver/uart.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define UART_PORT 2

const char* ssid = "wahoo";

WiFiClientSecure secureClient;
HTTPClient httpClient;

String get_wifi_status(int status) {
    switch(status) {
        case WL_IDLE_STATUS:
            return "WL_IDLE_STATUS";
        case WL_SCAN_COMPLETED:
            return "WL_SCAN_COMPLETED";
        case WL_NO_SSID_AVAIL:
            return "WL_NO_SSID_AVAIL";
        case WL_CONNECT_FAILED:
            return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "WL_CONNECTION_LOST";
        case WL_CONNECTED:
            return "WL_CONNECTED";
        case WL_DISCONNECTED:
            return "WL_DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

void connectToWiFi() {
    Serial.begin(115200);
    int status = WL_IDLE_STATUS;
    Serial.println("\nConnecting to Wi-Fi...");
    WiFi.begin(ssid);

    int attempts = 0;
    int maxAttempts = 30;

    while(status != WL_CONNECTED && attempts < maxAttempts) {
        status = WiFi.status();
        Serial.println(get_wifi_status(status));
        attempts++;
        delay(500);
    }

    if (status != WL_CONNECTED) {
        Serial.println("Failed to connect to Wi-Fi");
        return;
    }

    Serial.println("\nConnected to the Wi-Fi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}

void setupHTTPClient() {
    secureClient.setInsecure(); 
    httpClient.setTimeout(10000);
    Serial.println("HTTPS Client Setup");
}

void sendPostRequest(const char *barcode) {
    int success = 0;
    int attempts = 0;
    int maxAttempts = 3;

    while (!success && attempts < maxAttempts) {
        // Prepare POST data
        String postData = "barcode=";
        postData += barcode;

        String url = "https://cs4640.cs.virginia.edu/xtz3mx/pourpro-test/?command=increment-product-on-scan";
        httpClient.begin(secureClient, url);
        httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");

        int httpResponseCode = httpClient.POST(postData);

        // Handle based on response code
        if (httpResponseCode > 0) {
            Serial.printf("HTTP POST successful, response code: %d\n", httpResponseCode);
            String response = httpClient.getString();
            Serial.println("Response: " + response);
            success = 1;
            httpClient.end();
        } else {
            Serial.printf("HTTP POST failed, response code: %d, error: %s\n", httpResponseCode, httpClient.errorToString(httpResponseCode).c_str());
            delay(500 * attempts);
        }

        attempts++;
    }
}

void setup() {
    Serial.begin(115200);

    // Configure UART
    const uart_port_t uart_num = UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // Configure UART pins and install driver
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Set up UART with event queue
    const int uart_buffer_size = 2048;
    QueueHandle_t uart_queue;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));

    Serial.println("UART initialized");

    // Connect to WiFi
    connectToWiFi();
    
    setupHTTPClient();

}

void loop() {
    size_t length; 
    uint8_t data[2048]; 

    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, &length));

    // Prevent overflow
    if (length > sizeof(data)) {
        length = sizeof(data);
    }

    // Read data from UART
    int bytesRead = uart_read_bytes(UART_PORT, data, length, 100);

    if (bytesRead > 2) {
        Serial.printf("Bytes Read: %d\n", bytesRead);

        // Null-terminate
        if (bytesRead < sizeof(data)) {
            data[bytesRead] = '\0';
        } else {
            data[sizeof(data) - 1] = '\0';
        }

        Serial.print("Barcode: ");
        for (int i = 0; i < bytesRead; i++) {
            Serial.print((char)data[i]);
        }
        Serial.println();

        sendPostRequest((const char*)data);
        delay(1000);
    }
}
