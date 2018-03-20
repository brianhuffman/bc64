/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#import "SDL.h"
#import "SDLMain.h"
#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/* Use NIB Code */

static int    gArgc;
static char **gArgv;

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
    fprintf(stderr, "terminate\n");
    /* Post a SDL_QUIT event */
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}
@end


/* The main class of the application, the application's delegate */
@implementation SDLMain

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
    int status;
    fprintf(stderr, "applicationDidFinishLaunching\n");
    
    /* Hand off to main application code */
    status = SDL_main (gArgc, gArgv);

    /* We're done, thank you for playing */
    exit(status);
}
@end


#ifdef main
#  undef main
#endif


/* Main entry point to executable - should *not* be SDL_main! */
int main (int argc, const char *argv[])
{

    fprintf(stderr, "main\n");

    /* Copy the arguments into a global variable */
    gArgc = argc;
    gArgv = (char **) argv;
    
    [SDLApplication poseAsClass:[NSApplication class]];
    NSApplicationMain (argc, argv);

    return 0;
}
