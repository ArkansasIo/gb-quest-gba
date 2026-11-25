#include <assert.h>
#include <stdint.h>
#include "../src/elite_affixes.h"

int main(void) {
  assert(ELITE_AFFIX_COUNT == 5);

  /* Berserker trades a faster cadence for stronger attacks. */
  assert(elite_affix_attack(100, ELITE_AFFIX_BERSERKER) == 125);
  assert(elite_affix_attack(100, ELITE_AFFIX_NONE) == 100);
  assert(elite_affix_attack(65535, ELITE_AFFIX_BERSERKER) == 65535);
  assert(elite_affix_delay(100, ELITE_AFFIX_BERSERKER) == 75);
  assert(elite_affix_delay(1, ELITE_AFFIX_BERSERKER) == 1);
  assert(elite_affix_delay(100, ELITE_AFFIX_IRONHIDE) == 100);

  /* Ironhide increases physical and magical defense. */
  assert(elite_affix_defense(100, ELITE_AFFIX_IRONHIDE) == 125);
  assert(elite_affix_defense(100, ELITE_AFFIX_NONE) == 100);
  assert(elite_affix_defense(65535, ELITE_AFFIX_IRONHIDE) == 65535);

  /* Leeching returns one fifth of actual HP damage; other affixes do not. */
  assert(elite_affix_leech(100, ELITE_AFFIX_LEECHING) == 20);
  assert(elite_affix_leech(4, ELITE_AFFIX_LEECHING) == 0);
  assert(elite_affix_leech(100, ELITE_AFFIX_VENOMOUS) == 0);

  assert(elite_affix_title(ELITE_AFFIX_BERSERKER)[0] == 'B');
  assert(elite_affix_title(ELITE_AFFIX_IRONHIDE)[0] == 'I');
  assert(elite_affix_title(ELITE_AFFIX_VENOMOUS)[0] == 'V');
  assert(elite_affix_title(ELITE_AFFIX_LEECHING)[0] == 'L');
  assert(elite_affix_title(ELITE_AFFIX_NONE)[0] == 'E');
  return 0;
}
