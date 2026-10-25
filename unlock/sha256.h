#ifndef GU_SHA256_H
#define GU_SHA256_H

#include <Uefi.h>

/*
 * One-shot SHA-256. Data must contain Size readable bytes, Size must be less
 * than 2^61 bytes, and Digest must have room for 32 bytes. Data may be NULL
 * when Size is zero. No heap allocation or firmware services are used.
 */
VOID GuSha256(const VOID *Data, UINTN Size, UINT8 Digest[32]);

/*
 * Compare against a NUL-terminated string of exactly 64 hexadecimal digits.
 * Upper- and lower-case digits are accepted. Malformed hex, NULL Hex64,
 * NULL Data with nonzero Size, and lengths outside SHA-256's domain fail.
 */
BOOLEAN GuHashMatches(const VOID *Data, UINTN Size, const char *Hex64);

/*
 * Write the SHA-256 of Data as 64 lower-case hexadecimal digits plus a NUL
 * into Hex. This is how a report records a hash nothing is compared against
 * yet, such as the PROM of a board whose profile is still being discovered.
 * Data and Size follow GuSha256; Hex must have room for 65 bytes.
 */
VOID GuHashHex(const VOID *Data, UINTN Size, char Hex[65]);

#endif
