
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef __CYGWIN__
#include <windows.h>
#endif

#include "main.h"
#include "conf.h"
#include "log.h"
#include "kad.h"
#include "utils.h"
#include "unix.h"
#include "net.h"
#include "announces.h"
#include "searches.h"
#include "peerfile.h"
#ifdef __CYGWIN__
#include "windows.h"
#endif

#ifdef LPD
#include "ext-lpd.h"
#endif
#ifdef BOB
#include "ext-bob.h"
#endif
#ifdef DNS
#include "ext-dns.h"
#endif
#ifdef NSS
#include "ext-nss.h"
#endif
#ifdef CMD
#include "ext-cmd.h"
#endif
#ifdef FWD
#include "ext-fwd.h"
#endif
#ifdef TLS
#include "ext-tls-client.h"
#include "ext-tls-server.h"
#endif


void main_setup()
{
	// Setup port-forwarding
#ifdef FWD
	fwd_setup();
#endif

	// Setup the Kademlia DHT
	kad_setup();

	// Setup handler to announces
	announces_setup();

	// Setup handler to expire results
	searches_setup();

	// Setup import of peerfile
	peerfile_setup();

	// Setup extensions
#ifdef LPD
	lpd_setup();
#endif
#ifdef BOB
	bob_setup();
#endif
#ifdef DNS
	dns_setup();
#endif
#ifdef NSS
	nss_setup();
#endif
#ifdef TLS
	tls_client_setup();
	tls_server_setup();
#endif
#ifdef CMD
	cmd_setup();
#endif
}

// Cleanup resources on any non crash program exit
void main_free( void ) {
#ifdef CMD
	cmd_free();
#endif
#ifdef NSS
	nss_free();
#endif
#ifdef DNS
	dns_free();
#endif
#ifdef BOB
	bob_free();
#endif
#ifdef LPD
	lpd_free();
#endif
#ifdef TLS
	tls_server_free();
	tls_client_free();
#endif

	peerfile_free();

	searches_free();

	announces_free();

	kad_free();

#ifdef FWD
	fwd_free();
#endif

	conf_free();

	net_free();
}

int main_start( void ) {

	main_setup();

	// Loop over all sockets and file descriptors
	net_loop();

	// Export peers if a file is provided
	peerfile_export();

	main_free();

	return 0;
}

#ifdef __CYGWIN__
int main( int argc, char **argv ) {
	char cmd[MAX_PATH];
	char path[MAX_PATH];
	char *p;

	conf_init();
	conf_setup( argc, argv );

	if( gconf->service_start ) {
		gconf->use_syslog = 1;

		// Get kadnode.exe binary lcoation
		if( GetModuleFileNameA( NULL, path, sizeof(path) ) && (p = strrchr( path, '\\' )) ) {
			*(p + 1) = '\0';
		} else {
			log_err( "Cannot get location of KadNode binary." );
			exit( 1 );
		}

		// Set DNS server to localhost
		sprintf( cmd, "cmd.exe /c \"%s\\dns_setup.bat\"", path );
		windows_exec( cmd );

		int rc = windows_service_start( (void (*)()) main_start );

		// Reset DNS settings to DHCP
		sprintf( cmd, "cmd.exe /c \"%s\\dns_reset.bat\"", path );
		windows_exec( cmd );

		return rc;
	}

	if( gconf->is_daemon ) {
		gconf->use_syslog = 1;

		// Fork before any threads are started
		unix_fork();

		// Change working directory to C:\ directory or disk equivalent
		if( GetModuleFileNameA( NULL, path, sizeof(path) ) && (p = strchr( path, '\\' )) ) {
			*(p + 1) = 0;
			SetCurrentDirectoryA( path );
		}

		// Close pipes
		fclose( stderr );
		fclose( stdout );
		fclose( stdin );
	} else {
		conf_info();
	}

	// Catch signals
	windows_signals();

	// Write pid file
	if( gconf->pidfile ) {
		unix_write_pidfile( GetCurrentProcessId(), gconf->pidfile );
	}

	// Drop privileges
	unix_dropuid0();

	return main_start();
}
#else
int main( int argc, char **argv ) {

	conf_init();
	conf_setup( argc, argv );

	if( gconf->is_daemon ) {
		gconf->use_syslog = 1;

		// Fork before any threads are started
		unix_fork();

		if( chdir( "/" ) != 0 ) {
			log_err( "Changing working directory to '/' failed: %s", strerror( errno ) );
			exit( 1 );
		}

		// Close pipes
		fclose( stderr );
		fclose( stdout );
		fclose( stdin );
	} else {
		conf_info();
	}

	// Catch signals
	unix_signals();

	// Write pid file
	if( gconf->pidfile ) {
		unix_write_pidfile( getpid(), gconf->pidfile );
	}

	// Drop privileges
	unix_dropuid0();

	return main_start();
}
#endif
