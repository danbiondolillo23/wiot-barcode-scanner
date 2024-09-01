# Bluetooth-Based Inventory System

## Project Collaborators
- Daniel Biondolillo
- Dagim Tekle
- Samuel Veliveli

## Overview

Traditional Bluetooth-based inventory systems typically rely on centralized connections to desktop computers or laptops, which serve as the primary data processing hubs. While this setup can be effective in smaller or controlled environments, it is less suitable for large-scale inventory operations in settings like factory production floors, warehouses, or construction sites. These environments present unique challenges such as dust, moisture, temperature extremes, and the constant movement of heavy machinery. Such conditions can damage sensitive and expensive computer equipment, leading to increased costs, operational downtime, and decreased efficiency.

### The Problem

In large-scale inventory environments, the centralized nature of traditional systems introduces several inefficiencies:
- **Vulnerability to Harsh Conditions:** Expensive computer equipment is prone to damage from dust, moisture, and temperature extremes.
- **Operational Downtime:** The need for frequent repairs or replacements can disrupt inventory management operations.
- **Limited Flexibility:** Centralized systems often lack the modularity needed to efficiently manage inventory across large or complex sites.

### The Solution

Our solution leverages a Bluetooth barcode scanner in combination with an nrf52840 Bluetooth board and a Heltec WiFi board to create a modular, cost-effective, and robust inventory management system. This system eliminates the need for fragile desktop computers or laptops, reducing the risk of hardware damage and operational downtime in harsh environments.

Key benefits of our system include:
- **Modularity:** Bluetooth/WiFi units can be strategically placed throughout the site, extending the effective range and coverage of the inventory system.
- **Scalability:** The system is easily expandable, allowing for additional units to cover larger areas as needed.
- **Reliability:** Distributed units ensure continuous operation; if one unit requires maintenance, others can continue to function without disruption.

### System Architecture

In our system, Bluetooth barcode scanners communicate with nrf52840 Bluetooth boards, which are then wired to Heltec WiFi boards. The WiFi boards connect to the internet and transmit inventory data to a centralized management system. This modular configuration allows for broader coverage without compromising reliability or risking damage to expensive equipment.

If one unit needs maintenance or repair, others in the network can continue to operate, ensuring continuous and effective inventory tracking.

### How It Works

1. The nrf52840 board establishes a connection with the Bluetooth barcode scanner and subscribes to scan notifications.
2. Upon a scan event, the nrf52840 board reads the characteristic value (the barcode) and transmits it over UART (pins for wired serial communication) to the Heltec WiFi board.
3. The Heltec WiFi board, after ensuring it is connected to the internet, constructs an HTTP POST request with the barcode included in the payload.
4. The WiFi board sends this packet to a web server, which processes the request and updates the inventory count for the product associated with the barcode.

This architecture allows the system to efficiently manage inventory across large areas without relying on centralized, fragile, and expensive hardware.

---

### Conclusion

Our Bluetooth-based inventory system provides a robust, scalable, and cost-effective solution to the challenges of large-scale inventory management in harsh environments. By leveraging modular components and distributed units, this system enhances flexibility, reduces the risk of equipment damage, and ensures continuous operation.

