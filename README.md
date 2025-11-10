# Boost Chatroom

A C++ console chatroom using Boost.Asio, built for Windows/Linux.

## Features

- Start a server to host a chatroom
- Connect to a server as a client
- Real-time message broadcasting between clients
- Sending Text and Files
- Send/Retry/Pause/Cancel... commands for Files
- Chat History up to 100 messages upon joining

## Screenshots

<img width="2091" height="1273" alt="background" src="https://github.com/user-attachments/assets/cfdbbf94-85dd-41ba-81db-70733cba069b" />

*Chatroom running on Windows 11 with two clients connected. Client1 successfully sends a message to Client2 and vice versa.*

<img width="1445" height="698" alt="image" src="https://github.com/user-attachments/assets/9aad37ba-afab-4749-8ecf-98d402ec92b6" />

*Simplified Project Structure overview.*

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

You can also try using the build.sh bash script, it executes these commands one by one.

## Use Example
1. Run the server application, type your ip (for example 127.0.0.1 loopback or 0.0.0.0 to enable listening on all network interfaces), and ports (by default text port: 5555, file port: 5556)
2. Run the client application, provide info for the server you setup above
3. Send messages by typing a message and pressing enter
4. type /help for more commands (for example /file [path] to send a file to the server and other clients).
5. The fun part is, if someone joins, he'll get your last 100 messages

## Class descriptions
### Server
**ServerMain**: Entry point for the server, captures input from the user and sets up the ServerManager Instance

**ServerManager**: The heart of the server. Manages connections using a multithreaded approach. Actively listens to new clients trying to connect to the socket, and spins up their own async reads for both text and file ports. The server is also responsible for keeping message history, sharing it with newly connected clients, as well as broadcasting actively sent messages (while skipping the sender).

---

### Client
**ClientMain**: Entry point for the client, captures input from the user and sets up both CommandProcessor and ClientServerConnectionManager

**CommandProcessor**: A Command Design Pattern class to allow for easy addition of commands for the client.

**ClientServerConnectionManager**: Similarly to *ServerManager*, manages the clients connection, message sending etc.

---

### Shared
**FileTransferQueue**: A File manager that uses a deque for processing files sequentially. It makes sure the client doesn't get a mix of images because of asynchronous writing by the server and client.

**IMessage**: An interface for the message classes

 -  **FileMessage**: Represents messages that contain files (Bytes)
   
 -  **TextMessage**: Represents messages that contains text (Strings)
   
**MessageFactory**: A Factory design pattern class that uses a creator by id method to make it possible to do changes in one place, and to make the code cleaner.

**MessageReciever**: A class responsible for parsing/reading data received by the socket.

**ServerMessageSender**: A class responsible for sending data via the socket.

## Issues
The program currently doesnt care about file size or text size, this is on purpose to not restrict users, although it can lead to crashes. Can be fixed by checking clients header and verifying that the size part is correct
The Tests don't cover connection testing, they only cover logical tests (for example if serialize()/deserialize() correctly process data)
## OPTIONAL: firewall problems

When you run the server and want incoming traffic to not get filtered run these commands if you have an active firewall on Ubuntu.
```
sudo ufw allow [textport]/tcp
sudo ufw allow [fileport]/tcp
```
other operating systems may need a different command to allow unfiltered traffic on these ports.
You can also use the ready .sh script for that in the project folder.
