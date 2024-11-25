#include "MainServer.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

constexpr char ack_message[] = "ACK:image_received";

// Fonction pour identifier le type de fichier à partir des données binaires
static std::string identify_image_format(const std::vector<uint8_t>& data)
{
    if (data.size() < 8) {
        return "unknown"; // Trop court pour être valide
    }

    // Vérification des signatures
    if (data[0] == 0xFF && data[1] == 0xD8) {
        return "jpg";
    }
    else if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return "png";
    }
    else if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46) {
        return "gif";
    }
    else if (data[0] == 0x42 && data[1] == 0x4D) {
        return "bmp";
    }
    else if ((data[0] == 0x49 && data[1] == 0x49) || (data[0] == 0x4D && data[1] == 0x4D)) {
        return "tiff";
    }

    return "unknown"; // Format non reconnu
}

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
                    // Extraire les données binaires dans un vecteur
                    std::vector<uint8_t> data(boost::asio::buffer_cast<const uint8_t*>(self->buffer_.data()),
                        boost::asio::buffer_cast<const uint8_t*>(self->buffer_.data()) + self->buffer_.size());

                    // Identifier le format d'image
                    static int number_icr = 0; // need atomic ?
                    number_icr++;

                    std::string extension = identify_image_format(data);
                    std::string filename = "image_received_" + std::to_string(number_icr) + "." + extension;

                    self->save_image(filename, self->buffer_);

                    // Simuler un délai (attention : ceci bloque le thread !)
                    //std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // Délai de 1 seconde

                    self->ws_.async_write(
                        boost::asio::buffer(ack_message),
                        [](beast::error_code ec, std::size_t bytes_transferred) {
                            if (ec) {
                                std::cerr << "Erreur lors de l'envoi de l'ACK : " << ec.message() << std::endl;
                            }
                        });
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

    // Fonction pour sauvegarder les données reçues comme une image
    void save_image(const std::string& filename, const beast::flat_buffer& buffer)
    {
        std::ofstream file(filename, std::ios::binary); // Ouvre en mode binaire
        if (!file) {
            std::cerr << "Erreur : impossible d'ouvrir le fichier " << filename << " pour écriture." << std::endl;
            return;
        }

        // Écrit les données dans le fichier
        file.write(static_cast<const char*>(buffer.data().data()), buffer.size());
        if (file) {
            std::cout << "Image sauvegardée sous : " << filename << std::endl;
        }
        else {
            std::cerr << "Erreur : écriture échouée." << std::endl;
        }
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