#ifndef GANGSTAR_AUDIO_CHUNK_H
#define GANGSTAR_AUDIO_CHUNK_H
/* Only observed IMA layouts under the existing owning update lock qualify. */
static inline unsigned rio_audio_half_chunk(unsigned original,unsigned buffers,int known_stream,int owning_update) {
    return known_stream && owning_update && buffers==4 &&
        (original==35280 || original==25600) ? original/2 : original;
}
#endif
