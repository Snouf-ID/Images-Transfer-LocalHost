#include "WindowsFileDiag.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

constexpr char ACK_MESSAGE[] = "ACK:image_received";
constexpr uint_least16_t APP_PORT = 5000;

constexpr uint32_t MAX_FILE_SAME_NAME = 500'000;
constexpr uint32_t DATA_FILE_RECEIVE_HEADER_SIZE = 12; // sizeof(uint32_t) + sizeof(double)

std::string global_save_directory_path = "";

static std::string generate_unique_file_path(const std::string& directory, const std::string& file_name)
{
    const std::filesystem::path dir_path(directory);
    const std::filesystem::path file_path = dir_path / file_name;

    // File name not take, so we can use it
    if (!std::filesystem::exists(file_path))
    {
        return file_path.string();
    }

    const std::string& base_name = file_path.stem().string();
    const std::string& extension = file_path.extension().string();

    uint32_t counter = 1;

    // Generate new name
    while (true)
    {
        const std::string& new_file_name = base_name + "_" + std::to_string(counter) + extension;
        const std::filesystem::path& new_file_path = dir_path / new_file_name;

        if (!std::filesystem::exists(new_file_path))
        {
            return new_file_path.string();
        }

        counter++;

        if (counter > MAX_FILE_SAME_NAME)
        {
            throw std::runtime_error("Error generating name, more than 500 000 files with the same name, program close");
        }
    }

    return "";
}

static void save_file(const std::string& file_name, const uint8_t* data, size_t size, double last_modified)
{
    const std::string& full_path = generate_unique_file_path(global_save_directory_path, file_name);

    std::ofstream out_file(full_path, std::ios::binary);
    if (!out_file)
    {
        throw std::ios_base::failure("Failed to open file: " + full_path + " for writing");

        return;
    }
    out_file.write(reinterpret_cast<const char*>(data), size);
    out_file.close();

    WindowsFileDiag::apply_last_modified_date_on_file(full_path, last_modified);
    WindowsFileDiag::apply_metadata_date_on_file(full_path);

    std::cout << "File save : " << file_name << " (" << size << " octets)" << std::endl;
}

/**
 * @class Session
 * @brief Manages a single WebSocket session for communication with a client.
 *
 * This class represents an individual WebSocket connection. It handles the WebSocket handshake,
 * processes incoming binary data, and manages communication with the client asynchronously.
 * The session is managed using shared ownership via `std::enable_shared_from_this` to ensure
 * proper lifetime handling.
 *
 * The `run()` function initiates the WebSocket handshake and starts listening for incoming messages.
 * The `do_read()` function continuously reads messages asynchronously, while `process_binary_message()`
 * handles the binary data received by extracting metadata and saving the file to disk.
 *
 * Errors during communication or processing are reported using exceptions.
 *
 * @note This implementation is designed to handle binary WebSocket messages containing file data
 * in a custom format. Other message types are not expected and will be logged.
 */
class Session : public std::enable_shared_from_this<Session>
{
public:
     /**
     * @brief Initializes the WebSocket session with a given socket.
     *
     * This constructor takes ownership of a TCP socket and initializes the WebSocket stream
     * for communication with the client.
     *
     * @param socket The TCP socket representing the client connection.
     */
    explicit Session(tcp::socket socket)
        : m_ws(std::move(socket))
    {
        [[maybe_unused]] std::size_t last_value = m_ws.read_message_max();
        m_ws.read_message_max(2'147'483'648); // 2 GO max size file receive, if more you need incr value, make a dev for cut un part or discard file
    }

    /**
     * @brief Starts the WebSocket session by performing the handshake.
     *
     * The `run()` function begins the WebSocket handshake asynchronously and, upon success,
     * transitions to reading messages from the client. The handshake ensures the WebSocket
     * protocol is properly established.
     */
    void run()
    {
        m_ws.async_accept([self = shared_from_this()](beast::error_code ec) {
            self->on_accept(ec);
            });
    }

private:
    websocket::stream<tcp::socket> m_ws;
    beast::flat_buffer m_buffer;

    /**
     * @brief Processes binary messages received from the WebSocket client.
     *
     * This function interprets binary data in a custom format. The data contains:
     * - A file name length (4 bytes).
     * - The file's last modification timestamp (8 bytes, double).
     * - The file name (variable length).
     * - The file content.
     *
     * It extracts these components, saves the file to disk, and sends an acknowledgment back
     * to the client.
     *
     * @param buffer A `beast::flat_buffer` containing the binary data from the client.
     *
     * @throws std::runtime_error If the binary data is too short or improperly formatted.
     */
    void process_binary_message(beast::flat_buffer& buffer)
    {
        const uint8_t* data = boost::asio::buffer_cast<const uint8_t*>(buffer.data());
        const size_t size = buffer.size();

        // Vérifier la longueur minimale pour contenir un préfixe
        if (size < DATA_FILE_RECEIVE_HEADER_SIZE)
        {
            throw std::runtime_error("Binary data too short to be parse into file (1)");
            return;
        }

        // Extract file name size value (4 octets)
        const uint32_t name_length = *reinterpret_cast<const uint32_t*>(data);
        data += 4; // sizeof(uint32_t)

        // Extract last modified date value (8 octets, double, little-endian)
        const double last_modified = *reinterpret_cast<const double*>(data);
        data += 8; // sizeof(double)

        // Check if data can contain name of file in size
        if (size < DATA_FILE_RECEIVE_HEADER_SIZE + name_length)
        {
            throw std::runtime_error("Binary data too short to be parse into file (2)");
            return;
        }

        // we can do string view
        const std::string file_name(reinterpret_cast<const char*>(data), name_length);

        data += name_length;

        const uint8_t* file_content = data;
        const size_t file_size = size - DATA_FILE_RECEIVE_HEADER_SIZE - name_length;

        save_file(file_name, file_content, file_size, last_modified);

        // Send confirmation of file is get by server
        m_ws.async_write(
            boost::asio::buffer(ACK_MESSAGE),
            [](beast::error_code ec, std::size_t bytes_transferred) {
                if (ec)
                {
                    throw std::runtime_error("Error while send ACK : " + ec.message());
                }
            });
    }

    /**
     * @brief Handles the WebSocket handshake result.
     *
     * This function is called after the asynchronous WebSocket handshake is completed.
     * If the handshake is successful, it begins reading data from the client.
     *
     * @param ec The error code indicating the result of the handshake.
     */
    void on_accept(beast::error_code ec)
    {
        if (ec)
        {
            throw std::runtime_error("Error on_accept " + ec.message());
            return;
        }
        // After accept new client we launch infinite do_read func for get all this files send
        do_read();
    }

    /**
     * @brief Reads incoming WebSocket messages from the client.
     *
     * The `do_read()` function waits for a message from the client asynchronously. Once a message
     * is received, it determines whether it is binary data or a text message. Binary messages are
     * processed, and the buffer is cleared for the next message.
     *
     * @note This function calls itself recursively to handle multiple messages in sequence.
     */
    void do_read()
    {
        m_ws.async_read(m_buffer,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                if (ec)
                {
                    // if client close, we won't crash server, just notify with console msg
                    const int error_value = ec.value();
                    if (error_value == WSAECONNABORTED)
                    {
                        std::cerr << "Client close connection, he close his internet page, reload internet page or shutdown." << std::endl;
                    }
                    else // else, if it's an unknow error like read fail, we crash server. (we don't want a file download miss at the end)
                    {
                        throw std::runtime_error("Async read fail : " + ec.message() + " " + ec.what() + " " + ec.category().name() + " " + std::to_string(ec.value()) + " " + std::to_string(error_value));
                    }
                    return;
                }
                // Processing of data received here
                if (self->m_ws.got_binary())
                {
                    self->process_binary_message(self->m_buffer);
                }
                else
                {
                    // If it's not binary, show message... this should never happen because we don't do that on HTML client page actualy
                    std::cout << "Message : " << beast::buffers_to_string(self->m_buffer.data()) << std::endl;
                }

                // Clear buffer after we use it
                self->m_buffer.consume(self->m_buffer.size());

                // launch another do_read for other file client send
                self->do_read();
            });
    }
};

/**
 * @class WebSocketServer
 * @brief A WebSocket server that listens for incoming connections on a specified endpoint.
 *
 * This class creates and manages a TCP acceptor that listens for incoming WebSocket connections.
 * It accepts new connections asynchronously and spawns a new session to handle each connection.
 *
 * The `accept()` function is responsible for accepting new client connections asynchronously and
 * invoking the `Session` class to handle communication with the client.
 */
class WebSocketServer
{
public:
    WebSocketServer(net::io_context& ioc, tcp::endpoint endpoint)
        : m_acceptor(ioc, endpoint) {
        accept();
    }

private:
    void accept() {
        m_acceptor.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<Session>(std::move(socket))->run();
            accept(); // Accept next connections
            });
    }

    tcp::acceptor m_acceptor;
};

/**
 * @function print_local_IPv4
 * @brief Retrieves and prints the local IPv4 addresses of network interfaces.
 *
 * This function uses Boost.Asio to resolve and retrieve the network interfaces of the local machine.
 * It filters out the non-IPv4 addresses and iterates over the results to print the available IPv4
 * addresses. This is helpful for identifying the local machine's IP address for use in network-related
 * applications, similar to the `ipconfig` command in the command prompt, but in C++.
 *
 * @param io_context A reference to the Boost.Asio io_context object, which is used for performing
 * networking operations asynchronously.
 *
 * @note The function will only print the IPv4 addresses of the local network interfaces and exclude
 * any non-IPv4 addresses. The printed address can be useful for setting up a server or client connection.
 */
static void print_local_IPv4(net::io_context& io_context)
{
    // local interface list
    boost::asio::ip::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
    auto results = resolver.resolve(query);

    for (auto const& entry : results)
    {
        auto endpoint = entry.endpoint();
        if (endpoint.address().is_v4())
        {
            std::cout << "Important! Here is your local IPV4 address to indicate on the WEB page : " << endpoint.address().to_string() << std::endl;
        }
    }
}

/**
 * @function main
 * @brief Entry point of the program to initialize and run the application.
 *
 * This function serves as the entry point of the program. It starts by using the Windows File Dialog
 * to allow the user to select a directory where data will be saved. If no folder is selected, the
 * program terminates early. After that, a Boost.Asio io_context is set up to handle networking tasks,
 * and the local machine's IPv4 address is retrieved. A WebSocket server is then set up to listen for
 * incoming connections on port 5000, bound to all available IPv4 network interfaces. The io_context is
 * started to run the event loop that processes all network-related operations.
 *
 * @note
 * - If folder selection fails, an error message is displayed, and the program exits with a non-zero status.
 * - The WebSocket server listens on port 5000 for IPv4 connections from any available network interface.
 */
int main()
{
    try
    {
        global_save_directory_path = WindowsFileDiag::open_select_folder_diag_window();
        if (global_save_directory_path.empty())
        {
            std::cerr << "No folder selected. Server closing." << std::endl;
            return EXIT_FAILURE;
        }

        net::io_context ioc;
        print_local_IPv4(ioc);

        tcp::endpoint endpoint(tcp::v4(), APP_PORT);
        WebSocketServer server(ioc, endpoint);

        std::cout << "WebSocket server listening on all network interfaces available in ipv4 on port " << APP_PORT << std::endl;

        ioc.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Something went wrong. Exception : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
