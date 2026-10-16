/* TumatraPDF fork build configuration.
   Included by Version.h before its fallback defines. */

// fork versioning starts at 1.0 (independent of upstream 3.x);
// UPDATE_CHECK_VERA becomes "1.0.9" (release channel, not pre-release).
// BUMP THIS IN THE SAME COMMIT AS THE vN.N.N RELEASE TAG, or every released
// exe keeps reporting the old version and the update check loops
// (update.txt Latest is always compared against CURR_VERSION).
#define CURR_VERSION 1.0.11
#define CURR_VERSION_COMMA 1,0,11
