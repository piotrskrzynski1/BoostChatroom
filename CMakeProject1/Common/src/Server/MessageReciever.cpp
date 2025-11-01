#include <Server/MessageReciever.h>
#include <MessageTypes/TextMessage.h>
void MessageReciever::handle_read_text(std::shared_ptr<std::vector<char>> buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    if (!error)
    {
        buffer->resize(bytes_transferred);

        TextMessage received_msg;

        try {
            received_msg.deserialize(*buffer);

            std::cout << "Server says (deserialized): " << received_msg.to_string() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialization error: " << e.what() << std::endl;
        }

        this->start_read_header(socket);
    }
    else if (error == boost::asio::error::eof)
    {
        std::cout << "Server closed the connection.\n";
    }
    else
    {
        std::cerr << "Read error: " << error.message() << std::endl;
    }
}


void MessageReciever::start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    //pocz¹tku ka¿dej wiadomoœci sk³ada siê z dwóch liczb typu uint32_t. Pierwsza liczba oznacza typ druga ilosc bajtów
    const size_t header_size = sizeof(uint32_t) * 2;
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*socket, boost::asio::buffer(*header_buffer),
        [header_buffer, this,socket](const boost::system::error_code& err, std::size_t bytes_transferred)
    {
        if (err)
        {
            handle_read_text(header_buffer, err, bytes_transferred,socket);
            return;
        }

        uint32_t body_length;


        std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint32_t));
        body_length = ntohl(body_length);

        if (body_length == 0)
        {
            handle_read_text(header_buffer, boost::system::error_code(), header_buffer->size(), socket);
        }
        else
        {
            start_read_body(header_buffer, body_length,socket);
        }
    });
}

void MessageReciever::start_read_body(std::shared_ptr<std::vector<char>> header_buffer, uint32_t body_length, std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    auto body_buffer = std::make_shared<std::vector<char>>(body_length);

    boost::asio::async_read(*socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer, this,socket](const boost::system::error_code& err, std::size_t bytes_transferred)
    {
        if (err)
        {
            handle_read_text(body_buffer, err, bytes_transferred,socket);
            return;
        }

        auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
        fulldata->insert(fulldata->end(), body_buffer->begin(), body_buffer->end());

        handle_read_text(fulldata, boost::system::error_code(), fulldata->size(),socket);
    });
}