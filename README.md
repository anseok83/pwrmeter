# Pwrmeter

Pwrmeter (Power meter) is a Arduino-based instantaneous power meter using SCT-013 current sensor.

## Features and screen shot

### Version 0.0.1 (2021)
* Measure instantaneous power usage (kW) using sct-013 sensor.
* Report power usage data using REST
* Over-the-air Wifi configuration (No hard coding is required to enter SSID/password)

## Arduino board (Pwrmeter Sensor)

![SCT-013 sensor installation](/images/sct013.png)

![Pwrmeter sensor](/images/pwrmeter_sensor.png)

* Notes:
Circuit of pwrmeter is referred from https://learn.openenergymonitor.org/electricity-monitoring/ct-sensors/interface-with-arduino
We used a 120 ohm for burden resistor. (Maximum current is limited to ~ 30A.

* Connections
A0 [CURRENT_MEASURE_PIN] : To measure an instantaneous current value. Measuring a voltage on the burden resistor.
D15 [TRIGGER_PIN] : To reset Wifi configuration.

## Software components (including libraries):

* WiFiManager (https://github.com/tzapu/WiFiManager)

  Used WiFiManager library for easy and flexible on-board Wifi configurations.
  
* EmonLib (https://www.arduino.cc/reference/en/libraries/emonlib/)

* aRest (https://www.arduino.cc/reference/en/libraries/arest/)


## REST address

You may need to find the IP address of the pwrsensor (arduino) board after Wifi configuration and connection. You may check from your Wifi router or by other ways. (Or also can fix the address through the router.)

REST address to get the power value : http://[Your_pwrsensor_IP_address]/powerMeasure

Example:
```
~$curl -v http://192.168.1.228/powerMeasure
*   Trying 192.168.1.228:80...
* TCP_NODELAY set
* Connected to 192.168.1.228 (192.168.1.228) port 80 (#0)
> GET /powerMeasure HTTP/1.1
> Host: 192.168.1.228
> User-Agent: curl/7.68.0
> Accept: */*
> 
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< Access-Control-Allow-Origin: *
< Access-Control-Allow-Methods: POST, GET, PUT, OPTIONS
< Content-Type: application/json
< Connection: close
< 
{"powerMeasure": 0.302 "id": "1", "name": "powme", "hardware": "esp8266", "connected": true}
* Closing connection 0
~$
```

## Homeassistant integration

You can integrate the pwrmeter sensor using home-assistant (https://www.home-assistant.io/) using RESTfult component (https://www.home-assistant.io/integrations/rest/).

Example configuration code for the integration is following.

```
sensor:
  - platform: rest
    resource: http://192.168.1.228/powerMeasure
    name: Power
    value_template: '{{ value_json.powerMeasure }}'
    unit_of_measurement: W
  - platform: filter
    name: "Filtered Power"
    entity_id: sensor.power
    filters:
      - filter: time_simple_moving_average
        window_size: 00:30
```

![Pwrsensor filtered power value](/images/pwrmeter_filtered.png)


<br><br>
