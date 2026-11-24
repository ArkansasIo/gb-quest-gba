#include <assert.h>
#include <stdint.h>
#include "../src/combat_rules.h"

int main(void) {
  /* Neutral, resistant, and vulnerable hits. */
  assert(combat_apply_aspect(100, 0, 0, 1) == 100);
  assert(combat_apply_aspect(100, 1, 0, 1) == 50);
  assert(combat_apply_aspect(100, 0, 1, 1) == 200);

  /* Resistance takes precedence if malformed data sets both flags. */
  assert(combat_apply_aspect(100, 1, 1, 1) == 50);

  /* A different aspect bit must not affect the hit. */
  assert(combat_apply_aspect(100, 2, 4, 1) == 100);

  /* Vulnerability doubling must saturate rather than wrap uint16_t. */
  assert(combat_apply_aspect(40000, 0, 1, 1) == 65535);
  assert(combat_apply_aspect(0, 0, 1, 1) == 0);
  return 0;
}
