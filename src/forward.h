/* Shared Game Sync forwarding plumbing for the capture shims (Chroma, LightFX,
 * Logitech). One worker thread + WinHTTP fetches the pair token from /pair then
 * POSTs frame JSON to 127.0.0.1:9400/lighting/game-sync/frame, coalescing to
 * POST_MIN_INTERVAL_MS. Each shim calls forward_publish from its commit point;
 * the worker drains and POSTs off the game thread. rgb is R,G,B triples,
 * row-major. COLORREF wire format stays 0x00BBGGRR. */
#ifndef NEXUS_FORWARD_H
#define NEXUS_FORWARD_H

/* Largest grid: Chroma CUSTOM2 8x24 (Logitech bitmap is 6x21). */
#define FWD_MAX_BYTES (8 * 24 * 4)

/* DllMain DLL_PROCESS_ATTACH: init CS + auto-reset event. */
void forward_init(void);
/* DllMain DLL_PROCESS_DETACH: signal the worker to stop. */
void forward_shutdown(void);

/* Optional source-app title, sanitized to JSON-safe ASCII. Set once at SDK init,
 * before frames flow. */
void forward_capture_app_w(const wchar_t *title);
void forward_capture_app_a(const char *title);

/* Publish one frame; the worker coalesces + POSTs. effect is "CHROMA_CUSTOM" /
 * "CHROMA_CUSTOM2" / "CHROMA_STATIC". For CHROMA_STATIC pass rows=1,cols=1 and
 * rgb = 3 bytes. rgb is R,G,B triples row-major. */
void forward_publish(const char *device, const char *effect, int rows, int cols, const unsigned char *rgb);

/* Start the worker thread (idempotent). Shims call this from their SDK init. */
void forward_ensure_worker(void);

/* Debug logging gated by /DSHIM_LOG (off in production). Shims log SDK calls via
 * SLOG(...); the impl is in forward.c so all shims share one log + lock. */
#ifdef SHIM_LOG
void fwd_slog(const char *fmt, ...);
#define SLOG(...) fwd_slog(__VA_ARGS__)
#else
#define SLOG(...) ((void)0)
#endif

#endif
