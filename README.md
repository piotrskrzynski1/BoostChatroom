# Boost Chatroom

A C++ console chatroom using Boost.Asio, built for Windows/Linux.

## Features

- Start a server to host a chatroom
- Connect to a server as a client
- Real-time message broadcasting between clients
- Sending Text and Files
- Send/Retry/Pause/Cancel... commands for Files
- Chat History up to 100 messages upon joining

## Screenshot

<img width="2091" height="1273" alt="background" src="https://github.com/user-attachments/assets/cfdbbf94-85dd-41ba-81db-70733cba069b" />

*Chatroom running on Windows 11 with two clients connected. Client1 successfully sends a message to Client2 and vice versa.*

## Requirements

- Linux Operating System
- C++17 compatible compiler (g++)
- Boost.Asio (installed via vcpkg)
- GTest Library
- Ninja Build Tool

## Example Installation Instructions Step-by-step
1. Clone the repository:
   ```bash
   git clone https://github.com/piotrskrzynski1/BoostChatroom.git
   ```
2. Open a terminal in the project folder.
3. Input commands one by one into the terminal:
```
sudo apt update
sudo apt install cmake ninja-build build-essential libboost-all-dev libgtest-dev
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ ..
ninja
```

