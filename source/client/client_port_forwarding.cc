//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/client_port_forwarding.h"

#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/threading/asio_event_dispatcher.h"
#include "client/port_forwarding_window_proxy.h"

#include <asio/ip/address.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

namespace client {

class ClientPortForwarding::Handler
{
public:
    explicit Handler(ClientPortForwarding* parent);
    ~Handler();

    void dettach();

    void onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket);
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);
    void onRead(const std::error_code& error_code, size_t bytes_transferred);

private:
    ClientPortForwarding* parent_;
    DISALLOW_COPY_AND_ASSIGN(Handler);
};

//--------------------------------------------------------------------------------------------------
ClientPortForwarding::Handler::Handler(ClientPortForwarding* parent)
    : parent_(parent)
{
    DCHECK(parent_);
}

//--------------------------------------------------------------------------------------------------
ClientPortForwarding::Handler::~Handler() = default;

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::Handler::dettach()
{
    parent_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::Handler::onAccept(
    const std::error_code& error_code, asio::ip::tcp::socket socket)
{
    if (parent_)
        parent_->onAccept(error_code, std::move(socket));
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::Handler::onWrite(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (parent_)
        parent_->onWrite(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::Handler::onRead(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (parent_)
        parent_->onRead(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
ClientPortForwarding::ClientPortForwarding(std::shared_ptr<base::TaskRunner> io_task_runner)
    : Client(io_task_runner),
      incoming_message_(std::make_unique<proto::port_forwarding::HostToClient>()),
      outgoing_message_(std::make_unique<proto::port_forwarding::ClientToHost>()),
      handler_(base::make_local_shared<Handler>(this)),
      statistics_timer_(base::WaitableTimer::Type::REPEATED, io_task_runner)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientPortForwarding::~ClientPortForwarding()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::setPortForwardingWindow(
    std::shared_ptr<PortForwardingWindowProxy> port_forwarding_window_proxy)
{
    port_forwarding_window_proxy_ = std::move(port_forwarding_window_proxy);
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::setPortForwardingConfig(const proto::port_forwarding::Config& config)
{
    remote_host_ = config.remote_host();
    remote_port_ = config.remote_port();
    local_port_ = config.local_port();
    command_line_ = config.command_line();

    LOG(LS_INFO) << "Session config received (remote host: " << remote_host_ << " remote port: "
                 << remote_port_ << " local port: " << local_port_ << " command line: "
                 << command_line_ << ")";
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::onSessionStarted()
{
    LOG(LS_INFO) << "Port forwarding session started";

    statistics_timer_.start(std::chrono::seconds(1), [this]()
    {
        if (port_forwarding_window_proxy_)
        {
            PortForwardingWindow::Statistics statistics;
            statistics.rx_bytes = rx_bytes_;
            statistics.tx_bytes = tx_bytes_;

            port_forwarding_window_proxy_->setStatistics(statistics);
        }
    });

    sendPortForwardingRequest();

    if (port_forwarding_window_proxy_)
        port_forwarding_window_proxy_->start();
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::onSessionMessageReceived(
    uint8_t /* channel_id */, const base::ByteArray& buffer)
{
    incoming_message_->Clear();

    if (!base::parse(buffer, incoming_message_.get()))
    {
        LOG(LS_ERROR) << "Unable to parse message";
        return;
    }

    if (incoming_message_->has_data())
    {
        if (!is_started_)
        {
            LOG(LS_ERROR) << "Data received without forwarding result";
            return;
        }

        if (state_ != State::CONNECTED)
        {
            LOG(LS_ERROR) << "Data received without connected socket";
            return;
        }

        proto::port_forwarding::Data* data = incoming_message_->mutable_data();
        bool schedule_write = write_queue_.empty();

        write_queue_.emplace(std::move(*data->mutable_data()));

        if (schedule_write)
            doWrite();
    }
    else if (incoming_message_->has_result())
    {
        const proto::port_forwarding::Result& result = incoming_message_->result();

        if (result.error_code() != proto::port_forwarding::Result::SUCCESS)
        {
            LOG(LS_ERROR) << "Error when connecting on remote side: " << result.error_code();
            return;
        }

        LOG(LS_INFO) << "Port forwarding result is received";

        asio::ip::tcp::endpoint endpoint(asio::ip::address_v6::any(), local_port_);

        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            base::AsioEventDispatcher::currentIoContext());

        std::error_code error_code;
        acceptor_->open(endpoint.protocol(), error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "acceptor_->open failed: "
                          << base::utf16FromLocal8Bit(error_code.message());
            return;
        }

        acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "acceptor_->set_option failed: "
                          << base::utf16FromLocal8Bit(error_code.message());
            return;
        }

        acceptor_->bind(endpoint, error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "acceptor_->bind failed: "
                          << base::utf16FromLocal8Bit(error_code.message());
            return;
        }

        acceptor_->listen(asio::ip::tcp::socket::max_listen_connections, error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "acceptor_->listen failed: "
                          << base::utf16FromLocal8Bit(error_code.message());
            return;
        }

        LOG(LS_INFO) << "Waiting for incomming connection on port " << local_port_;
        state_ = State::ACCEPTING;
        acceptor_->async_accept(
            std::bind(&Handler::onAccept, handler_, std::placeholders::_1, std::placeholders::_2));

        startCommandLine(command_line_);
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::onSessionMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

void ClientPortForwarding::onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Error while accepting connection: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return;
    }

    LOG(LS_INFO) << "Connection accepted on local port " << local_port_;
    state_ = State::CONNECTED;
    socket_ = std::make_unique<asio::ip::tcp::socket>(std::move(socket));

    sendPortForwardingStart();

    is_started_ = true;
    doRead();
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to write: " << base::utf16FromLocal8Bit(error_code.message())
                      << " (" << error_code.value() << ")";
        return;
    }

    rx_bytes_ += bytes_transferred;

    // Delete the sent message from the queue.
    if (!write_queue_.empty())
        write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    if (!write_queue_.empty())
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::onRead(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to read: " << base::utf16FromLocal8Bit(error_code.message())
                      << " (" << error_code.value() << ")";
        return;
    }

    sendPortForwardingData(read_buffer_.data(), bytes_transferred);

    // Read next data from socket.
    doRead();
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::doWrite()
{
    if (!socket_ || write_queue_.empty())
        return;

    const std::string& task = write_queue_.front();
    if (task.empty())
        return;

    // Send the buffer to the recipient.
    asio::async_write(*socket_,
                      asio::buffer(task.data(), task.size()),
                      std::bind(&Handler::onWrite,
                                handler_,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::doRead()
{
    if (!socket_)
        return;

    socket_->async_read_some(
        asio::buffer(read_buffer_.data(), read_buffer_.size()),
        std::bind(&Handler::onRead, handler_, std::placeholders::_1, std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::sendPortForwardingRequest()
{
    outgoing_message_->Clear();

    proto::port_forwarding::Request* request = outgoing_message_->mutable_request();
    request->set_remote_port(remote_port_);

    LOG(LS_INFO) << "Sending port forwarding request for " << remote_host_ << ":" << remote_port_;
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, *outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void ClientPortForwarding::sendPortForwardingStart()
{
    outgoing_message_->Clear();

    proto::port_forwarding::Start* start = outgoing_message_->mutable_start();
    start->set_dummy(1);

    LOG(LS_INFO) << "Sending port forwarding start for " << remote_host_ << ":" << remote_port_;
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, *outgoing_message_);
}

void ClientPortForwarding::sendPortForwardingData(const char* buffer, size_t length)
{
    outgoing_message_->Clear();

    tx_bytes_ += length;

    proto::port_forwarding::Data* data = outgoing_message_->mutable_data();
    data->set_data(buffer, length);

    // Send data to client.
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, *outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
// static
void ClientPortForwarding::startCommandLine(std::string_view command_line)
{
    LOG(LS_INFO) << "Starting command line: " << command_line.data();

    if (command_line.empty())
    {
        LOG(LS_INFO) << "Empty command line";
        return;
    }

#if defined(OS_WIN)
    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);

    if (!CreateProcessW(nullptr,
                        const_cast<wchar_t*>(base::wideFromUtf8(command_line).c_str()),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_UNICODE_ENVIRONMENT,
                        nullptr,
                        nullptr,
                        &startup_info,
                        &process_info))
    {
        PLOG(LS_ERROR) << "CreateProcessAsUserW failed";
        return;
    }

    if (!CloseHandle(process_info.hThread))
    {
        PLOG(LS_ERROR) << "CloseHandle failed";
    }

    if (!CloseHandle(process_info.hProcess))
    {
        PLOG(LS_ERROR) << "CloseHandle failed";
    }

    LOG(LS_INFO) << "Command line is started";
#endif // defined(OS_WIN)
}

} // namespace client
