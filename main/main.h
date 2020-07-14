#pragma once

struct BmpMessage {
    float temperature;
    float pressure;
    float humidity;
};

struct UltrasonicMessage {
    int echo_us;
};

struct JsonMessage {
    char json[120];
};
