/* video.h - video functions for c64 emulator */
/* by Brian Huffman 12-1-00 */

void video_init (void);
void video_draw_sdl_screen (void);
void video_screenshot (void);

void callback_frame (void);