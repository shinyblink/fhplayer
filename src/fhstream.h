#ifndef FHSTREAM_H
#define FHSTREAM_H

#include <stdint.h>
#include <stdio.h>
#include <farbherd.h>

#include <SDL_mutex.h>
#include <SDL_thread.h>

// FHStream API

// If defined, ZSTD is used as a memory compressor.
// The number given is the quality.
// At 3, memory use is around 20% of the input file size, as opposed to 50%.
#define FHSTREAM_COMPRESSED 3

typedef struct fhstream_t {
	FILE * file;
	farbherd_header_t head;
	farbherd_frame_t frame;
	size_t storedFrameCount;
	// Contains sizes of compressed data
	size_t * storedFrameArrayS;
	// Contains ZSTD compressed ARGB32 data in host endianness
	uint8_t ** storedFrameArrayD;
	// The current frame. You should adjust this manually.
	size_t currentFrame;
	// 
	uint16_t * workspace;
	size_t dataSize;
	// Used by main thread
	void * dctx;
	// Alternate 'reader' thread.
	SDL_Thread * thread;
	// Locks the storedFrame (...) variables.
	SDL_mutex * mutex;
	int shuttingDown;
} fhstream_t;

// Create a stream from any FILE (including stdin).
// Returns NULL and sets error to a constant string on error, otherwise error is unset.
fhstream_t * fhstream_new(FILE * file, char ** error);
// Outputs a frame's ARGB32 data into a buffer, or returns !=0 for failure.
int fhstream_getframe(uint32_t * pixels, fhstream_t * stream);
// Closes the fhstream_t (but not the internal file)
void fhstream_close(fhstream_t * stream);

#endif
