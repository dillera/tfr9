// When fnbattleship was built, and from which fujinet-battleship, for the
// title.  The Makefile compiles this again whenever any other part of
// fnbattleship is compiled.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef BUILD_DATE
#define BUILD_DATE "?"
#endif
#ifndef UPSTREAM_VERSION
#define UPSTREAM_VERSION "?"
#endif

const char build_date[] = BUILD_DATE;
const char upstream_version[] = UPSTREAM_VERSION;
