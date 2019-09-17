#include <cashgettools.h>
#include <unistd.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/*
 * gets by CashWeb ID via specified BitDB HTTP endpoint
 * writes MIME type to given memory location and file to descriptor
 */
EMSCRIPTEN_KEEPALIVE
CW_STATUS CWG_WA_get_by_id(const char *id, const char *bitdbNode, char *mimeStr, int fd) {
	struct CWG_params params;
	char mimeBuf[CWG_MIMESTR_BUF]; mimeBuf[0] = 0;
	init_CWG_params(&params, NULL, bitdbNode, &mimeBuf);	
	params.datadir = "/data";
	CWG_err_stream = stderr;

	CW_STATUS status = CWG_get_by_id(id, &params, fd);
	strcpy(mimeStr, mimeBuf);
	
	return status;
}

/*
 * writes error message to stderr/stdout (errStream true/false) based on given CashWeb status code (should be > 0)
 */
EMSCRIPTEN_KEEPALIVE
const char *CWG_WA_errno_print_msg(CW_STATUS errno) {
	return CWG_errno_to_msg(errno);
}
