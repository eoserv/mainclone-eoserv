
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "socket.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>

#include "socket_impl.hpp"

#if defined(WIN32) || defined(WIN64)
static WSADATA socket_wsadata;
#endif // defined(WIN32) || defined(WIN64)

#include "util.hpp"

static char ErrorBuf[1024];

static std::size_t eoserv_strlcpy(char *dest, const char *src, std::size_t size)
{
	if (size > 0)
	{
		std::size_t src_size = std::strlen(src);

		if (src_size > size)
			size = src_size;

		std::memcpy(dest, src, size-1);
		dest[size-1] = '\0';
	}

	return size;
}

#if defined(WIN32) || defined(WIN64)
const char *OSErrorString()
{
	int error = GetLastError();

	if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, ErrorBuf, 1023, 0))
	{
		std::snprintf(ErrorBuf, 1024, "Unknown error %i", error);
	}
	else
	{
		eoserv_strlcpy(ErrorBuf, util::trim(ErrorBuf).c_str(), 1024);
	}

	return ErrorBuf;
}
#else // defined(WIN32) || defined(WIN64)
// POSIX string.h for strerror
#include <string.h>
const char *OSErrorString()
{
	eoserv_strlcpy(ErrorBuf, strerror(errno), sizeof(ErrorBuf));
}
#endif // defined(WIN32) || defined(WIN64)

void Socket_Init::init()
{
#if defined(WIN32) || defined(WIN64)
	if (WSAStartup(MAKEWORD(2,0), &socket_wsadata) != 0)
	{
		throw Socket_InitFailed(OSErrorString());
	}
#endif // defined(WIN32) || defined(WIN64)
}

static Socket_Init socket_init;

IPAddress::IPAddress()
{
	this->SetInt(0);
}

IPAddress::IPAddress(std::string str_addr)
{
	this->SetString(str_addr);
}

IPAddress::IPAddress(const char *str_addr)
{
	this->SetString(str_addr);
}

IPAddress::IPAddress(unsigned int addr)
{
	this->SetInt(addr);
}

IPAddress::IPAddress(unsigned char o1, unsigned char o2, unsigned char o3, unsigned char o4)
{
	this->SetOctets(o1, o2, o3, o4);
}

IPAddress IPAddress::Lookup(std::string host)
{
	addrinfo hints;
	addrinfo *ai;
	IPAddress ipaddr;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(host.c_str(), 0, &hints, &ai) != 0)
	{
		ipaddr.address = 0;
	}
	else
	{
		ipaddr.address = ntohl(reinterpret_cast<sockaddr_in *>(ai->ai_addr)->sin_addr.s_addr);
	}

	freeaddrinfo(ai);

	return ipaddr;
}

IPAddress &IPAddress::SetInt(unsigned int addr)
{
	this->address = addr;
	return *this;
}

IPAddress &IPAddress::SetOctets(unsigned char o1, unsigned char o2, unsigned char o3, unsigned char o4)
{
	this->address = o1 << 24 | o2 << 16 | o3 << 8 | o4;
	return *this;
}

IPAddress &IPAddress::SetString(const char *str_addr)
{
	return this->SetString(std::string(str_addr));
}

IPAddress &IPAddress::SetString(std::string str_addr)
{
	unsigned int io1, io2, io3, io4;
	std::sscanf(str_addr.c_str(), "%u.%u.%u.%u", &io1, &io2, &io3, &io4);
	unsigned char o1 = io1, o2 = io2, o3 = io3, o4 = io4;
	this->address = o1 << 24 | o2 << 16 | o3 << 8 | o4;
	return *this;
}

IPAddress &IPAddress::operator =(std::string str_addr)
{
	return this->SetString(str_addr);
}

IPAddress &IPAddress::operator =(const char *str_addr)
{
	return this->SetString(str_addr);
}

IPAddress &IPAddress::operator =(unsigned int addr)
{
	return this->SetInt(addr);
}

unsigned int IPAddress::GetInt() const
{
	return this->address;
}

std::string IPAddress::GetString() const
{
	char buf[16];
	unsigned char o1, o2, o3, o4;
	o1 = (this->address & 0xFF000000) >> 24;
	o2 = (this->address & 0x00FF0000) >> 16;
	o3 = (this->address & 0x0000FF00) >> 8;
	o4 = this->address & 0x000000FF;
	std::sprintf(buf, "%u.%u.%u.%u", o1, o2, o3, o4);
	return std::string(buf);
}

IPAddress::operator unsigned int() const
{
	return this->address;
}

IPAddress::operator std::string() const
{
	return this->GetString();
}

bool IPAddress::operator ==(const IPAddress &other) const
{
	return (this->address == other.address);
}

struct Client::impl_
{
	SOCKET sock;
	sockaddr_in sin;

	impl_(const SOCKET &sock = SOCKET(), const sockaddr_in &sin = sockaddr_in())
		: sock(sock)
		, sin(sin)
	{ }
};

Client::Client()
	: impl(new impl_(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
	, connected(false)
	, server(0)
	, recv_buffer_max(-1U)
	, send_buffer_max(-1U)
	, connect_time(0)
{ }

Client::Client(const IPAddress &addr, uint16_t port)
	: impl(new impl_(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
	, connected(false)
	, server(0)
	, recv_buffer_max(-1U)
	, send_buffer_max(-1U)
	, connect_time(0)
{
	this->Connect(addr, port);
}

Client::Client(Server *server)
	: impl(new impl_(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
	, connected(false)
	, server(server)
	, recv_buffer_max(-1U)
	, send_buffer_max(-1U)
	, connect_time(0)
{ }

Client::Client(const Socket &sock, Server *server)
	: impl(new impl_(sock.sock, sock.sin))
	, connected(true)
	, server(server)
	, recv_buffer_max(-1U)
	, send_buffer_max(-1U)
	, connect_time(0)
{ }

bool Client::Connect(const IPAddress &addr, uint16_t port)
{
	std::memset(&this->impl->sin, 0, sizeof(this->impl->sin));
	this->impl->sin.sin_family = AF_INET;
	this->impl->sin.sin_addr.s_addr = htonl(addr);
	this->impl->sin.sin_port = htons(port);

	if (connect(this->impl->sock, reinterpret_cast<sockaddr *>(&this->impl->sin), sizeof(this->impl->sin)) != 0)
	{
		return this->connected = false;
	}

	return this->connected = true;
}

void Client::Bind(const IPAddress &addr, uint16_t port)
{
	sockaddr_in sin;
	uint16_t portn = htons(port);

	std::memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(addr);
	sin.sin_port = portn;

	if (bind(this->impl->sock, reinterpret_cast<sockaddr *>(&sin), sizeof(sin)) == SOCKET_ERROR)
	{
		throw Socket_BindFailed(OSErrorString());
	}
}

std::string Client::Recv(std::size_t length)
{
	if (length <= 0)
	{
		return "";
	}
	std::string ret = this->recv_buffer.substr(0, length);
	this->recv_buffer.erase(0, length);
	return ret;
}

void Client::Send(const std::string &data)
{
	this->send_buffer += data;

	if (this->send_buffer.length() > this->send_buffer_max)
	{
		this->connected = false;
	}
}

#if defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)
void Client::Tick(double timeout)
{
	pollfd fd;

	fd.fd = this->impl->sock;
	fd.events = POLLIN;

	if (this->send_buffer.length() > 0)
	{
		fd.events |= POLLOUT;
	}

	int result = poll(&fd, 1, long(timeout * 1000));

	if (result == -1)
	{
		throw Socket_SelectFailed(OSErrorString());
	}

	if (result > 0)
	{
		if (fd.revents & POLLERR || fd.revents & POLLHUP || fd.revents & POLLNVAL)
		{
			this->Close();
			return;
		}

		if (fd.revents & POLLIN)
		{
			char buf[32767];
			int recieved = recv(this->impl->sock, buf, 32767, 0);
			if (recieved > 0)
			{
				this->recv_buffer.append(buf, recieved);
			}
			else
			{
				this->Close();
				return;
			}

			if (this->recv_buffer.length() > this->recv_buffer_max)
			{
				this->Close();
				return;
			}
		}

		if (fd.revents & POLLOUT)
		{
			int written = send(this->impl->sock, this->send_buffer.c_str(), this->send_buffer.length(), 0);
			if (written == SOCKET_ERROR)
			{
				this->Close();
				return;
			}
			this->send_buffer.erase(0,written);
		}
	}
}
#else // defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)
void Client::Tick(double timeout)
{
	fd_set read_fds, write_fds, except_fds;
	long tsecs = long(timeout);
	timeval timeout_val = {tsecs, long((timeout - double(tsecs))*1000000)};

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);

	if (this->recv_buffer.length() < this->recv_buffer_max)
	{
		FD_SET(this->impl->sock, &read_fds);
	}

	if (this->send_buffer.length() > 0)
	{
		FD_SET(this->impl->sock, &write_fds);
	}

	FD_SET(this->impl->sock, &except_fds);

	int result = select(this->impl->sock+1, &read_fds, &write_fds, &except_fds, &timeout_val);
	if (result == -1)
	{
		throw Socket_SelectFailed(OSErrorString());
	}

	if (result > 0)
	{
		if (FD_ISSET(this->impl->sock, &except_fds))
		{
			this->Close();
			return;
		}

		if (FD_ISSET(this->impl->sock, &read_fds))
		{
			char buf[32767];
			int recieved = recv(this->impl->sock, buf, 32767, 0);
			if (recieved > 0)
			{
				this->recv_buffer.append(buf, recieved);
			}
			else
			{
				this->Close();
				return;
			}

			if (this->recv_buffer.length() > this->recv_buffer_max)
			{
				this->Close();
				return;
			}
		}

		if (FD_ISSET(this->impl->sock, &write_fds))
		{
			int written = send(this->impl->sock, this->send_buffer.c_str(), this->send_buffer.length(), 0);
			if (written == SOCKET_ERROR)
			{
				this->Close();
				return;
			}
			this->send_buffer.erase(0,written);
		}
	}
}
#endif // defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)

bool Client::Connected()
{
	return this->connected;
}

void Client::Close(bool force)
{
	if (this->connected)
	{
		this->connected = false;
		this->closed_time = std::time(0);

		// This won't work properly for the first 2 seconds of 1 January 1970
		if (force)
		{
			this->closed_time = 0;
		}
	}
}

IPAddress Client::GetRemoteAddr()
{
	return IPAddress(ntohl(this->impl->sin.sin_addr.s_addr));
}

time_t Client::ConnectTime()
{
	return this->connect_time;
}

Client::~Client()
{
	if (this->connected)
	{
		this->Close();
	}
}

struct Server::impl_
{
	fd_set read_fds;
	fd_set write_fds;
	fd_set except_fds;
	SOCKET sock;

	impl_(const SOCKET &sock = INVALID_SOCKET)
		: sock(sock)
	{ }
};

Server::Server()
	: impl(new impl_(socket(AF_INET, SOCK_STREAM, 0)))
	, state(Created)
	, recv_buffer_max(128 * 1024)
	, send_buffer_max(128 * 1024)
	, maxconn(0)
{ }

Server::Server(const IPAddress &addr, uint16_t port)
	: impl(new impl_(socket(AF_INET, SOCK_STREAM, 0)))
	, state(Created)
	, recv_buffer_max(128 * 1024)
	, send_buffer_max(128 * 1024)
	, maxconn(0)
{
	this->Bind(addr, port);
}

void Server::Bind(const IPAddress &addr, uint16_t port)
{
	sockaddr_in sin;
	this->address = addr;
	this->port = port;
	this->portn = htons(port);

	std::memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(this->address);
	sin.sin_port = this->portn;

	if (bind(this->impl->sock, reinterpret_cast<sockaddr *>(&sin), sizeof(sin)) == SOCKET_ERROR)
	{
		this->state = Invalid;
		throw Socket_BindFailed(OSErrorString());
	}

	this->state = Bound;
}

void Server::Listen(int maxconn, int backlog)
{
	this->maxconn = maxconn;

	//if (this->state == Bound)
	//{
		if (listen(this->impl->sock, backlog) != SOCKET_ERROR)
		{
			this->state = Listening;
			return;
		}
	//}

	this->state = Invalid;
	throw Socket_ListenFailed(OSErrorString());
}

Client *Server::Poll()
{
	SOCKET newsock;
	sockaddr_in sin;
	socklen_t addrsize = sizeof(sockaddr_in);
	Client *newclient;
#if defined(WIN32) || defined(WIN64)
	unsigned long nonblocking;
#endif // defined(WIN32) || defined(WIN64)

	if (this->clients.size() >= this->maxconn)
	{
		if ((newsock = accept(this->impl->sock, reinterpret_cast<sockaddr *>(&sin), &addrsize)) != INVALID_SOCKET)
		{
#if defined(WIN32) || defined(WIN64)
			closesocket(newsock);
#else // defined(WIN32) || defined(WIN64)
			close(newsock);
#endif // defined(WIN32) || defined(WIN64)
			return 0;
		}
	}

#if defined(WIN32) || defined(WIN64)
	nonblocking = 1;
	ioctlsocket(this->impl->sock, FIONBIO, &nonblocking);
#else // defined(WIN32) || defined(WIN64)
	fcntl(this->impl->sock, F_SETFL, FNONBLOCK|FASYNC);
#endif // defined(WIN32) || defined(WIN64)
	if ((newsock = accept(this->impl->sock, reinterpret_cast<sockaddr *>(&sin), &addrsize)) == INVALID_SOCKET)
	{
		return 0;
	}
#if defined(WIN32) || defined(WIN64)
	nonblocking = 0;
	ioctlsocket(this->impl->sock, FIONBIO, &nonblocking);
#else // defined(WIN32) || defined(WIN64)
	fcntl(this->impl->sock, F_SETFL, 0);
#endif // defined(WIN32) || defined(WIN64)

	newclient = this->ClientFactory(Socket(newsock, sin));
	newclient->send_buffer_max = this->send_buffer_max;
	newclient->recv_buffer_max = this->recv_buffer_max;
	newclient->connect_time = time(0);

	this->clients.push_back(newclient);

	return newclient;
}

#if defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)
std::vector<Client *> *Server::Select(double timeout)
{
	static std::vector<Client *> selected;
	std::vector<pollfd> fds;
	int result;
	pollfd fd;

	fds.reserve(this->clients.size() + 1);

	fd.fd = this->impl->sock;
	fd.events = POLLERR;
	fds.push_back(fd);

	UTIL_FOREACH(this->clients, client)
	{
		fd.fd = client->impl->sock;

		fd.events = 0;

		if (client->recv_buffer.length() < client->recv_buffer_max)
		{
			fd.events |= POLLIN;
		}

		if (client->send_buffer.length() > 0)
		{
			fd.events |= POLLOUT;
		}

		fds.push_back(fd);
	}

	result = poll(&fds[0], fds.size(), long(timeout * 1000));

	if (result == -1)
	{
		throw Socket_SelectFailed(OSErrorString());
	}

	if (result > 0)
	{
		if (fds[0].revents & POLLERR == POLLERR)
		{
			throw Socket_Exception("There was an exception on the listening socket.");
		}

		int i = 0;
		UTIL_FOREACH(this->clients, client)
		{
			++i;
			if (fds[i].revents & POLLERR || fds[i].revents & POLLHUP || fds[i].revents & POLLNVAL)
			{
				client->Close();
				continue;
			}

			if (fds[i].revents & POLLIN)
			{
				char buf[32767];
				int recieved = recv(client->impl->sock, buf, 32767, 0);
				if (recieved > 0)
				{
					client->recv_buffer.append(buf, recieved);
				}
				else
				{
					client->Close();
					continue;
				}

				if (client->recv_buffer.length() > client->recv_buffer_max)
				{
					client->Close();
					continue;
				}
			}

			if (fds[i].revents & POLLOUT)
			{
				int written = send(client->impl->sock, client->send_buffer.c_str(), client->send_buffer.length(), 0);
				if (written == SOCKET_ERROR)
				{
					client->Close();
					continue;
				}
				client->send_buffer.erase(0,written);
			}
		}
	}

	UTIL_FOREACH(this->clients, client)
	{
		if (client->connected || client->recv_buffer.length() > 0)
		{
			selected.push_back(client);
		}
	}

	return &selected;
}
#else // defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)
std::vector<Client *> *Server::Select(double timeout)
{
	long tsecs = long(timeout);
	timeval timeout_val = {tsecs, long((timeout - double(tsecs))*1000000)};
	static std::vector<Client *> selected;
	SOCKET nfds = this->impl->sock;
	int result;

	FD_ZERO(&this->impl->read_fds);
	FD_ZERO(&this->impl->write_fds);
	FD_ZERO(&this->impl->except_fds);

	UTIL_FOREACH(this->clients, client)
	{
		if (client->recv_buffer.length() < client->recv_buffer_max)
		{
			FD_SET(client->impl->sock, &this->impl->read_fds);
		}

		if (client->send_buffer.length() > 0)
		{
			FD_SET(client->impl->sock, &this->impl->write_fds);
		}

		FD_SET(client->impl->sock, &this->impl->except_fds);

		if (client->impl->sock > nfds)
		{
			nfds = client->impl->sock;
		}
	}

	FD_SET(this->impl->sock, &this->impl->except_fds);

	result = select(nfds+1, &this->impl->read_fds, &this->impl->write_fds, &this->impl->except_fds, &timeout_val);

	if (result == -1)
	{
		throw Socket_SelectFailed(OSErrorString());
	}

	if (result > 0)
	{
		if (FD_ISSET(this->impl->sock, &this->impl->except_fds))
		{
			throw Socket_Exception("There was an exception on the listening socket.");
		}

		UTIL_FOREACH(this->clients, client)
		{
			if (FD_ISSET(client->impl->sock, &this->impl->except_fds))
			{
				client->Close();
				continue;
			}

			if (FD_ISSET(client->impl->sock, &this->impl->read_fds))
			{
				char buf[32767];
				int recieved = recv(client->impl->sock, buf, 32767, 0);
				if (recieved > 0)
				{
					client->recv_buffer.append(buf, recieved);
				}
				else
				{
					client->Close();
					continue;
				}

				if (client->recv_buffer.length() > client->recv_buffer_max)
				{
					client->Close();
					continue;
				}
			}

			if (FD_ISSET(client->impl->sock, &this->impl->write_fds))
			{
				int written = send(client->impl->sock, client->send_buffer.c_str(), client->send_buffer.length(), 0);
				if (written == SOCKET_ERROR)
				{
					client->Close();
					continue;
				}
				client->send_buffer.erase(0,written);
			}
		}
	}

	UTIL_FOREACH(this->clients, client)
	{
		if (client->recv_buffer.length() > 0)
		{
			selected.push_back(client);
		}
	}

	return &selected;
}
#endif // defined(SOCKET_POLL) && !defined(WIN32) && !defined(WIN64)

void Server::BuryTheDead()
{
	// TODO: Optimize
	restart_loop:
	UTIL_IFOREACH(this->clients, it)
	{
		Client *client = *it;

		if (!client->Connected() && ((client->send_buffer.length() == 0 && client->recv_buffer.length() == 0) || client->closed_time + 2 < std::time(0)))
		{
#if defined(WIN32) || defined(WIN64)
			closesocket(client->impl->sock);
#else // defined(WIN32) || defined(WIN64)
			close(client->impl->sock);
#endif // defined(WIN32) || defined(WIN64)
			delete client;
			this->clients.erase(it);
			goto restart_loop;
		}
	}
}

Server::~Server()
{
#if defined(WIN32) || defined(WIN64)
	closesocket(this->impl->sock);
#else // defined(WIN32) || defined(WIN64)
	close(this->impl->sock);
#endif // defined(WIN32) || defined(WIN64)
}
