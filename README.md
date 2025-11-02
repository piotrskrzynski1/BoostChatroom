# Boost Chatroom

A C++ console chatroom using Boost.Asio, built for Windows.

## Features

- Start a server to host a chatroom
- Connect to a server as a client
- Real-time message broadcasting between clients

## Screenshot

<img width="2091" height="1273" alt="background" src="https://github.com/user-attachments/assets/cfdbbf94-85dd-41ba-81db-70733cba069b" />

*Chatroom running on Windows 11 with two clients connected. Client1 successfuly sends a message to Client2 and vice versa.*

## Requirements

- Windows 10 or 11
- C++17 compatible compiler
- Boost.Asio (installed via vcpkg)

## Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/piotrskrzynski1/WinFormsCameraRecorder.git
   ```
2. Open the project folder in Visual Studio.

3. Install vcpkg or use the one provided by Visual Studio.

4. Point CMake to your vcpkg.cmake in CMakePresets.json.

5. Configure CMake and compile your preferred build configuration (Debug/Release).

