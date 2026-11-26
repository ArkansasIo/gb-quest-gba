#include <assert.h>
#include <stdint.h>
#include "../src/boss_phases.h"

int main(void) {
  /* Inclusive HP thresholds and malformed/empty health values. */
  assert(boss_phase_for_hp(100, 100) == BOSS_PHASE_I);
  assert(boss_phase_for_hp(76, 100) == BOSS_PHASE_I);
  assert(boss_phase_for_hp(75, 100) == BOSS_PHASE_II);
  assert(boss_phase_for_hp(51, 100) == BOSS_PHASE_II);
  assert(boss_phase_for_hp(50, 100) == BOSS_PHASE_III);
  assert(boss_phase_for_hp(26, 100) == BOSS_PHASE_III);
  assert(boss_phase_for_hp(25, 100) == BOSS_PHASE_IV);
  assert(boss_phase_for_hp(0, 100) == BOSS_PHASE_IV);
  assert(boss_phase_for_hp(120, 100) == BOSS_PHASE_I);
  assert(boss_phase_for_hp(1, 0) == BOSS_PHASE_I);

  /* Phase scaling remains inside the uint16_t range. */
  assert(boss_phase_damage(100, BOSS_PHASE_I) == 100);
  assert(boss_phase_damage(100, BOSS_PHASE_II) == 100);
  assert(boss_phase_damage(100, BOSS_PHASE_III) == 110);
  assert(boss_phase_damage(100, BOSS_PHASE_IV) == 125);
  assert(boss_phase_damage(65535, BOSS_PHASE_IV) == 65535);

  /* Cooldowns shorten while windups lengthen to preserve counterplay. */
  assert(boss_phase_move_delay(20, BOSS_PHASE_I) == 20);
  assert(boss_phase_move_delay(20, BOSS_PHASE_II) == 18);
  assert(boss_phase_move_delay(20, BOSS_PHASE_III) == 10);
  assert(boss_phase_move_delay(20, BOSS_PHASE_IV) == 10);
  assert(boss_phase_attack_delay(72, BOSS_PHASE_I) == 72);
  assert(boss_phase_attack_delay(72, BOSS_PHASE_II) == 63);
  assert(boss_phase_attack_delay(72, BOSS_PHASE_III) == 48);
  assert(boss_phase_attack_delay(72, BOSS_PHASE_IV) == 36);
  assert(boss_phase_windup(22, BOSS_PHASE_I) == 22);
  assert(boss_phase_windup(22, BOSS_PHASE_II) == 24);
  assert(boss_phase_windup(22, BOSS_PHASE_III) == 26);
  assert(boss_phase_windup(22, BOSS_PHASE_IV) == 30);
  assert(boss_phase_windup(250, BOSS_PHASE_IV) == 255);

  /* Heavy attacks become more frequent and gain range only at final phase. */
  assert(boss_phase_slam_period(BOSS_PHASE_I) == 3);
  assert(boss_phase_slam_period(BOSS_PHASE_II) == 2);
  assert(boss_phase_slam_period(BOSS_PHASE_III) == 2);
  assert(boss_phase_slam_period(BOSS_PHASE_IV) == 1);
  assert(boss_phase_slam_radius(BOSS_PHASE_III) == 2);
  assert(boss_phase_slam_radius(BOSS_PHASE_IV) == 3);
  assert(boss_phase_breath_range(BOSS_PHASE_III) == 3);
  assert(boss_phase_breath_range(BOSS_PHASE_IV) == 4);

  assert(boss_phase_title(BOSS_PHASE_I)[0] == 'B');
  assert(boss_phase_title(BOSS_PHASE_II)[11] == 'I');
  assert(boss_phase_title(BOSS_PHASE_III)[11] == 'I');
  assert(boss_phase_title(BOSS_PHASE_IV)[5] == 'F');
  return 0;
}
