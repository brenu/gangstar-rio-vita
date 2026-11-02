#ifndef GANGSTAR_SAVE_IDENTITY_H
#define GANGSTAR_SAVE_IDENTITY_H
#include <stddef.h>
#include <stdint.h>
#define RIO_SAVE_ID_CAP 48
/* Reads the identity only after validating the native save's complete blocks
 * and checksum. Input bytes are never modified. */
int gangstar_save_read_identity(const uint8_t *data,size_t size,char identity[RIO_SAVE_ID_CAP]);
/* Startup only: preserve a verified snapshot, adopt an imported save's identity,
 * and persist it before native save validation can run. Returns 0 on failure;
 * callers must stop startup rather than let the game reset an unreadable save. */
int gangstar_save_prepare(const char *root,const char *fresh_id,
    char identity[RIO_SAVE_ID_CAP],char *error,size_t error_size);
#endif
