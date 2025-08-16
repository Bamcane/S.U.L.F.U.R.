#ifndef GAME_SERVER_ENTITIES_BODYOFTEE_H
#define GAME_SERVER_ENTITIES_BODYOFTEE_H

#include "botentity.h"

class CBodyOfTee : public CBotEntity
{
public:
	CBodyOfTee(CGameWorld *pWorld, vec2 Pos, Uuid BotID);

	bool IsFriendlyDamage(CEntity *pFrom) override;
	bool TakeDamage(vec2 Force, vec2 Source, int Dmg, CEntity *pFrom, int Weapon) override;
	void Snap(int SnappingClient) override;
	void TriggerRead(int ClientID, const char **ppMessage) override;

protected:
	void Action() override;
};

#endif // GAME_SERVER_ENTITIES_BODYOFTEE_H
