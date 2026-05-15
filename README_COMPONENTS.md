# SMART BOX Components

This document lists the main hardware components used by the SMART BOX firmware.

## Core Device
- Particle Argon (or compatible Particle device)

## Sensors
- DHT11 temperature and humidity sensor (Adafruit_DHT_Particle library)
- Grove Ultrasonic Ranger (distance measurement for the hatch)

## Outputs
- Grove 4-Digit Display (TM1637) for time display
- Grove Chainable RGB LED (status indicator)
- Piezo buzzer for audible alerts

## Pin Map (from firmware)

| Component | Pin(s) | Notes |
| --- | --- | --- |
| DHT11 | D2 | Temperature and humidity sensor |
| Ultrasonic Ranger | D4 | Distance measurement |
| 4-Digit Display (TM1637) | D1 (CLK), D0 (DIO) | Time display |
| Chainable RGB LED | A4 (Data), A5 (Clock) | Status indicator |
| Buzzer | A0 | Audible alerts |
