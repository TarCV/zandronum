#include "match_stats.h"
#include "doomstat.h"
#include "d_player.h"
#include "d_netinf.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_main.h"
#include <time.h>

void MSTAT_Write_Stats() {
    if (NETWORK_GetState( ) != NETSTATE_SERVER && NETWORK_GetState() != NETSTATE_SINGLE_MULTIPLAYER) return;

    long epoch = static_cast<long>(time(NULL));
    const int fn_len = 100;
    char fn[fn_len];
    if (snprintf(fn, fn_len, "matchstat-%li.tsv", epoch) <= 0) return;

    FileTextAppender f = FileTextAppender(fn);

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
