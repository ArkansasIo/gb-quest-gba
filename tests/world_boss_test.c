#include <assert.h>
#include <stdint.h>
#include "../src/world_boss.h"

int main(void) {
  /* Only the designated zone and an appropriately leveled hero qualify. */
  assert(!world_boss_zone_eligible(67, 720, 600));
  assert(!world_boss_zone_eligible(68, 519, 600));
  assert(world_boss_zone_eligible(68, 520, 600));
  assert(world_boss_zone_eligible(68, 720, 600));
  assert(!world_boss_zone_eligible(68, 100, 600));
  assert(!world_boss_zone_eligible(68, 1, 199));
  assert(world_boss_zone_eligible(68, 120, 200));

  /* Attack and defense bonuses saturate before legacy 16-bit overflow. */
  assert(world_boss_attack(100) == 150);
  assert(world_boss_attack(1) == 1);
  assert(world_boss_attack(60000) == 60000);
  assert(world_boss_attack(65535) == 60000);
  assert(world_boss_defense(100) == 125);
  assert(world_boss_defense(1) == 1);
  assert(world_boss_defense(60000) == 60000);

  /* XP doubles exactly until the uint32_t ceiling, then saturates. */
  assert(world_boss_reward_xp(100) == 200);
  assert(world_boss_reward_xp(0x7FFFFFFFUL) == 0xFFFFFFFEUL);
  assert(world_boss_reward_xp(0x80000000UL) == 0xFFFFFFFFUL);
  assert(world_boss_reward_xp(0xFFFFFFFFUL) == 0xFFFFFFFFUL);
  return 0;
}
