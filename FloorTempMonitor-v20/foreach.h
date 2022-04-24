/*
 * Loop over THING with loopvar
 *
 * For THING a THING_FIRST and THING_LAST (inclusive) have to be defined
 *
 * Example usage:
 *
 * #define DEV_INDEX_FIRST 10
 * #define DEV_INDEX_LAST  20
 *
 * FOREACH(DEV_INDEX, i)
 *  Serial.print(i)
 *
 * will output the numbers 10, 11, 12, 13 ... 20
 *
 * The loopvar is define in the macro expansion as an uint8_t, so use for small non-negative integer values (0-255)
 */

#define FOREACH(THING, loopvar)       for(uint8_t loopvar = THING##_FIRST ; loopvar <= THING##_LAST ; loopvar++)
