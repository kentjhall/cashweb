#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <select.h>
#include <socket.h>
#include <microhttpd.h>

#define PORT 80

static int answer_cash_request(void *cls, struct MHD_Connection *connection,
		  const char *url,
		  const char *method,
		  const char *version,
		  const char *upload_data, size_t *upload_data_size,
		  void **con_cls) {
	
}

int main() {
	struct MHD_Daemon *daemon;
	if ((daemon = MHD_start_daemon (MHD_USE_INTERNAL_POLLING_THREAD, PORT, 
					NULL, NULL, 
					&anwer_cash_request, 
					NULL, MHD_OPTION_END)) == NULL) { return 1; }
	
	getchar(); // wait for enter key to be pressed, daemon running

	MHD_stop_daemon(daemon);
	return 0;
}
