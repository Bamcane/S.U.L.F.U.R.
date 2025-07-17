/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times

// server
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SAVE | CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 2, CFGFLAG_SAVE | CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator (2=additional restricted spectator chat)")
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 1, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SAVE | CFGFLAG_SERVER, "Supposed player skill level")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 3, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive clients")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 2, 1, 3, CFGFLAG_SAVE | CFGFLAG_SERVER, "How to deal with inactive clients (1=move player to spectator, 2=move to free spectator slot/kick, 3=kick)")
MACRO_CONFIG_INT(SvInactiveKickSpec, sv_inactivekick_spec, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Kick inactive spectators")

MACRO_CONFIG_INT(SvSilentSpectatorMode, sv_silent_spectator_mode, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Mute join/leave message of spectator")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SAVE | CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SAVE | CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SAVE | CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "")
#endif

#endif
