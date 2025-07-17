#ifndef GAME_SERVER_ENTITIES_OLDTEE_H
#define GAME_SERVER_ENTITIES_OLDTEE_H

#include "botentity.h"

class COldTee : public CBotEntity
{
public:
	COldTee(CGameWorld *pWorld, vec2 Pos, Uuid BotID, STeeInfo TeeInfo);

	bool IsFriendlyDamage(CEntity *pFrom) override;
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon) override;
    const char *GetName() override;
    void Snap(int SnappingClient) override;
    bool TriggerGo(int ClientID, const char *pGoTo) override;
protected:
    void Action() override;
    int m_RandomEmoteTimer;
};

#endif // GAME_SERVER_ENTITIES_OLDTEE_H
