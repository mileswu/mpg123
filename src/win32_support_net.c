#include "config.h"
#include "mpg123.h"
#include "mpg123app.h"
#include "httpget.h"
#include "debug.h"
#include "resolver.h"
#include <errno.h>

#ifdef NETWORK
static size_t win32_net_readstring (mpg123_string *string, size_t maxlen, FILE *f)
{
	debug2("Attempting readstring on %d for %d bytes", f ? fileno(f) : 0, maxlen);
	int err;
	string->fill = 0;
	while(maxlen == 0 || string->fill < maxlen)
	{
		if(string->size-string->fill < 1)
		if(!mpg123_grow_string(string, string->fill+4096))
		{
			error("Cannot allocate memory for reading.");
			string->fill = 0;
			return 0;
		}
		err = win32_net_read(0,string->p+string->fill,1); /*fd is ignored */
		/* Whoa... reading one byte at a time... one could ensure the line break in another way, but more work. */
		if( err == 1)
		{
			string->fill++;
			if(string->p[string->fill-1] == '\n') break;
		}
		else if(errno != EINTR)
		{
			error("Error reading from socket or unexpected EOF.");
			string->fill = 0;
			/* bail out to prevent endless loop */
			return 0;
		}
	}

	if(!mpg123_grow_string(string, string->fill+1))
	{
		string->fill=0;
	}
	else
	{
		string->p[string->fill] = 0;
		string->fill++;
	}
	return string->fill;
}

static int win32_net_writestring (SOCKET fd, mpg123_string *string)
{
	size_t result, bytes;
	char *ptr = string->p;
	bytes = string->fill ? string->fill-1 : 0;

	while(bytes)
	{
		result = win32_net_write(fd, ptr, bytes);
		if(result < 0 && errno != EINTR)/* errno shouldn't be used here for win32*/
		{
			perror ("writing http string");
			return FALSE;
		}
		else if(result == 0)
		{
			error("write: socket closed unexpectedly");
			return FALSE;
		}
		ptr   += result;
		bytes -= result;
	}
	return TRUE;
}

static SOCKET win32_net_resolve_redirect(mpg123_string *response, mpg123_string *request_url, mpg123_string *purl)
{
	debug1("request_url:%s", request_url->p);
	/* initialized with full old url */
	if(!mpg123_copy_string(request_url, purl)) return FALSE;

	/* We may strip it down to a prefix ot totally. */
	if(strncasecmp(response->p, "Location: http://", 17))
	{ /* OK, only partial strip, need prefix for relative path. */
		char* ptmp = NULL;
		/* though it's not RFC (?), accept relative URIs as wget does */
		fprintf(stderr, "NOTE: no complete URL in redirect, constructing one\n");
		/* not absolute uri, could still be server-absolute */
		/* I prepend a part of the request... out of the request */
		if(response->p[10] == '/')
		{
			/* only prepend http://server/ */
			/* I null the first / after http:// */
			ptmp = strchr(purl->p+7,'/');
			if(ptmp != NULL){ purl->fill = ptmp-purl->p+1; purl->p[purl->fill-1] = 0; }
		}
		else
		{
			/* prepend http://server/path/ */
			/* now we want the last / */
			ptmp = strrchr(purl->p+7, '/');
			if(ptmp != NULL){ purl->fill = ptmp-purl->p+2; purl->p[purl->fill-1] = 0; }
		}
	}
	else purl->fill = 0;

	debug1("prefix=%s", purl->fill ? purl->p : "");
	if(!mpg123_add_string(purl, response->p+10)) return FALSE;

	debug1("           purl: %s", purl->p);
	debug1("old request_url: %s", request_url->p);

	return TRUE;
}

int win32_net_http_open(char* url, struct httpdata *hd)
{
	mpg123_string purl, host, port, path;
	mpg123_string request, response, request_url;
	mpg123_string httpauth1;
	SOCKET sock = SOCKET_ERROR;
	int oom  = 0;
	int relocate, numrelocs = 0;
	int got_location = FALSE;
	FILE *myfile = NULL;
	/*
		workaround for http://www.global24music.com/rautemusik/files/extreme/isdn.pls
		this site's apache gives me a relocation to the same place when I give the port in Host request field
		for the record: Apache/2.0.51 (Fedora)
	*/
	int try_without_port = 0;
	mpg123_init_string(&purl);
	mpg123_init_string(&host);
	mpg123_init_string(&port);
	mpg123_init_string(&path);
	mpg123_init_string(&request);
	mpg123_init_string(&response);
	mpg123_init_string(&request_url);
	mpg123_init_string(&httpauth1);

	/* Get initial info for proxy server. Once. */
	if(hd->proxystate == PROXY_UNKNOWN && !proxy_init(hd)) goto exit;

	if(!translate_url(url, &purl)){ oom=1; goto exit; }

	/* Don't confuse the different auth strings... */
	if(!split_url(&purl, &httpauth1, NULL, NULL, NULL) ){ oom=1; goto exit; }

	/* "GET http://"		11
	 * " HTTP/1.0\r\nUser-Agent: <PACKAGE_NAME>/<PACKAGE_VERSION>\r\n"
	 * 				26 + PACKAGE_NAME + PACKAGE_VERSION
	 * accept header            + accept_length()
	 * "Authorization: Basic \r\n"	23
	 * "\r\n"			 2
	 * ... plus the other predefined header lines
	 */
	/* Just use this estimate as first guess to reduce malloc calls in string library. */
	{
		size_t length_estimate = 62 + strlen(PACKAGE_NAME) + strlen(PACKAGE_VERSION) 
		                       + accept_length() + strlen(CONN_HEAD) + strlen(icy_yes) + purl.fill;
		if(    !mpg123_grow_string(&request, length_estimate)
		    || !mpg123_grow_string(&response,4096) )
		{
			oom=1; goto exit;
		}
	}

	do
	{
		/* Storing the request url, with http:// prepended if needed. */
		/* used to be url here... seemed wrong to me (when loop advanced...) */
		if(strncasecmp(purl.p, "http://", 7) != 0) mpg123_set_string(&request_url, "http://");
		else mpg123_set_string(&request_url, "");

		mpg123_add_string(&request_url, purl.p);

		if (hd->proxystate >= PROXY_HOST)
		{
			/* We will connect to proxy, full URL goes into the request. */
			if(    !mpg123_copy_string(&hd->proxyhost, &host)
			    || !mpg123_copy_string(&hd->proxyport, &port)
			    || !mpg123_set_string(&request, "GET ")
			    || !mpg123_add_string(&request, request_url.p) )
			{
				oom=1; goto exit;
			}
		}
		else
		{
			/* We will connect to the host from the URL and only the path goes into the request. */
			if(!split_url(&purl, NULL, &host, &port, &path)){ oom=1; goto exit; }
			if(    !mpg123_set_string(&request, "GET ")
			    || !mpg123_add_string(&request, path.p) )
			{
				oom=1; goto exit;
			}
		}

		if(!fill_request(&request, &host, &port, &httpauth1, &try_without_port)){ oom=1; goto exit; }

		httpauth1.fill = 0; /* We use the auth data from the URL only once. */
		debug2("attempting to open_connection to %s:%s", host.p, port.p);
		sock = win32_net_open_connection(&host, &port);
		if(sock < 0)
		{
			error1("Unable to establish connection to %s", host.fill ? host.p : "");
			goto exit;
		}
#define http_failure closesocket(sock); sock=SOCKET_ERROR; goto exit;
		
		if(param.verbose > 2) fprintf(stderr, "HTTP request:\n%s\n",request.p);
		if(!win32_net_writestring(sock, &request)){ http_failure; }
		debug("Skipping fdopen for WSA sockets");
		relocate = FALSE;
		/* Arbitrary length limit here... */
#define safe_readstring \
		win32_net_readstring(&response, SIZE_MAX/16, myfile); \
		if(response.fill > SIZE_MAX/16) /* > because of appended zero. */ \
		{ \
			error("HTTP response line exceeds max. length"); \
			http_failure; \
		} \
		else if(response.fill == 0) \
		{ \
			error("readstring failed"); \
			http_failure; \
		} \
		if(param.verbose > 2) fprintf(stderr, "HTTP in: %s", response.p);
		safe_readstring;

		{
			char *sptr;
			if((sptr = strchr(response.p, ' ')))
			{
				if(response.fill > sptr-response.p+2)
				switch (sptr[1])
				{
					case '3':
						relocate = TRUE;
					case '2':
						break;
					default:
						fprintf (stderr, "HTTP request failed: %s", sptr+1); /* '\n' is included */
						http_failure;
				}
				else{ error("Too short response,"); http_failure; }
			}
		}

		/* If we are relocated, we need to look out for a Location header. */
		got_location = FALSE;

		do
		{
			safe_readstring; /* Think about that: Should we really error out when we get nothing? Could be that the server forgot the trailing empty line... */
			if (!strncasecmp(response.p, "Location: ", 10))
			{ /* It is a redirection! */
				if(!win32_net_resolve_redirect(&response, &request_url, &purl)){ oom=1, http_failure; }

				if(!strcmp(purl.p, request_url.p))
				{
					warning("relocated to very same place! trying request again without host port");
					try_without_port = 1;
				}
				got_location = TRUE;
			}
			else
			{ /* We got a header line (or the closing empty line). */
				char *tmp;
				debug1("searching for header values... %s", response.p);
				/* Not sure if I want to bail out on error here. */
				/* Also: What text encoding are these strings in? Doesn't need to be plain ASCII... */
				get_header_string(&response, "content-type", &hd->content_type);
				get_header_string(&response, "icy-name",     &hd->icy_name);
				get_header_string(&response, "icy-url",      &hd->icy_url);

				/* watch out for icy-metaint */
				if((tmp = get_header_val("icy-metaint", &response)))
				{
					hd->icy_interval = (off_t) atol(tmp); /* atoll ? */
					debug1("got icy-metaint %li", (long int)hd->icy_interval);
				}
			}
		} while(response.p[0] != '\r' && response.p[0] != '\n');
	} while(relocate && got_location && purl.fill && numrelocs++ < HTTP_MAX_RELOCATIONS);
	if(relocate)
	{
		if(!got_location)
		error("Server meant to redirect but failed to provide a location!");
		else
		error1("Too many HTTP relocations (%i).", numrelocs);

		http_failure;
	}

exit: /* The end as well as the exception handling point... */
	if(oom) error("Apparently, I ran out of memory or had some bad input data...");

	mpg123_free_string(&purl);
	mpg123_free_string(&host);
	mpg123_free_string(&port);
	mpg123_free_string(&path);
	mpg123_free_string(&request);
	mpg123_free_string(&response);
	mpg123_free_string(&request_url);
	mpg123_free_string(&httpauth1);
	return sock;
}
#else
int win32_net_http_open(char* url, struct httpdata *hd)
{
  return -1;
}
#endif
