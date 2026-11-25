/*
 * Minimal miniaudio compilation unit for Sonora.
 * Strips unused high-level engine/decoding/encoding/node-graph code
 * to minimize binary footprint and steady-state memory usage.
 */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
