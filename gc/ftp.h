/****************************************************************************
 * TinyFTP
 * Nintendo Wii/GameCube FTP implementation
 *
 * FTP devoptab
 * Implemented by hax
 ****************************************************************************/

#ifndef _LIBFTF_H
#define _LIBFTF_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FTP_MOUNTED 5

bool ftpInitDevice(const char* name, const char *user, const char *password, 
					const char *share, const char *hostname, bool ftp_passive);
void ftpClose(const char* name);
bool CheckFTPConnection(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* _LIBFTF_H */
