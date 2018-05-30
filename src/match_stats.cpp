#include "match_stats.h"
#include "doomstat.h"
#include "d_player.h"
#include "d_netinf.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_main.h"
#include <time.h>

CVAR(String, matchstatdir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

#define WRITE_CVAR(format, type, name) { f.WriteF(#name "=" format "\n", static_cast<type>(name)); }

void WriteConfigFile(FString actualdir, const char* slash, long epoch)
{
    FString infofile;
    infofile.Format ("%s%smatchinfo-%li.tsv", actualdir.GetChars(), slash, epoch);

    FileTextAppender f = FileTextAppender(infofile);
    WRITE_CVAR("%d", bool, deathmatch);
    WRITE_CVAR("%d", bool, teamplay);
    WRITE_CVAR("%d", bool, duel);
    WRITE_CVAR("%d", bool, terminator);
    WRITE_CVAR("%d", bool, lastmanstanding);
    WRITE_CVAR("%d", bool, teamlms);
    WRITE_CVAR("%d", bool, possession);
    WRITE_CVAR("%d", int, teampossession);
    WRITE_CVAR("%ud", int, dmflags);
    WRITE_CVAR("%ud", int, dmflags2);
    WRITE_CVAR("%ud", int, zadmflags);
    WRITE_CVAR("%ud", int, compatflags);
    WRITE_CVAR("%ud", int, compatflags2);
    WRITE_CVAR("%ud", int, zacompatflags);
}
void WriteMatchFile(FString actualdir, const char* slash, long epoch)
{
    FString matchfile;
    matchfile.Format ("%s%smatchstat-%li.tsv", actualdir.GetChars(), slash, epoch);

    FileTextAppender f = FileTextAppender(matchfile);
    if (deathmatch || duel)
    {
        // account and name are not quoted
        f.WriteF("wins\tfrags\tdeaths\thandicap\ttime\tisbot\taccount\tname\n");
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
                if (playeringame[i])
                {
                    // mostly copied from scoreboard.cpp
                    int wins = static_cast<unsigned int> (players[i].ulWins);
                    int frags = D_GetFragCount(&players[i]);
                    int deaths = static_cast<unsigned int> (players[i].ulDeathCount);
                    int handicap = players[i].userinfo.GetHandicap();
                    int time = static_cast<unsigned int> (players[i].ulTime / TICRATE); // seconds, not minutes
                    bool isbot = players[i].bIsBot;
                    const char *account = SERVER_GetClient( i )->GetAccountName();
                    const char *name = players[i].userinfo.GetName();
                    f.WriteF("%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", wins, frags, deaths, handicap, time, isbot, account, name);
                }
        }
    }
}

void MSTAT_Write_Stats() {
    if (NETWORK_GetState( ) != NETSTATE_SERVER && NETWORK_GetState() != NETSTATE_SINGLE_MULTIPLAYER) return;

    FString actualdir = NicePath(matchstatdir);
    FixPathSeperator(actualdir);
    const char *slash = (actualdir.IsNotEmpty() && actualdir[actualdir.Len()-1] != '/') ? "/" : "";

    long epoch = static_cast<long>(time(NULL));

    WriteConfigFile(actualdir, slash, epoch);
    WriteMatchFile(actualdir, slash, epoch);
}
