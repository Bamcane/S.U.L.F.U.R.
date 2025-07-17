/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_VARIABLES_H
#define ENGINE_SHARED_CONFIG_VARIABLES_H
#undef ENGINE_SHARED_CONFIG_VARIABLES_H // this file will be included several times

// TODO: remove this
#include "././game/variables.h"

MACRO_CONFIG_STR(Password, password, 32, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Password to the server")
MACRO_CONFIG_STR(Logfile, logfile, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Filename to log all output to")
MACRO_CONFIG_INT(LogfileTimestamp, logfile_timestamp, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Add a time stamp to the log file's name")
MACRO_CONFIG_INT(ConsoleOutputLevel, console_output_level, 0, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the console")

MACRO_CONFIG_STR(SvName, sv_name, 128, "unnamed server", CFGFLAG_SAVE | CFGFLAG_SERVER, "Server name")
MACRO_CONFIG_STR(SvHostname, sv_hostname, 128, "", CFGFLAG_SAVE | CFGFLAG_SERVER, "Server hostname")
MACRO_CONFIG_STR(Bindaddr, bindaddr, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_MASTER, "Address to bind the client/server to")
MACRO_CONFIG_INT(SvPort, sv_port, 8303, 0, 0, CFGFLAG_SAVE | CFGFLAG_SERVER, "Port to use for the server")
MACRO_CONFIG_INT(SvExternalPort, sv_external_port, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_SERVER, "External port to report to the master servers")
MACRO_CONFIG_STR(SvMap, sv_map, 128, "dm1", CFGFLAG_SAVE | CFGFLAG_SERVER, "Map to use on the server")
MACRO_CONFIG_INT(SvMaxClients, sv_max_clients, MAX_CLIENTS, 1, MAX_CLIENTS, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of clients that are allowed on a server")
MACRO_CONFIG_INT(SvMaxClientsPerIP, sv_max_clients_per_ip, 4, 1, MAX_CLIENTS, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of clients with the same IP that can connect to the server")
MACRO_CONFIG_INT(SvMapDownloadSpeed, sv_map_download_speed, 8, 1, 16, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of map data packages a client gets on each request")
MACRO_CONFIG_INT(SvHighBandwidth, sv_high_bandwidth, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Use high bandwidth mode. Doubles the bandwidth required for the server. LAN use only")
MACRO_CONFIG_STR(SvRegister, sv_register, 16, "1", CFGFLAG_SERVER, "Register server with master server for public listing, can also accept a comma-separated list of protocols to register on, like 'ipv4,ipv6'")
MACRO_CONFIG_STR(SvRegisterExtra, sv_register_extra, 256, "", CFGFLAG_SERVER, "Extra headers to send to the register endpoint, comma-separated 'Header: Value' pairs")
MACRO_CONFIG_STR(SvRegisterUrl, sv_register_url, 128, "https://master1.ddnet.org/ddnet/15/register", CFGFLAG_SERVER, "Masterserver URL to register to")
MACRO_CONFIG_STR(SvRconPassword, sv_rcon_password, 32, "", CFGFLAG_SAVE | CFGFLAG_SERVER, "Remote console password (full access)")
MACRO_CONFIG_STR(SvRconModPassword, sv_rcon_mod_password, 32, "", CFGFLAG_SAVE | CFGFLAG_SERVER, "Remote console password for moderators (limited access)")
MACRO_CONFIG_INT(SvRconMaxTries, sv_rcon_max_tries, 3, 0, 100, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of tries for remote console authentication")
MACRO_CONFIG_INT(SvRconBantime, sv_rcon_bantime, 5, 0, 1440, CFGFLAG_SAVE | CFGFLAG_SERVER, "The time a client gets banned if remote console authentication fails. 0 makes it just use kick")
MACRO_CONFIG_INT(SvAutoDemoRecord, sv_auto_demo_record, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Automatically record demos")
MACRO_CONFIG_INT(SvAutoDemoMax, sv_auto_demo_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_STR(SvMaplist, sv_maplist, 32, "all", CFGFLAG_SAVE | CFGFLAG_SERVER, "Maplist for authed clients (none, standard, all)")

MACRO_CONFIG_STR(EcBindaddr, ec_bindaddr, 128, "localhost", CFGFLAG_SAVE | CFGFLAG_ECON, "Address to bind the external console to. Anything but 'localhost' is dangerous")
MACRO_CONFIG_INT(EcPort, ec_port, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_ECON, "Port to use for the external console")
MACRO_CONFIG_STR(EcPassword, ec_password, 32, "", CFGFLAG_SAVE | CFGFLAG_ECON, "External console password")
MACRO_CONFIG_INT(EcBantime, ec_bantime, 0, 0, 1440, CFGFLAG_SAVE | CFGFLAG_ECON, "The time a client gets banned if econ authentication fails. 0 just closes the connection")
MACRO_CONFIG_INT(EcAuthTimeout, ec_auth_timeout, 30, 1, 120, CFGFLAG_SAVE | CFGFLAG_ECON, "Time in seconds before the the econ authentification times out")
MACRO_CONFIG_INT(EcOutputLevel, ec_output_level, 1, 0, 2, CFGFLAG_SAVE | CFGFLAG_ECON, "Adjusts the amount of information in the external console")

MACRO_CONFIG_INT(NetTcpAbortOnClose, net_tcp_abort_on_close, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER | CFGFLAG_ECON, "Aborts tcp connection on close")

MACRO_CONFIG_INT(Debug, debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug mode")
MACRO_CONFIG_INT(DbgPref, dbg_pref, 0, 0, 1, CFGFLAG_SERVER, "Performance outputs")
MACRO_CONFIG_INT(DbgHitch, dbg_hitch, 0, 0, 0, CFGFLAG_SERVER, "Hitch warnings")
MACRO_CONFIG_INT(DbgCurl, dbg_curl, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug curl")

MACRO_CONFIG_INT(HttpAllowInsecure, http_allow_insecure, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Allow insecure HTTP protocol in addition to the secure HTTPS one. Mostly useful for testing.")

#endif
