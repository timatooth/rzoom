//
//  magick-helper.h
//  rzoom
//
//  Created by Tim Sullivan on 29/10/15.
//  Copyright © 2015 Tim Sullivan. All rights reserved.
//

#ifndef magick_helper_h
#define magick_helper_h

#define ThrowWandException(wand) \
{ \
char \
*description; \
\
ExceptionType \
severity; \
\
description=MagickGetException(wand,&severity); \
(void) fprintf(stderr,"%s %s %lu %s fookit\n",GetMagickModule(),description); \
description=(char *) MagickRelinquishMemory(description); \
exit(-1); \
}

#endif /* magick_helper_h */
