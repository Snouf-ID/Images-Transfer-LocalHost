#include "MainServer.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include "WindowsFileDiag.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

constexpr char ack_message[] = "ACK:image_received";
std::string saveDirectory;

// Classe Session
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket)
        : ws_(std::move(socket)) {}

    // Lance la session WebSocket
    void run() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            self->on_accept(ec);
            });
    }

    // Ferme proprement le WebSocket
    void close() {
        ws_.async_close(websocket::close_code::normal,
            [self = shared_from_this()](beast::error_code ec) {
                if (ec) {
                    std::cerr << "Erreur lors de la fermeture du socket : " << ec.message() << std::endl;
                }
                else {
                    std::cout << "Socket fermé proprement." << std::endl;
                }
            });
    }

private:
    websocket::stream<tcp::socket> ws_;

    void process_binary_message(beast::flat_buffer& buffer)
    {
        auto data = boost::asio::buffer_cast<const uint8_t*>(buffer.data());
        auto size = buffer.size();

        // Vérifier la longueur minimale pour contenir un préfixe
        if (size < 4) {
            std::cerr << "Données binaires reçues trop courtes !" << std::endl;
            return;
        }

        // Extraire la longueur du nom du fichier
        uint32_t name_length = *reinterpret_cast<const uint32_t*>(data);
        if (size < 4 + name_length) {
            std::cerr << "Taille de message incohérente !" << std::endl;
            return;
        }

        // Extraire le nom du fichier
        std::string file_name(reinterpret_cast<const char*>(data + 4), name_length);

        // Extraire le contenu binaire
        const uint8_t* file_content = data + 4 + name_length;
        size_t file_size = size - 4 - name_length;

        // Sauvegarder le fichier
        save_file(file_name, file_content, file_size);

        // Simuler un délai (attention : ceci bloque le thread !)
        //std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // Délai de 1 seconde

        // Envoyer une confirmation
        ws_.async_write(
            boost::asio::buffer(ack_message),
            [](beast::error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "Erreur lors de l'envoi de l'ACK : " << ec.message() << std::endl;
                }
            });
    }

    // Callback d'acceptation
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "Erreur lors de l'acceptation : " << ec.message() << std::endl;
            return;
        }
        // Démarre la lecture après acceptation
        do_read();
    }

    // Lecture des messages
    void do_read() {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                if (ec) {
                    std::cerr << "Erreur lors de la lecture : " << ec.message() << std::endl;
                    return;
                }
                // Traitement des données reçues ici
                if (self->ws_.got_binary())
                {
                    self->process_binary_message(self->buffer_);
                }
                else
                {
                    // Si ce n'est pas binaire, affiche le message
                    std::cout << "Message texte reçu : "
                        << beast::buffers_to_string(self->buffer_.data()) << std::endl;
                }

                // Efface le buffer après traitement
                self->buffer_.consume(self->buffer_.size());

                // Redémarre la lecture ou ferme la session si nécessaire
                self->do_read(); // Ou appelez `self->close();` ici pour fermer
            });
    }

    void save_file(const std::string& file_name, const uint8_t* data, size_t size)
    {
        std::string fullPath = saveDirectory + "\\" + file_name;

        // Sauvegarder dans un fichier
        std::ofstream out_file(fullPath, std::ios::binary);
        if (!out_file) {
            std::cerr << "Impossible d'ouvrir le fichier pour écriture : " << fullPath << std::endl;
            return;
        }
        out_file.write(reinterpret_cast<const char*>(data), size);
        out_file.close();

        std::cout << "Fichier sauvegardé : " << file_name << " (" << size << " octets)" << std::endl;
    }

    beast::flat_buffer buffer_;
};


class WebSocketServer {
public:
    WebSocketServer(net::io_context& ioc, tcp::endpoint endpoint)
        : acceptor_(ioc, endpoint) {
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<Session>(std::move(socket))->run();
            accept(); // Accepter les prochaines connexions
            });
    }

    tcp::acceptor acceptor_;
};

static void print_local_IPv4(net::io_context& io_context)
{
    try {
        // Obtenez une liste des interfaces réseau locales
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
        auto results = resolver.resolve(query);

        // Parcourez les résultats pour afficher les adresses IPv4
        for (auto const& entry : results) {
            auto endpoint = entry.endpoint();
            if (endpoint.address().is_v4()) { // Filtrer uniquement les IPv4
                std::cout << "Voici votre adress IPV4 local a indiquer sur la page WEB : " << endpoint.address().to_string() << std::endl;
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
    }
}

int main() {
    try {
        saveDirectory = WindowsFileDiag::select_folder();
        if (saveDirectory.empty()) {
            std::cerr << "Aucun dossier sélectionné. Fermeture du serveur." << std::endl;
            return -1;
        }

        net::io_context ioc;

        print_local_IPv4(ioc);

        tcp::endpoint endpoint(tcp::v4(), 5000);
        WebSocketServer server(ioc, endpoint);
        std::cout << "Serveur WebSocket en écoute sur toute les interfaces réseau disponible en ipv4 sur le port ws://XXXX:5000" << std::endl;
        //ioc.stop();
        ioc.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception : " << e.what() << std::endl;
    }
}