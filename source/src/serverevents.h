// processing of server events

// server ballistics
#include "serverballistics.h"

// ordered
void destroyevent::process(client *ci)
{
    client &c = *ci;
    clientstate &cs = c.state;
    // int damagepotential = effectiveDamage(weap, 0), damagedealt = 0;
    switch (weap)
    {
        case GUN_GRENADE:
        {
            if (!cs.grenades.remove(flags)) return;
            // damagedealt += explosion(c, o, WEAP_GRENADE, !m_real(gamemode, mutators));
            break;
        }

        case GUN_KNIFE:
        {
            // ...
            break;
        }

        default:
            return;
    }
    // gs.damage += damagedealt;
    // gs.shotdamage += max(damagedealt, damagepotential);
}

void shotevent::process(client *ci)
{
    client &c = *ci;
    clientstate &cs = c.state;
    int wait = millis - cs.lastshot;
    if (!cs.isalive(gamemillis) || // dead
        weap<0 || weap >= NUMGUNS || // invalid weapon
        weap != cs.gunselect || // not selected
        (weap == GUN_AKIMBO && cs.akimbomillis < gamemillis) || // akimbo after out
        wait<cs.gunwait[weap] || // not allowed
        cs.mag[weap] <= 0) // out of ammo in mag
        return;
    if (!melee_weap(weap)) // ammo cost
        --cs.mag[weap];
    cs.updateshot(millis);
    cs.gunwait[weap] = attackdelay(weap);
    // shot stuff...
    // send packet...
}

void reloadevent::process(client *ci)
{
    clientstate &cs = ci->state;
    if (!cs.isalive(gamemillis) || // dead
        weap<0 || weap >= NUMGUNS || // invalid weapon
        (weap == GUN_AKIMBO && cs.akimbomillis < gamemillis) || // akimbo after out
        cs.ammo[weap] < 1 /*reload*/) // no ammo
        return;
    const int mag = magsize(weap), reload = reloadsize(weap);

    if (!reload || // cannot reload
        cs.mag[weap] >= mag) // already full
        return;

    // perform the reload
    cs.mag[weap] = min(mag, cs.mag[weap] + reload);
    cs.ammo[weap] -= /*reload*/ 1;

    int wait = millis - cs.lastshot;
    sendf(-1, 1, "ri5", SV_RELOAD, ci->clientnum, weap, cs.mag[weap], cs.ammo[weap]);
    if (!cs.gunwait[weap] || wait >= cs.gunwait[weap])
        cs.updateshot(millis);
    cs.gunwait[weap] += reloadtime(weap);
}

void akimboevent::process(client *ci)
{
    clientstate &cs = ci->state;
    if (!cs.isalive(gamemillis) || !cs.akimbo || cs.akimbomillis) return;
    // WHY DOES THIS EVENT TYPE EVEN EXIST?!
    cs.akimbomillis = gamemillis + 30000;
}

// unordered
/*
void healevent::process(client *ci)
{
    // ...
}

void suicidebomberevent::process(client *ci)
{
    explosion(*ci, ci->state.o, WEAP_GRENADE, !m_real(gamemode, mutators), true, valid_client(id) ? clients[id] : NULL);
}

void airstrikeevent::process(client *ci)
{
    explosion(*ci, o, WEAP_GRENADE, !m_real(gamemode, mutators), false);
}
*/

// processing events
bool timedevent::flush(client *ci, int fmillis)
{
    if (!valid) return true;
    else if (millis > fmillis) return false;
    else if (millis >= ci->lastevent)
    {
        ci->lastevent = millis;
        process(ci);
    }
    return true;
}

void processevents()
{
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type==ST_EMPTY || !c.isauthed || team_isspect(c.team)) continue;
        clientstate &cs = c.state;
        /*
        // game ending nuke...
		if(cs.nukemillis && cs.nukemillis <= gamemillis && minremain)
        {
			// boom... gg
			//forceintermission = true;
			cs.nukemillis = 0;
			sendf(-1, 1, "ri4", N_STREAKUSE, i, STREAK_NUKE, 0);
			// apply the nuke effect
			nuke(c);
		}
        */
        // drown, bleed, regen
        if(cs.state == CS_ALIVE)
        {
            /*
            // drown underwater
			if(cs.o.z < smapstats.hdr.waterlevel)
            {
				if(cs.drownmillis <= 0)
                {
					if(cs.drownmillis) // resume partial drowning
						cs.drownval = max(cs.drownval - ((servmillis + cs.drownmillis) / 1000), 0);
					cs.drownmillis = gamemillis;
				}
				char drownstate = max(0, (gamemillis - cs.drownmillis) / 1000 - 10);
				while(cs.drownval < drownstate)
                {
					++cs.drownval;
					serverdamage(&c, &c, (m_classic(gamemode, mutators) ? 5 : (cs.drownval + 10)) * HEALTHSCALE, WEAP_MAX + 13, FRAG_NONE, cs.o);
					if(cs.state != CS_ALIVE) break; // dead!
				}
			}
			else if(cs.drownmillis > 0)
                cs.drownmillis = -cs.drownmillis; // save partial drowning
			// bleeding--oh no!
            if(cs.wounds.length())
            {
				loopv(cs.wounds)
                {
					wound &w = cs.wounds[i];
					if(!valid_client(w.inflictor)) cs.wounds.remove(i--);
					else if(w.lastdealt + 500 < gamemillis)
                    {
						client &owner = *clients[w.inflictor];
						const int bleeddmg = (m_zombie(gamemode) ? BLEEDDMGZ : owner.state.perk2 == PERK_POWER ? BLEEDDMGPLUS : BLEEDDMG) * HEALTHSCALE;
						owner.state.damage += bleeddmg;
						owner.state.shotdamage += bleeddmg;
						// where were we wounded?
						vec woundloc = cs.o;
						woundloc.add(w.offset);
						// blood fx and stuff
						sendhit(owner, WEAP_KNIFE, woundloc, bleeddmg);
						// use wounded location as damage source
						serverdamage(&c, &owner, bleeddmg, WEAP_KNIFE, FRAG_NONE, woundloc, c.state.o.dist(owner.state.o));
						w.lastdealt = gamemillis;
					}
				}
			}
            // health regeneration
			else if(m_regen(gamemode, mutators) && cs.state == CS_ALIVE && cs.health < STARTHEALTH && cs.lastregen + (cs.perk1 == PERK_POWER ? REGENINT * .7f : REGENINT) < gamemillis){
				int amt = round_(float((STARTHEALTH - cs.health) / 5 + 15));
				if(cs.perk1 == PERK_POWER) amt *= 1.4f;
				if(amt >= STARTHEALTH - cs.health)
					amt = STARTHEALTH - cs.health;
				sendf(-1, 1, "ri3", N_REGEN, i, cs.health += amt);
				cs.lastregen = gamemillis;
			}
            */
        }
        // not alive: spawn queue
        else if(/*cs.state == CS_WAITING ||*/ (c.type == ST_AI && valid_client(c.ownernum) && clients[c.ownernum]->isonrightmap && cs.state == CS_DEAD && cs.lastspawn<0))
        {
			const int waitremain = SPAWNDELAY - gamemillis + cs.lastdeath;
            extern int canspawn(client *c);
			if(canspawn(&c) == SP_OK && waitremain <= 0)
                sendspawn(&c);
		}
        // akimbo out!
        if(cs.akimbomillis && cs.akimbomillis < gamemillis) { cs.akimbomillis = 0; cs.akimbo = false; }
        // events
        while(c.events.length()) // are ordered
        {
            timedevent *ev = c.events[0];
            if (ev->flush(&c, gamemillis)) delete c.events.remove(0);
            else break;
        }
        // timers
        loopvj(c.timers)
        {
            if (!c.timers[j]->valid || (c.timers[j]->type == GE_HEAL && (cs.health >= MAXHEALTH || cs.state != CS_ALIVE)))
            {
                delete c.timers.remove(j--);
                continue;
            }
            else if (c.timers[j]->millis <= gamemillis)
            {
                c.timers[j]->process(&c);
                delete c.timers.remove(j--);
            }
        }
    }
}

