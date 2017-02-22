// echo_server.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <string>

using namespace boost::asio;
struct TcpSession;
typedef boost::shared_ptr<ip::tcp::socket> sock_ptr;
typedef boost::shared_ptr<TcpSession> tcpSess_ptr;
typedef buffers_iterator<streambuf::const_buffers_type> iterator;

struct TcpSession : public boost::enable_shared_from_this<TcpSession>
{
	TcpSession(io_service& service) : sock_(service) { }

	void start() 
	{
		sock_.async_read_some(buffer(read_, 1024), boost::bind(&TcpSession::read_handler, shared_from_this(), _1, _2));
	}

	void close()
	{
		if(sock_.is_open())
		{
			sock_.close();
		}
	}

	ip::tcp::socket& get_socket() 
	{
		return sock_;
	}

	void read_handler(const boost::system::error_code& ec, size_t length)
	{
		if(ec)
		{
			std::cout << ec.message() << std::endl;
		}
		else
		{
			std::cout << "received: " << std::string(read_, length) << std::endl;
			sock_.async_write_some(buffer(read_, length), boost::bind(&TcpSession::write_handler, shared_from_this(), _1, _2));
		}
	}

	void write_handler(const boost::system::error_code& ec, size_t length)
	{
		if(ec)
		{
			std::cout << ec.message() << std::endl;
		}
		else
		{
			std::cout << "send: " << std::string(read_, length) << std::endl;
			sock_.async_read_some(buffer(read_, 1024), boost::bind(&TcpSession::read_handler, shared_from_this(), _1, _2));
		}
	}

public:
	ip::tcp::socket sock_;
	char read_[1024];
};


struct Server : public boost::enable_shared_from_this<Server>
{
	Server(io_service* services, ip::tcp::endpoint& ep) 
		: service_(services[0]), accept_(services[1], ep), timer_(services[0], boost::posix_time::seconds(3600))
	{
		start_accept();	
		timer_.async_wait(boost::bind(&Server::timeout_handler, this, _1));
	}

	void start_accept()
	{
		tcpSess_ptr session = boost::shared_ptr<TcpSession>(new TcpSession(service_)); 
		accept_.async_accept(session->get_socket(), boost::bind(&Server::accept_handler, this, session, _1));
	}

	void accept_handler(tcpSess_ptr ptr, const boost::system::error_code& ec)
	{
		if(ec)
		{
			std::cout << ec.message() << std::endl;
			// 清理工作
		}
		else
		{
			ptr->start();
			std::cout << "ip: " << ptr->get_socket().remote_endpoint().address()
				<< " port: " << ptr->get_socket().remote_endpoint().port()
				<< std::endl;
		}
		ptr.reset(new TcpSession(service_)); 
		accept_.async_accept(ptr->get_socket(), boost::bind(&Server::accept_handler, this, ptr, _1));
	}

	void timeout_handler(const boost::system::error_code& ec)
	{
		if(ec)
			std::cout << ec.message() << std::endl;
		timer_.async_wait(boost::bind(&Server::timeout_handler, this, _1));
	}

private:
	io_service& service_;
	ip::tcp::acceptor accept_;
	deadline_timer timer_;
};

void run_service(io_service* service)
{
	service->run();
}

int _tmain(int argc, _TCHAR* argv[])
{
	io_service services[2];
	ip::tcp::endpoint ep(ip::address_v4::any(), 12345);
	Server server(services, ep);
	boost::thread(boost::bind(run_service, &services[0]));
	services[1].run();
	return 0;
}