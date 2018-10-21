// FHStream API
#include "fhstream.h"
#include <string.h>
#include <zstd.h>

static int fhstream_workthread(void * stream);

fhstream_t * fhstream_new(FILE * file, char ** error) {
	farbherd_header_t head;
	if (farbherd_read_farbherd_header(file, &head)) {
		*error = "Failed to read farbherd header";
		return NULL;
	}
	size_t datasize = farbherd_datasize(head.imageHead);
	if (!datasize) {
		*error = "Farbherd stream has a width or height of 0.";
		free(head.fileExtData);
		return NULL;
	}
	uint16_t * workspace = malloc(datasize);
	if (!workspace) {
		*error = "Failed to allocate workspace";
		free(head.fileExtData);
		return NULL;
	}
	fhstream_t * fst = malloc(sizeof(fhstream_t));
	if (!fst) {
		*error = "Failed to allocate fhstream_t";
		free(workspace);
		free(head.fileExtData);
		return NULL;
	}
	if (farbherd_init_farbherd_frame(&(fst->frame), head)) {
		*error = "Failed to init farbherd frame";
		free(workspace);
		free(head.fileExtData);
		free(fst);
		return NULL;
	}
	// Transfer of head.fileExtData pointer
	fst->head = head;
	fst->file = file;
	fst->storedFrameCount = 0;
	fst->storedFrameArrayS = NULL;
	fst->storedFrameArrayD = NULL;
	fst->currentFrame = 0;
	memset(workspace, 0, datasize);
	fst->workspace = workspace;
	fst->dataSize = datasize;
	fst->dctx = ZSTD_createDCtx();
	fst->shuttingDown = 0;
	fst->mutex = SDL_CreateMutex();
	fst->thread = SDL_CreateThread(fhstream_workthread, "fhstream", fst);
	return fst;
}

// Internal code to append a decoded frame. Frees the given buffer.
static int fhstream_appendframe(fhstream_t * stream, uint32_t * pixels, ZSTD_CCtx * context) {
#ifdef FHSTREAM_COMPRESSED
	size_t dz = ZSTD_compressBound(stream->dataSize / 2);
	uint8_t * compressedData = malloc(dz);
	if (!compressedData)
		return 1;
	size_t res = ZSTD_compressCCtx(context, compressedData, dz, pixels, stream->dataSize / 2, FHSTREAM_COMPRESSED);
	free(pixels);
	if (!ZSTD_isError(res)) {
		uint8_t * cData2 = realloc(compressedData, res);
		if (!cData2) {
			free(compressedData);
			return 1;
		} else {
			compressedData = cData2;
		}
	} else {
		return 1;
	}
#else
	void * compressedData = pixels;
	size_t res = stream->dataSize / 2;
#endif
	SDL_LockMutex(stream->mutex);
	size_t idx = stream->storedFrameCount++;
	size_t * idxa = realloc(stream->storedFrameArrayS, stream->storedFrameCount * sizeof(size_t));
	uint8_t ** idxb = realloc(stream->storedFrameArrayD, stream->storedFrameCount * sizeof(uint8_t **));
	// Neither of these operations have any bad effects if stuff goes wrong - they can be backed out of safely
	//  without any technical leak by subtracting 1 from storedFrameCount.
	// If this is tried again later, then realloc will presumably do nothing, and everything is fine.
	if (idxa)
		stream->storedFrameArrayS = idxa;
	if (idxb)
		stream->storedFrameArrayD = idxb;
	if (!(idxa && idxb)) {
		stream->storedFrameCount--;
		SDL_UnlockMutex(stream->mutex);
		free(compressedData);
		return 1;
	}
	idxa[idx] = res;
	idxb[idx] = compressedData;
	SDL_UnlockMutex(stream->mutex);
	return 0;
}

// Internal code to decode & append a decoded frame.
static int fhstream_nextframe(fhstream_t * stream, ZSTD_CCtx * context) {
	if (farbherd_read_farbherd_frame(stream->file, &(stream->frame), stream->head))
		return 1;
	farbherd_apply_delta(stream->workspace, stream->frame.deltas, stream->dataSize);
	// Now inject the frame.
	uint32_t * pixels = malloc(stream->dataSize / 2);
	if (!pixels)
		return 1;
	for (size_t ip = 0; ip < stream->dataSize / 8; ip++) {
		int r = (be16toh(stream->workspace[(ip * 4) + 0]) >> 8) & 0xFF;
		int g = (be16toh(stream->workspace[(ip * 4) + 1]) >> 8) & 0xFF;
		int b = (be16toh(stream->workspace[(ip * 4) + 2]) >> 8) & 0xFF;
		int a = (be16toh(stream->workspace[(ip * 4) + 3]) >> 8) & 0xFF;
		uint32_t colour = 0;
		colour |= r << 16;
		colour |= g << 8;
		colour |= b;
		colour |= a << 24;
		pixels[ip] = colour;
	}
	int res = fhstream_appendframe(stream, pixels, context);
	return res;
}

static int fhstream_workthread(void * sm) {
	// Note: contexts seem to just prevent memory thrashing, but that's fine
	ZSTD_CCtx * context = ZSTD_createCCtx();
	fhstream_t * stream = sm;
	while (!stream->shuttingDown) {
		if (fhstream_nextframe(stream, context)) {
			ZSTD_freeCCtx(context);
			return 1;
		}
	}
	ZSTD_freeCCtx(context);
	return 0;
}

int fhstream_getframe(uint32_t * pixels, fhstream_t * stream) {
	SDL_LockMutex(stream->mutex);
	size_t storedFrameCount = stream->storedFrameCount;
	SDL_UnlockMutex(stream->mutex);
	if (storedFrameCount <= stream->currentFrame)
		return 1;
#ifdef FHSTREAM_COMPRESSED
	// 'decode' frame
	ZSTD_decompressDCtx(stream->dctx, pixels, stream->dataSize / 2, stream->storedFrameArrayD[stream->currentFrame], stream->storedFrameArrayS[stream->currentFrame]);
#else
	memcpy(pixels, stream->storedFrameArrayD[stream->currentFrame], stream->dataSize / 2);
#endif
	return 0;
}
void fhstream_close(fhstream_t * stream) {
	stream->shuttingDown = 1;
	SDL_WaitThread(stream->thread, NULL);
	SDL_DestroyMutex(stream->mutex);
	for (size_t i = 0; i < stream->storedFrameCount; i++)
		free(stream->storedFrameArrayD[i]);
	free(stream->head.fileExtData);
	free(stream->frame.frameExtData);
	free(stream->frame.deltas);
	free(stream->workspace);
	ZSTD_freeDCtx(stream->dctx);
	free(stream);
}

