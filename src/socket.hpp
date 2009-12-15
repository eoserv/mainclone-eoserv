
/* $Id$
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#ifndef SOCKET_HPP_INCLUDED
#define SOCKET_HPP_INCLUDED

#include "fwd/socket.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <list>
#include <string>
#include <vector>

#include "shared.hpp"
#include "util.hpp"

#include "container/ptr_list.hpp"
#include "container/ptr_vector.hpp"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <winsock2.h>
#ifdef NTDDI_WIN2K
#include <Wspiapi.h>
#else // NTDDI_WIN2K
#ifdef __MINGW32__
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //  __MINGW32__
#include <ws2tcpip.h>
#endif // NTDDI_WIN2K

/**
 * Stores the initialization state of WinSock.
 */
extern bool socket_ws_init;

/**
 * Stores internal WinSock information.
 */
extern WSADATA socket_wsadata;

/**
 * Type for storing the size of a POSIX sockaddr_in struct.
 * Defined here because it does not exist in MinGW's winsock headers.
 */
typedef int socklen_t;

#else // defined(WIN32) || defined(WIN64)
// Stop doxygen generating a gigantic include graph
#ifndef DOXYGEN
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#ifdef SOCKET_POLL
#include <sys/poll.h>
#endif // SOCKET_POLL
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif // DOXYGEN

/**
 * Type for storing a socket handle.
 * Defined here because it does not exist in linux headers.
 */
typedef int SOCKET;

/**
 * Socket handle representing an invalid socket.
 */
const SOCKET INVALID_SOCKET = -1;

/**
 * Return code representing a socket error.
 */
const int SOCKET_ERROR = -1;
#endif // defined(WIN32) || defined(WIN64)

/**
 * Generic Socket exception type
 */
class Socket_Exception : public std::exception
{
	protected:
		const char *err;
	public:
		Socket_Exception(const char *e) : err(e) {};
		const char *error() { return err; };
		virtual const char *what() { return "Socket_Exception"; }
};

/**
 * Exception thrown when intializing the socket library failed
 */
class Socket_InitFailed : public Socket_Exception
{
	public:
		Socket_InitFailed(const char *e) : Socket_Exception(e) {}
		const char *what() { return "Socket_InitFailed"; }
};

/**
 * Exception thrown when a call to bind() failed
 */
class Socket_BindFailed : public Socket_Exception
{
	public:
		Socket_BindFailed(const char *e) : Socket_Exception(e) {}
		const char *what() { return "Socket_BindFailed"; }
};

/**
 * Exception thrown when a call to listen() failed
 */
class Socket_ListenFailed : public Socket_Exception
{
	public:
		Socket_ListenFailed(const char *e) : Socket_Exception(e) {}
		const char *what() { return "Socket_ListenFailed"; }
};

/**
 * Exception thrown when a call to select() failed
 */
class Socket_SelectFailed : public Socket_Exception
{
	public:
		Socket_SelectFailed(const char *e) : Socket_Exception(e) {}
		const char *what() { return "Socket_SelectFailed"; }
};

#if defined(WIN32) || defined(WIN64)
inline void _Socket_WSAStartup()
{
	if (!socket_ws_init)
	{
		if (WSAStartup(MAKEWORD(2,0), &socket_wsadata) != 0)
		{
			throw Socket_InitFailed(OSErrorString());
		}
		socket_ws_init = true;
	}
}
#define Socket_WSAStartup() _Socket_WSAStartup()
#else // defined(WIN32) || defined(WIN64)
#define Socket_WSAStartup()
#endif // defined(WIN32) || defined(WIN64)

/**
 * Stores an IP address and converts between string and numeric formats.
 */
class IPAddress
{
	protected:
		/**
		 * Integer version of the IP address.
		 */
		uint32_t address;

		/**
		 * Return the IP address as a string (eg 255.255.255.255).
		 */
		std::string GetString();

	public:
		/**
		 * Initialize the address as 0.0.0.0.
		 */
		IPAddress();

		/**
		 * Initialize the address to the integer value.
		 */
		IPAddress(unsigned int);

		/**
		 * Initialize the address using 4 octets.
		 */
		IPAddress(unsigned char, unsigned char, unsigned char, unsigned char);

		/**
		 * Initialize the address using a string (eg 255.255.255.255).
		 */
		IPAddress(const char *);

		/**
		 * Initialize the address using a POSIX in_addr struct.
		 */
		IPAddress(in_addr);

		/**
		 * Initialize the address using a string (eg 255.255.255.255).
		 */
		IPAddress(std::string);

		/**
		 * Lookup a hostname and use that to create an IPAddress class
		 */
		static IPAddress Lookup(std::string host);

		/**
		 * Set the address to an integer value
		 */
		IPAddress &operator =(unsigned int);

		/**
		 * Set the address using a string (eg 255.255.255.255).
		 */
		IPAddress &operator =(const char *);

		/**
		 * Set the address using a POSIX in_addr struct.
		 */
		IPAddress &operator =(in_addr);

		/**
		 * Set the address using a string (eg 255.255.255.255).
		 */
		IPAddress &operator =(std::string);

		/**
		 * Return the IP address as an integer.
		 */
		operator unsigned int();

		/**
		 * Return the IP address as a POSIX in_addr struct.
		 */
		operator in_addr();

		/**
		 * Return the IP address as a string (eg 255.255.255.255).
		 */
		operator std::string();

		bool operator ==(const IPAddress &) const;
		bool operator <(const IPAddress &) const;

	SCRIPT_REGISTER_REF_DF(IPAddress)

	SCRIPT_REGISTER_END()
};

/**
 * Generic TCP client class.
 */
class Client : public Shared
{
	protected:
		bool connected;
		time_t closed_time;
		SOCKET sock;
		sockaddr_in sin;
		std::string send_buffer;
		std::string recv_buffer;
		Server *server;
		std::size_t recv_buffer_max;
		std::size_t send_buffer_max;
		time_t connect_time;

	public:
		Client();
		Client(IPAddress addr, uint16_t port);
		Client(Server *);
		Client(SOCKET, sockaddr_in, Server *);
		bool Connect(IPAddress addr, uint16_t port);
		void Bind(IPAddress addr, uint16_t port);
		std::string Recv(std::size_t length);
		void Send(const std::string &data);
		void Tick(double timeout);
		bool Connected();
		IPAddress GetRemoteAddr();
		void Close(bool force = false);
		time_t ConnectTime();
		virtual ~Client();

	friend class Server;
};

/**
 * Generic TCP server class.
 */
class Server : public Shared
{
	public:
		enum State
		{
			/**
			 * There was an error preparing the server.
			 */
			Invalid,

			/**
			 * Newly created server, not listening yet.
			 */
			Created,

			/**
			 * Server has been bound to a port but is not yet listening.
			 */
			Bound,

			/**
			 * Server is listening and is ready to accept clients.
			 */
			Listening
		};

		/**
		 * Stores file descriptors for Select().
		 */
		fd_set read_fds;

		/**
		 * Stores file descriptors for Select().
		 */
		fd_set write_fds;

		/**
		 * Stores file descriptors for Select().
		 */
		fd_set except_fds;

		/**
		 * Maximum amount of data that will be buffered for recieving per client.
		 */
		std::size_t recv_buffer_max;

		/**
		 * Maximum amount of data that will be buffered for sending per client.
		 */
		std::size_t send_buffer_max;

		/**
		 * Maximum number of connections the server will hold at one time.
		 */
		unsigned int maxconn;

	private:
		/**
		 * Initializes all data and WinSock if required.
		 * This is called by every Socket constructor.
		 * @throw Socket_InitFailed
		 */
		void Initialize();

	protected:
		/**
		 * The address the server will listen on.
		 */
		IPAddress address;

		/**
		 * The port the server will listen on in host order.
		 */
		uint16_t port;

		/**
		 * The port the server will listen on in network order.
		 */
		uint16_t portn;

		/**
		 * Socket handle of the listener.
		 */
		SOCKET server;

		/**
		 * Current server state.
		 * @sa State
		 */
		State state;

		virtual Client *ClientFactory(SOCKET sock, sockaddr_in sin);

	public:
		/**
		 * List of connected clients.
		 */
		PtrList<Client> clients;

		/**
		 * Initializes the Server.
		 */
		Server()
		{
			this->Initialize();
		}

		/**
		 * Initializes the Server and binds to the specified address and port.
		 * @param addr Address to bind to
		 * @param port Port number to bind to
		 */
		Server(IPAddress addr, uint16_t port)
		{
			this->Initialize();
			this->Bind(addr, port);
		}

		/**
		 * Bind the Server to the specified address and port.
		 * Once this succeeds you should call Listen().
		 * @param addr Address to bind to.
		 * @param port Port number to bind to.
		 * @throw Socket_BindFailed
		 */
		void Bind(IPAddress addr, uint16_t port);

		/**
		 * Bind the Server to the specified address and port.
		 * @param maxconn Maximum number of clients to have at one time.
		 * @param backlog Number of connections to keep in the queue.
		 * @throw Socket_ListenFailed
		 */
		void Listen(int maxconn, int backlog = 10);

		/**
		 * Check for new connection requests.
		 * @return NULL if there are no pending connections, a pointer to the Client otherwise.
		 */
		Client *Poll();

		/**
		 * Check clients for incoming data and errors, and sends data in their send_buffer.
		 * If data is recieved, it is added to their recv_buffer.
		 * @param timeout Max number of seconds to block for
		 * @throw Socket_SelectFailed
		 * @throw Socket_Exception
		 * @return Returns a list of clients that have data in their recv_buffer.
		 */
		PtrVector<Client> *Select(double timeout);

		/**
		 * Destroys any dead clients, should be called periodically.
		 * All pointers to Client objects from this Server should be considered invalid after execution.
		 */
		void BuryTheDead();

		State State()
		{
			return this->state;
		}

		int Connections()
		{
			return this->clients.size();
		}

		int MaxConnections()
		{
			return this->maxconn;
		}

		virtual ~Server()
		{
#if defined(WIN32) || defined(WIN64)
			closesocket(this->server);
#else // defined(WIN32) || defined(WIN64)
			close(this->server);
#endif // defined(WIN32) || defined(WIN64)
		}
};


#endif // SOCKET_HPP_INCLUDED
