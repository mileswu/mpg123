#include "config.h"
#include "mpg123app.h"
#include "debug.h"

#ifdef WANT_WIN32_UNICODE
int win32_cmdline_utf8(int * argc, char *** argv)
{
	wchar_t **argv_wide;
	char *argvptr;
	int argcounter;

	/* That's too lame. */
	if(argv == NULL || argc == NULL) return -1;

	argv_wide = CommandLineToArgvW(GetCommandLineW(), argc);
	if(argv_wide == NULL){ error("Cannot get wide command line."); return -1; }

	*argv = (char **)calloc(sizeof (char *), *argc);
	if(*argv == NULL){ error("Cannot allocate memory for command line."); return -1; }

	for(argcounter = 0; argcounter < *argc; argcounter++)
	{
		win32_wide_utf8(argv_wide[argcounter], (const char **)&argvptr, NULL);
		(*argv)[argcounter] = argvptr;
	}
	LocalFree(argv_wide); /* We don't need it anymore */

	return 0;
}

void win32_cmdline_free(int argc, char **argv)
{
	int i;

	if(argv == NULL) return;

	for(i=0; i<argc; ++i) free(argv[i]);
}
#endif /* WIN32_WANT_UNICODE */

void win32_set_priority (const int arg)
{
	DWORD proc_result = 0;
	HANDLE current_proc = NULL;
	if (arg) {
	  if ((current_proc = GetCurrentProcess()))
	  {
	    switch (arg) {
	      case -2: proc_result = SetPriorityClass(current_proc, IDLE_PRIORITY_CLASS); break;  
	      case -1: proc_result = SetPriorityClass(current_proc, BELOW_NORMAL_PRIORITY_CLASS); break;  
	      case 0: proc_result = SetPriorityClass(current_proc, NORMAL_PRIORITY_CLASS); break; /*default priority*/
	      case 1: proc_result = SetPriorityClass(current_proc, ABOVE_NORMAL_PRIORITY_CLASS); break;
	      case 2: proc_result = SetPriorityClass(current_proc, HIGH_PRIORITY_CLASS); break;
	      case 3: proc_result = SetPriorityClass(current_proc, REALTIME_PRIORITY_CLASS); break;
	      default: fprintf(stderr,"Unknown priority class specified\n");
	    }
	    if(!proc_result) {
	      fprintf(stderr,"SetPriorityClass failed\n");
	    }
	  }
	  else {
	    fprintf(stderr,"GetCurrentProcess failed\n");
	  }
	}
}
#if defined (WANT_WIN32_SOCKETS)
#ifdef DEBUG
#define msgme(x) win32_net_msg(x,__FILE__,__LINE__)
#define msgme1 win32_net_msg(1,__FILE__,__LINE__)
#else
#define msgme(x) x
#define msgme1 continue
#endif
struct ws_local
{
  int inited:1;
  SOCKET local_socket; /*stores last connet in win32_net_open_connection*/
  WSADATA wsadata;
};

static struct ws_local ws;

static void win32_net_msg (const int err, const char * const filedata, const int linedata)
{
  char *errbuff;
  int lc_err;
  if (err)
  {
    lc_err = WSAGetLastError();
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      lc_err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &errbuff,
      0,
      NULL );
    fprintf(stderr, "[%s:%d] [WSA2: %d] %s", filedata, linedata, lc_err, errbuff);
    LocalFree (errbuff);
  }
}

void win32_net_init (void)
{
  ws.inited = 1;
  switch ((WSAStartup(MAKEWORD(2,2), &ws.wsadata)))
  {
    case WSASYSNOTREADY: debug("WSAStartup failed with WSASYSNOTREADY"); break;
    case WSAVERNOTSUPPORTED: debug("WSAStartup failed with WSAVERNOTSUPPORTED"); break;
    case WSAEINPROGRESS: debug("WSAStartup failed with WSAEINPROGRESS"); break;
    case WSAEPROCLIM: debug("WSAStartup failed with WSAEPROCLIM"); break;
    case WSAEFAULT: debug("WSAStartup failed with WSAEFAULT"); break;
    default:
    break;
  }
}

void win32_net_deinit (void)
{
  debug("Begin winsock cleanup");
  if (ws.inited)
  {
    debug1("ws.local_socket = %d", ws.local_socket);
    msgme(shutdown(ws.local_socket, SD_BOTH));
    closesocket(ws.local_socket);
    WSACleanup();
    ws.inited = 0;
  }
}

static void win32_net_nonblock(SOCKET sock)
{
  u_long mode = 1;
  msgme(ioctlsocket(sock, FIONBIO, &mode));
}

static void win32_net_block(SOCKET sock)
{
  u_long mode = 0;
  msgme(ioctlsocket(sock, FIONBIO, &mode));
}

ssize_t win32_net_read (int fildes, void *buf, size_t nbyte)
{
  debug1("Attempting to read %d bytes from network.", nbyte);
  ssize_t ret;
  ret = (ssize_t) recv(ws.local_socket, buf, nbyte, 0);
  debug1("Read %d bytes from network.", ret);

  if (ret == SOCKET_ERROR) {msgme1;}
  return ret;
}

ssize_t win32_net_write (int fildes, const void *buf, size_t nbyte)
{
  debug1("Attempting to write %d bytes to network.", nbyte);
  ssize_t ret;
  ret = (ssize_t) send(ws.local_socket, buf, nbyte, 0);
  debug1("wrote %d bytes to network.", ret);

  if (ret == SOCKET_ERROR) {msgme1;}

  return ret;
}

off_t win32_net_lseek (int a, off_t b, int c)
{
  debug("lseek on a socket called!");
  return -1;
}

void win32_net_replace (mpg123_handle *fr)
{
  debug("win32_net_replace ran");
  mpg123_replace_reader(fr, win32_net_read, win32_net_lseek);
}

static int win32_net_timeout_connect(SOCKET sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	debug("win32_net_timeout_connect ran");
	if(param.timeout > 0)
	{
		int err;
		win32_net_nonblock(sockfd);
		msgme(err = connect(sockfd, serv_addr, addrlen));
		if(err != SOCKET_ERROR)
		{
			debug("immediately successful");
			win32_net_block(sockfd);
			return 0;
		}
		else if(errno == WSAEINPROGRESS)
		{
			struct timeval tv;
			fd_set fds;
			tv.tv_sec = param.timeout;
			tv.tv_usec = 0;

			debug("in progress, waiting...");

			FD_ZERO(&fds);
			FD_SET(sockfd, &fds);
			msgme(err = select(sockfd+1, NULL, &fds, NULL, &tv));
			if(err > 0)
			{
				socklen_t len = sizeof(err);
				if(   (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&err, &len) != SOCKET_ERROR)
				   && (err == 0) )
				{
					debug("non-blocking connect has been successful");
					win32_net_block(sockfd);
					return 0;
				}
				else
				{
					error1("connection error: %s", msgme(err));
					return -1;
				}
			}
			else if(err == 0)
			{
				error("connection timed out");
				return -1;
			}
			else
			{
				/*error1("error from select(): %s", strerror(errno));*/
				debug("error from select():");
				msgme1;
				return -1;
			}
		}
		else
		{
			/*error1("connection failed: %s", strerror(errno));*/
			debug("connection failed: ");
			msgme1;
			return err;
		}
	}
	else
	{
		if(connect(sockfd, serv_addr, addrlen) == SOCKET_ERROR)
		{
			/*error1("connecton failed: %s", strerror(errno));*/
			debug("connecton failed");
			msgme1;
			return -1;
		}
		else {
		debug("win32_net_timeout_connect succeed");
		return 0; /* _good_ */
		}
	}
}

SOCKET win32_net_open_connection(mpg123_string *host, mpg123_string *port)
{
	struct addrinfo hints;
	struct addrinfo *addr, *addrlist;
	SOCKET addrcount, sock = -1;

	if(param.verbose>1) fprintf(stderr, "Note: Attempting new-style connection to %s\n", host->p);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family   = AF_UNSPEC; /* We accept both IPv4 and IPv6 ... and perhaps IPv8;-) */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	debug2("Atempt resolve/connect to %s:%s", host->p, port->p);
	msgme(addrcount = getaddrinfo(host->p, port->p, &hints, &addrlist));

	if(addrcount <0)
	{
		error3("Resolving %s:%s: %s", host->p, port->p, gai_strerror(addrcount));
		return -1;
	}

	addr = addrlist;
	while(addr != NULL)
	{
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock == INVALID_SOCKET)
		{
			msgme1;
		}
		else
		{
			if(win32_net_timeout_connect(sock, addr->ai_addr, addr->ai_addrlen) == 0)
			break;
			debug("win32_net_timeout_connect error, closing socket");
			msgme(closesocket(sock));
			sock=-1;
		}
		addr=addr->ai_next;
	}
	if(sock < 0) {error2("Cannot resolve/connect to %s:%s!", host->p, port->p);}

	freeaddrinfo(addrlist);
	debug1("Saving socket %d",sock);
	ws.local_socket = sock;
	return sock;
}

#endif
