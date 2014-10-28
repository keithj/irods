/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/

/*
  This routine sets up the rodsEnv structure using the contents of the
  .irodsEnv file and possibly some environment variables.  For each of
  the .irodsEnv items, if an environment variable with the same name
  exists, it overrides the possible .irodsEnv item.  This is called by
  the various Rods commands and the agent.

  This routine also fills in irodsHome and irodsCwd if they are not
  otherwise defined, and if values needed to create them are available.

  The '#' character indicates a comment line.  Items may be enclosed in
  quotes, but do not need to be.  One or more spaces, or a '=', will
  preceed the item values.

  The items are defined in the rodsEnv struct.  The item names in the
  .irodsEnv file need to match the item names of the struct.

  If an error occurs, a message may logged or displayed but the
  structure is filled with whatever values are available.

  There is also an 'appendRodsEnv' function to add text to
  the env file, either creating it or appending to it.
*/

#include "rods.hpp"
#include "rodsErrorTable.hpp"
#include "getRodsEnv.hpp"
#include "rodsLog.hpp"
#include "irods_log.hpp"
#include "irods_environment_properties.hpp"
#include "irods_configuration_keywords.hpp"

#include "irods_stacktrace.hpp"

#define BUF_LEN 100
#define LARGE_BUF_LEN MAX_NAME_LEN+20

#define RODS_ENV_FILE "/.irods/irods_environment.json"  /* under the HOME directory */
extern "C" {

    extern int ProcessType;
    extern char *rstrcpy( char *dst, const char *src, int len ); // why do they not just include the header? - harry

    char *findNextTokenAndTerm( char *inPtr );

    int getRodsEnvFromFile( rodsEnv *rodsEnvArg );
    int getRodsEnvFromEnv( rodsEnv *rodsEnvArg );
    int createRodsEnvDefaults( rodsEnv *rodsEnvArg );

    static char authFileName  [ LONG_NAME_LEN ] = "";
    static char configFileName[ LONG_NAME_LEN ] = "";

    char *
    getRodsEnvFileName() {
        return configFileName;
    }

    /* Return the auth filename, if any */
    /* Used by obf routines so that the env struct doesn't have to be passed
       up and down the calling chain */
    char *
    getRodsEnvAuthFileName() {
        return authFileName;
    }

    /* convert either an integer value or a name matching the defines, to
       a value for the Logging Level */
    int
    convertLogLevel( char *inputStr ) {
        int i;
        i = atoi( inputStr );
        if ( i > 0 && i <= LOG_SQL ) {
            return i;
        }
        if ( strcmp( inputStr, "LOG_SQL" ) == 0 ) {
            return LOG_SQL;
        }
        if ( strcmp( inputStr, "LOG_SYS_FATAL" ) == 0 ) {
            return LOG_SYS_FATAL;
        }
        if ( strcmp( inputStr, "LOG_SYS_WARNING" ) == 0 ) {
            return LOG_SYS_WARNING;
        }
        if ( strcmp( inputStr, "LOG_ERROR" ) == 0 ) {
            return LOG_ERROR;
        }
        if ( strcmp( inputStr, "LOG_NOTICE" ) == 0 ) {
            return LOG_NOTICE;
        }
        if ( strcmp( inputStr, "LOG_DEBUG" ) == 0 ) {
            return LOG_DEBUG;
        }
        if ( strcmp( inputStr, "LOG_DEBUG3" ) == 0 ) {
            return LOG_DEBUG3;
        }
        if ( strcmp( inputStr, "LOG_DEBUG2" ) == 0 ) {
            return LOG_DEBUG2;
        }
        if ( strcmp( inputStr, "LOG_DEBUG1" ) == 0 ) {
            return LOG_DEBUG1;
        }
        return 0;
    }

    int getRodsEnv( rodsEnv *rodsEnvArg ) {
        if( !rodsEnvArg ) {
            printf( "ERROR - getRodsEnv :: null rodsEnv\n" );
            fflush( stdout );
            return SYS_INVALID_INPUT_PARAM;
        }

        memset( rodsEnvArg, 0, sizeof( rodsEnv ) );
        getRodsEnvFromFile( rodsEnvArg );
        getRodsEnvFromEnv( rodsEnvArg );
        createRodsEnvDefaults( rodsEnvArg );

        return 0;
    }

    static
    int capture_string_property(
        const int                      _msg_lvl,
        irods::environment_properties& _props,
        const std::string&             _key,
        char*                          _val ) {
        std::string prop_str;
        irods::error ret = _props.get_property< 
                               std::string >( 
                                   _key,
                                   prop_str );
        if( !ret.ok() ) {
            rodsLog(
                _msg_lvl,
                "%s is not defined",
                _key.c_str() );
            return -1;
        } else {
            rodsLog( 
                _msg_lvl, 
                "%s - %s",
                _key.c_str(),
                prop_str.c_str() );
            strncpy( 
                _val,
                prop_str.c_str(), 
                prop_str.size()+1 );
            return 0; 
        }

    } // capture_string_property

    static
    int capture_integer_property(
        const int                      _msg_lvl,
        irods::environment_properties& _props,
        const std::string&             _key,
        int&                           _val ) {
        irods::error ret = _props.get_property< int >( 
                               _key,
                               _val );
        if( !ret.ok() ) {
            rodsLog(
                _msg_lvl,
                "%s is not defined",
                _key.c_str() );
            return ret.code();
        }
        rodsLog( 
            _msg_lvl, 
            "%s - %d",
            _key.c_str(),
            _val );
        
        return 0; 

    } // capture_integer_property

    int getRodsEnvFromFile( 
        rodsEnv* _env ) {
        if( !_env ) {
            printf( "ERROR - getRodsEnv :: null rodsEnv\n" );
            fflush( stdout );
            return SYS_INVALID_INPUT_PARAM;
        }

        irods::environment_properties& props = 
            irods::environment_properties::getInstance();
        irods::error ret = props.capture_if_needed();
        if( !ret.ok() ) {
            // irods::log( PASS( ret ) );
            // legacy code doesnt error out 
            // return  ret.code();
        }

        int msg_lvl = LOG_DEBUG;
        if( getenv( PRINT_RODS_ENV_STR ) && 
            atoi( getenv(PRINT_RODS_ENV_STR ) ) ) {
            msg_lvl = LOG_NOTICE;
            unsetenv( PRINT_RODS_ENV_STR );
        }

         // default auth scheme
         strncpy(
             _env->rodsAuthScheme,
             "native",
             6 );

        capture_string_property( 
            msg_lvl,
            props, 
            irods::CFG_IRODS_SESSION_ENVIRONMENT_FILE_KW, 
             configFileName );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_USER_NAME_KW, 
             _env->rodsUserName );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_HOST_KW, 
             _env->rodsHost );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_XMSG_HOST_KW, 
             _env->xmsgHost );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_HOME_KW, 
             _env->rodsHome );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_CWD_KW, 
             _env->rodsCwd );
         
         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_AUTHENTICATION_SCHEME_KW, 
             _env->rodsAuthScheme );

         capture_integer_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_PORT_KW, 
             _env->rodsPort );

         capture_integer_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_XMSG_PORT_KW, 
             _env->xmsgPort );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_DEFAULT_RESOURCE_KW, 
             _env->rodsDefResource );
 
         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_ZONE_KW, 
             _env->rodsZone );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_CLIENT_SERVER_POLICY_KW, 
             _env->rodsClientServerPolicy );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_CLIENT_SERVER_NEGOTIATION_KW, 
             _env->rodsClientServerNegotiation );

         capture_integer_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_ENCRYPTION_KEY_SIZE_KW, 
             _env->rodsEncryptionKeySize );

         capture_integer_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_ENCRYPTION_SALT_SIZE_KW, 
             _env->rodsEncryptionSaltSize );

         capture_integer_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_ENCRYPTION_NUM_HASH_ROUNDS_KW, 
             _env->rodsEncryptionNumHashRounds );
 
         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_ENCRYPTION_ALGORITHM_KW, 
             _env->rodsEncryptionAlgorithm );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_DEFAULT_HASH_SCHEME_KW, 
             _env->rodsDefaultHashScheme );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_MATCH_HASH_POLICY_KW, 
             _env->rodsMatchHashPolicy );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_GSI_SERVER_DN_KW, 
             _env->rodsServerDn );

         capture_string_property( 
            msg_lvl,
             props, 
             irods::CFG_IRODS_DEBUG_KW, 
             _env->rodsDebug );

        _env->rodsLogLevel = 0;
        int status = capture_integer_property( 
                         msg_lvl,
                         props, 
                         irods::CFG_IRODS_LOG_LEVEL_KW, 
                         _env->rodsLogLevel );
        if( status == 0 ) {
            rodsLogLevel( _env->rodsLogLevel ); 

        }

        memset( _env->rodsAuthFileName, 0, sizeof( _env->rodsAuthFileName ) );
        status = capture_string_property( 
                     msg_lvl,
                     props, 
                     irods::CFG_IRODS_AUTHENTICATION_FILE_NAME_KW, 
                     _env->rodsAuthFileName );
        if( status == 0 ) {
            rstrcpy( 
                authFileName, 
                _env->rodsAuthFileName, 
                LONG_NAME_LEN - 1);
        }
        
        return 0;
    }


    static
    void capture_string_env_var(
        const std::string& _key,
        char*              _val ) {
        char* env = getenv( 
                        irods::to_env( _key ).c_str() );
        if( env ) {
            strncpy(
                _val,
                env,
                strlen(env)+1 );
            rodsLog( 
                LOG_DEBUG,
                "captured env [%s]-[%s]",
                _key.c_str(),
                _val );
        }
          
    } // capture_string_env_var

    static
    void capture_integer_env_var(
        const std::string& _key,
        int&               _val ) {
        char* env = getenv( 
                        irods::to_env( _key ).c_str() );
        if( env ) {
            _val = atoi( env );
            rodsLog( 
                LOG_DEBUG,
                "captured env [%s]-[%d]",
                _key.c_str(),
                _val );
        }
                
    } // capture_integer_env_var

    int
    getRodsEnvFromEnv( 
        rodsEnv* _env ) {
        if( !_env ) {
            printf( "ERROR - getRodsEnvFromEnv :: null rodsEnv\n" );
            fflush( stdout );
            return SYS_INVALID_INPUT_PARAM;
        }

        std::string env_var = irods::CFG_IRODS_USER_NAME_KW;
        capture_string_env_var(
            env_var,
            _env->rodsUserName );

        env_var = irods::CFG_IRODS_HOST_KW;
        capture_string_env_var(
            env_var,
            _env->rodsHost );

        env_var = irods::CFG_IRODS_XMSG_HOST_KW;
        capture_string_env_var(
            env_var,
            _env->xmsgHost );

        env_var = irods::CFG_IRODS_PORT_KW;
        capture_integer_env_var(
            env_var,
            _env->rodsPort );

        env_var = irods::CFG_IRODS_XMSG_PORT_KW;
        capture_integer_env_var(
            env_var,
            _env->xmsgPort );

        env_var = irods::CFG_IRODS_HOME_KW;
        capture_string_env_var(
            env_var,
            _env->rodsHome );

        env_var = irods::CFG_IRODS_CWD_KW;
        capture_string_env_var(
            env_var,
            _env->rodsCwd );

        env_var = irods::CFG_IRODS_AUTHENTICATION_SCHEME_KW;
        capture_string_env_var(
            env_var,
            _env->rodsAuthScheme );

        env_var = irods::CFG_IRODS_DEFAULT_RESOURCE_KW;
        capture_string_env_var(
            env_var,
            _env->rodsDefResource );

        env_var = irods::CFG_IRODS_ZONE_KW;
        capture_string_env_var(
            env_var,
            _env->rodsZone );

        env_var = irods::CFG_IRODS_CLIENT_SERVER_POLICY_KW;
        capture_string_env_var(
            env_var,
            _env->rodsClientServerPolicy );

        env_var = irods::CFG_IRODS_CLIENT_SERVER_NEGOTIATION_KW;
        capture_string_env_var(
            env_var,
            _env->rodsClientServerNegotiation );

        env_var = irods::CFG_IRODS_ENCRYPTION_KEY_SIZE_KW;
        capture_integer_env_var(
            env_var,
            _env->rodsEncryptionKeySize );
        
        env_var = irods::CFG_IRODS_ENCRYPTION_SALT_SIZE_KW;
        capture_integer_env_var(
            env_var,
            _env->rodsEncryptionSaltSize );

        env_var = irods::CFG_IRODS_ENCRYPTION_NUM_HASH_ROUNDS_KW;
        capture_integer_env_var(
            env_var,
            _env->rodsEncryptionNumHashRounds );

        env_var = irods::CFG_IRODS_ENCRYPTION_ALGORITHM_KW;
        capture_string_env_var(
            env_var,
            _env->rodsEncryptionAlgorithm );

        env_var = irods::CFG_IRODS_DEFAULT_HASH_SCHEME_KW;
        capture_string_env_var(
            env_var,
            _env->rodsDefaultHashScheme );

        env_var = irods::CFG_IRODS_MATCH_HASH_POLICY_KW;
        capture_string_env_var(
            env_var,
            _env->rodsMatchHashPolicy );

        env_var = irods::CFG_IRODS_GSI_SERVER_DN_KW;
        capture_string_env_var(
            env_var,
            _env->rodsServerDn );

        _env->rodsLogLevel = 0;
        env_var = irods::CFG_IRODS_LOG_LEVEL_KW;
        capture_integer_env_var(
            env_var,
            _env->rodsLogLevel );
        if ( _env->rodsLogLevel ) {
            rodsLogLevel( _env->rodsLogLevel ); /* go ahead and process it */
        }

        memset( _env->rodsAuthFileName, 0, sizeof( _env->rodsAuthFileName ) );
        env_var = irods::CFG_IRODS_AUTHENTICATION_FILE_NAME_KW;
        capture_string_env_var(
            env_var,
            _env->rodsAuthFileName );
        if( strlen( _env->rodsAuthFileName ) > 0 ) {
            rstrcpy( authFileName, _env->rodsAuthFileName, LONG_NAME_LEN );

        }

        env_var = irods::CFG_IRODS_DEBUG_KW;
        capture_string_env_var(
            env_var,
            _env->rodsDebug );

        return 0;
    }

    /* build a couple default values from others if appropriate */
    int
    createRodsEnvDefaults( rodsEnv *rodsEnvArg ) {
        if ( strlen( rodsEnvArg->rodsHome ) == 0 ) {
            if ( strlen( rodsEnvArg->rodsUserName ) > 0 &&
                    strlen( rodsEnvArg->rodsZone ) > 0 ) {
                snprintf( rodsEnvArg->rodsHome,  MAX_NAME_LEN, "/%s/home/%s",
                          rodsEnvArg->rodsZone, rodsEnvArg->rodsUserName );
            }
            rodsLog( LOG_NOTICE, "created irodsHome=%s", rodsEnvArg->rodsHome );
        }
        if ( strlen( rodsEnvArg->rodsCwd ) == 0 &&
                strlen( rodsEnvArg->rodsHome ) > 0 ) {
            rstrcpy( rodsEnvArg->rodsCwd, rodsEnvArg->rodsHome, MAX_NAME_LEN );
            rodsLog( LOG_NOTICE, "created irodsCwd=%s", rodsEnvArg->rodsCwd );
        }

        return 0;
    }


    /*
      find the next delimited token and terminate the string with matching quotes
    */
    char *findNextTokenAndTerm( char *inPtr ) {
        char *myPtr = 0;
        char *savePtr = 0;
        char *nextPtr = 0;
        int whiteSpace = 0;
        myPtr = inPtr;
        whiteSpace = 1;
        for ( ;; myPtr++ ) {
            if ( *myPtr == ' ' || *myPtr == '=' ) {
                continue;
            }
            if ( *myPtr == '"' && whiteSpace ) {
                myPtr++;
                savePtr = myPtr;
                for ( ;; ) {
                    if ( *myPtr == '"' ) {
                        nextPtr = myPtr + 1;
                        if ( *nextPtr == ' ' || *nextPtr == '\n'  || *nextPtr == '\0' ) {
                            /* imbedded "s are OK */
                            *myPtr = '\0';
                            return savePtr;
                        }
                    }
                    if ( *myPtr == '\n' ) {
                        *myPtr = '\0';
                    }
                    if ( *myPtr == '\0' ) {
                        /* terminated without a corresponding ", so backup and
                           put the starting one back */
                        savePtr--;
                        *savePtr = '"';
                        return savePtr;
                    }
                    myPtr++;
                }
            }
            if ( *myPtr == '\'' && whiteSpace ) {
                myPtr++;
                savePtr = myPtr;
                for ( ;; ) {
                    if ( *myPtr == '\'' ) {
                        nextPtr = myPtr + 1;
                        if ( *nextPtr == ' ' || *nextPtr == '\n'  || *nextPtr == '\0' ) {
                            /* imbedded 's are OK */
                            *myPtr = '\0';
                            return savePtr;
                        }
                    }
                    if ( *myPtr == '\n' ) {
                        *myPtr = '\0';
                    }
                    if ( *myPtr == '\0' ) {
                        /* terminated without a corresponding ", so backup and
                           put the starting one back */
                        savePtr--;
                        *savePtr = '\'';
                        return savePtr;
                    }
                    myPtr++;
                }
            }
            if ( whiteSpace ) {
                savePtr = myPtr;
            }
            whiteSpace = 0;
            if ( *myPtr == '\n' ) {
                *myPtr = '\0';
            }
            if ( *myPtr == '\r' ) {
                *myPtr = '\0';
            }
            if ( *myPtr == '\0' ) {
                return savePtr;
            }
        }
    }

    int appendRodsEnv( char *appendText ) {
        FILE *fptr;
        char *getVar = NULL;
printf( "XXXX - appendRodsEnv :: FIXME!!!!!!!!!!!!!\n" );
        getVar = getenv( irods::to_env( irods::CFG_IRODS_ENVIRONMENT_FILE_KW ).c_str() );
        if ( getVar != NULL && *getVar != '\0' ) {
            rstrcpy( configFileName, findNextTokenAndTerm( getVar ), LONG_NAME_LEN );
        }
        fptr = fopen( configFileName, "a" );
        if ( fptr == NULL ) {
            rodsLog( LOG_ERROR,
                     "appendRodsEnv: cannot create file %s",
                     configFileName );
            return 0;
        }
        fputs( appendText, fptr );
        fclose( fptr );
        return 0;
    }

} // extern "C"


