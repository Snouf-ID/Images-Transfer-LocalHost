# PikPok File Transfer

**PikPok File Transfer** is a document transfer program between a C++ application on Windows 10 and an HTML page accessible from any device (PC, Steam Deck, phone). This system allows for simple and quick file transfers via a C++ server and a web HTML client.

## How it Works

The program consists of two main parts:

1. **The Server**: A C++ executable that runs on a Windows 10 machine. This server is essentially a **websocket** that operates with the Boost library and handles file transfer requests by allowing a client to send files over a local network.
   
2. **The Client**: A simple HTML page that can be opened in any web browser. This client sends files to the server via a web interface.

The server and client communicate over **websockets** on the same local network (e.g., via Wi-Fi, mobile hotspot, or Ethernet). The client can select files to send, and the server receives them and saves them in the specified directory.

The file transfer happens seamlessly: once the client sends a file via the web interface, the server receives it and places it in the destination folder, preserving the file's modification and creation information.

### Handling File Dates

In JavaScript, only the **last modified date** of a file is accessible, which can lead to confusion between the creation date and the modification date. Therefore, this modification date is used as the creation date during the file transfer.

However, for **photos**, a special process is applied: we retrieve the **metadata** from the photo (such as the date the photo was taken), and if this metadata exists, it is used as the **creation date** of the file on the server, replacing the modification date.

## Prerequisites

- **Client**: Simply download the HTML page for the client.
- **Server**: Download the `.exe` executable for the server. If you want to compile the server in C++, you will need:
    - **Visual Studio** for compiling the server program.
    - **Boost 1.86.0**: Download and place Boost in a folder named `boost_1_86_0`. This folder is not directly linked to the repository as a submodule.

## Installation and Usage

### 1. Download and set up the server and client:
   - Download the C++ server executable and place it on your Windows 10 machine.
   - Download the HTML page for the client and open it in a compatible web browser.

### 2. Launch the server:
   - Run the server executable on your Windows 10 machine.
   - A pop-up window will appear asking you to select or create a folder where all documents will be saved.

### 3. Launch the client:
   - Open the HTML page in your browser on any device connected to the same local network.

### 4. Send files:
   - Select the files to send from the client interface and click the button to start the transfer. The server will receive the file and save it in the destination folder.

## Security

File transfers via PikPok File Transfer are only possible over the **local network**. This means that the program will only be accessible to devices connected to the same Wi-Fi network, Ethernet, or mobile hotspot. This model limits security risks by restricting access to local connections.

## Additional Notes

- **Compatibility**: This program works only on Windows 10 for the server, but the HTML client is compatible with any device that has a modern web browser.
- **Websocket**: The server does not function as a traditional server but uses a websocket to establish efficient bidirectional communication between the client and server.

## Contribute

Contributions to PikPok File Transfer are welcome. You can propose improvements, bug fixes, or new features by opening an issue or submitting a pull request.

---

PikPok File Transfer simplifies file transfers between different platforms, with an intuitive and simple interface. All of this is done in a secure environment on your local network.
