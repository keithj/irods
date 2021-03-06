/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to subStructFiles in the COPYRIGHT directory ***/
/* subStructFileReaddir.h
 */

#ifndef SUB_STRUCT_FILE_READDIR_H__
#define SUB_STRUCT_FILE_READDIR_H__

/* This is Object File I/O type API call */

#include "rodsType.h"
#include "rcConnect.h"
#include "subStructFileRead.h"

#if defined(RODS_SERVER)
#define RS_SUB_STRUCT_FILE_READDIR rsSubStructFileReaddir
/* prototype for the server handler */
#include "rodsConnect.h"
int
rsSubStructFileReaddir( rsComm_t *rsComm, subStructFileFdOprInp_t *subStructFileReaddirInp,
                        rodsDirent_t **rodsDirent );
int
_rsSubStructFileReaddir( rsComm_t *rsComm, subStructFileFdOprInp_t *subStructFileReaddirInp,
                         rodsDirent_t **rodsDirent );
int
remoteSubStructFileReaddir( rsComm_t *rsComm, subStructFileFdOprInp_t *subStructFileReaddirInp,
                            rodsDirent_t **rodsDirent, rodsServerHost_t *rodsServerHost );
#else
#define RS_SUB_STRUCT_FILE_READDIR NULL
#endif

/* prototype for the client call */
int
rcSubStructFileReaddir( rcComm_t *conn, subStructFileFdOprInp_t *subStructFileReaddirInp,
                        rodsDirent_t **rodsDirent );

#endif	// SUB_STRUCT_FILE_READDIR_H__
