### ESP32 IoT Framework

**Overview**: 
This project is a detailed Internet of Things (IoT) framework developed on the ESP32 platform using the Espressif IoT Development Framework (ESP-IDF). The project demonstrates the use of network & device management, data collection, and integration with cloud services.

**Key Features**:

- **WiFi Connectivity Management**: The ESP32 can be configured to act in both station mode and as an access point. The system handles automatic reconnections, credential management & storing, and user-generated disconnects (using the BOOT button).

- **HTTP Server and Web Interface**: An HTTP server is created on the ESP32 that hosts a web interface to interact with the device. Through the web page, users can configure device settings, get real-time DHT22 sensor data, and change firmware (through OTA firmware updates).

- **Non-Volatile Storage (NVS) Handling**: Utilizes the ESP32’s NVS to store and retrieve configurations across reboots, ensuring device settings like WiFi credentials are preserved.

- **Time Synchronization**: Integrates SNTP (Simple Network Time Protocol) to ensure the device maintains accurate time, which is critical for logging and scheduling tasks accurately.

- **AWS IoT Integration**: Connects with AWS IoT Core to publish and subscribe to MQTT topics, enabling the device to interact with cloud-based services and applications. Beneficial for remote monitoring (i.e., checking temperature and humidity of your room remotely).

- **Sensor Data Management**: While the DHT22 sensor is used here, this framework can work for any other sensor.

- **FreeRTOS Concepts**: The project employs several key FreeRTOS concepts to manage concurrency in multitasking environments:
  - **Task Management**: Uses tasks to handle different parts of the system like sensor data management, WiFi handling, and time synchronization concurrently.
  - **Inter-task Communication**: Implements queues and event groups to facilitate communication between tasks, particularly in managing HTTP requests and WiFi events.
  - **Synchronization**: Utilizes semaphores and mutexes to manage resource access, ensuring that shared resources like sensor data buffers and NVS storage are accessed safely among tasks.
  - **Real-time Operation**: Employs timers and real-time tasks scheduling to ensure timely operations, crucial for tasks like periodic sensor readings and time synchronization.

**Technologies Used**:
- ESP-IDF (Espresso IoT Development Framework)
- MQTT for messaging
- AWS IoT for cloud integration
- HTML/CSS/JavaScript for the web interface
