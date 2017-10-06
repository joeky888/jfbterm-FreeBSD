/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -
 * Copyright (C) 2003 Fumitoshi UKAI <ukai@debian.or.jp>
 * Copyright (C) 1999 Noritoshi MASUICHI <nmasu@ma3.justnet.ne.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY YUSUKE BABA ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL YUSUKE BABA BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (__linux__)
#include <linux/fb.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/fbio.h>
#else
#include <machine/console.h>
#endif
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <time.h>
#include <dev/wscons/wsconsio.h>
#endif

#include "framebuffer.h"
#include "getcap.h"
#include "palette.h"
#include "utilities.h"

static const uint16_t r256[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0xaa00, 0xaa00, 0xaa00, 0xaa00,
	0x5500, 0x5500, 0x5500, 0x5500, 0xff00, 0xff00, 0xff00, 0xff00,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0x8700, 0x8700, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xd700, 0xd700, 0xff00, 0xff00, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0x0800, 0x1200, 0x1c00, 0x2600, 0x3000, 0x3a00, 0x4400, 0x4e00,
	0x5800, 0x6200, 0x6c00, 0x7600, 0x8000, 0x8a00, 0x9400, 0x9e00,
	0xa800, 0xb200, 0xbc00, 0xc600, 0xd000, 0xda00, 0xe400, 0xee00
};

static const uint16_t g256[256] = {
	0x0000, 0x0000, 0xaa00, 0xaa00, 0x0000, 0x0000, 0xaa00, 0xaa00,
	0x5500, 0x5500, 0xff00, 0xff00, 0x5500, 0x5500, 0xff00, 0xff00,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5f00, 0x5f00,
	0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x8700, 0x8700, 0x8700, 0x8700,
	0x8700, 0x8700, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xaf00,
	0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xd700, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00, 0x5f00,
	0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0x8700, 0xaf00, 0xaf00,
	0xaf00, 0xaf00, 0xaf00, 0xaf00, 0xd700, 0xd700, 0xd700, 0xd700,
	0xd700, 0xd700, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0x0800, 0x1200, 0x1c00, 0x2600, 0x3000, 0x3a00, 0x4400, 0x4e00,
	0x5800, 0x6200, 0x6c00, 0x7600, 0x8000, 0x8a00, 0x9400, 0x9e00,
	0xa800, 0xb200, 0xbc00, 0xc600, 0xd000, 0xda00, 0xe400, 0xee00
};

static const uint16_t b256[256] = {
	0x0000, 0xaa00, 0x0000, 0xaa00, 0x0000, 0xaa00, 0x0000, 0xaa00,
	0x5500, 0xff00, 0x5500, 0xff00, 0x5500, 0xff00, 0x5500, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00,
	0x8700, 0xaf00, 0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00,
	0xd700, 0xff00, 0x0000, 0x5f00, 0x8700, 0xaf00, 0xd700, 0xff00,
	0x0800, 0x1200, 0x1c00, 0x2600, 0x3000, 0x3a00, 0x4400, 0x4e00,
	0x5800, 0x6200, 0x6c00, 0x7600, 0x8000, 0x8a00, 0x9400, 0x9e00,
	0xa800, 0xb200, 0xbc00, 0xc600, 0xd000, 0xda00, 0xe400, 0xee00
};

/* TODO: parse rgb.txt */
static const struct {
	const uint16_t r;
	const uint16_t g;
	const uint16_t b;
	const char *name;
} namedColor[] = {
	{ 0xff00, 0xfa00, 0xfa00, "snow"                 },
	{ 0xf800, 0xf800, 0xff00, "ghostwhite"           },
	{ 0xf500, 0xf500, 0xf500, "whitesmoke"           },
	{ 0xdc00, 0xdc00, 0xdc00, "gainsboro"            },
	{ 0xff00, 0xfa00, 0xf000, "floralwhite"          },
	{ 0xfd00, 0xf500, 0xe600, "oldlace"              },
	{ 0xfa00, 0xf000, 0xe600, "linen"                },
	{ 0xfa00, 0xeb00, 0xd700, "antiquewhite"         },
	{ 0xff00, 0xef00, 0xd500, "papayawhip"           },
	{ 0xff00, 0xeb00, 0xcd00, "blanchedalmond"       },
	{ 0xff00, 0xe400, 0xc400, "bisque"               },
	{ 0xff00, 0xda00, 0xb900, "peachpuff"            },
	{ 0xff00, 0xde00, 0xad00, "navajowhite"          },
	{ 0xff00, 0xe400, 0xb500, "moccasin"             },
	{ 0xff00, 0xf800, 0xdc00, "cornsilk"             },
	{ 0xff00, 0xff00, 0xf000, "ivory"                },
	{ 0xff00, 0xfa00, 0xcd00, "lemonchiffon"         },
	{ 0xff00, 0xf500, 0xee00, "seashell"             },
	{ 0xf000, 0xff00, 0xf000, "honeydew"             },
	{ 0xf500, 0xff00, 0xfa00, "mintcream"            },
	{ 0xf000, 0xff00, 0xff00, "azure"                },
	{ 0xf000, 0xf800, 0xff00, "aliceblue"            },
	{ 0xe600, 0xe600, 0xfa00, "lavender"             },
	{ 0xff00, 0xf000, 0xf500, "lavenderblush"        },
	{ 0xff00, 0xe400, 0xe100, "mistyrose"            },
	{ 0xff00, 0xff00, 0xff00, "white"                },
	{ 0x0000, 0x0000, 0x0000, "black"                },
	{ 0x2f00, 0x4f00, 0x4f00, "darkslategrey"        },
	{ 0x6900, 0x6900, 0x6900, "dimgrey"              },
	{ 0x7000, 0x8000, 0x9000, "slategrey"            },
	{ 0x7700, 0x8800, 0x9900, "lightslategrey"       },
	{ 0xbe00, 0xbe00, 0xbe00, "grey"                 },
	{ 0xd300, 0xd300, 0xd300, "lightgray"            },
	{ 0x1900, 0x1900, 0x7000, "midnightblue"         },
	{ 0x0000, 0x0000, 0x8000, "navyblue"             },
	{ 0x6400, 0x9500, 0xed00, "cornflowerblue"       },
	{ 0x4800, 0x3d00, 0x8b00, "darkslateblue"        },
	{ 0x6a00, 0x5a00, 0xcd00, "slateblue"            },
	{ 0x7b00, 0x6800, 0xee00, "mediumslateblue"      },
	{ 0x8400, 0x7000, 0xff00, "lightslateblue"       },
	{ 0x0000, 0x0000, 0xcd00, "mediumblue"           },
	{ 0x4100, 0x6900, 0xe100, "royalblue"            },
	{ 0x0000, 0x0000, 0xff00, "blue"                 },
	{ 0x1e00, 0x9000, 0xff00, "dodgerblue"           },
	{ 0x0000, 0xbf00, 0xff00, "deepskyblue"          },
	{ 0x8700, 0xce00, 0xeb00, "skyblue"              },
	{ 0x8700, 0xce00, 0xfa00, "lightskyblue"         },
	{ 0x4600, 0x8200, 0xb400, "steelblue"            },
	{ 0xb000, 0xc400, 0xde00, "lightsteelblue"       },
	{ 0xad00, 0xd800, 0xe600, "lightblue"            },
	{ 0xb000, 0xe000, 0xe600, "powderblue"           },
	{ 0xaf00, 0xee00, 0xee00, "paleturquoise"        },
	{ 0x0000, 0xce00, 0xd100, "darkturquoise"        },
	{ 0x4800, 0xd100, 0xcc00, "mediumturquoise"      },
	{ 0x4000, 0xe000, 0xd000, "turquoise"            },
	{ 0x0000, 0xff00, 0xff00, "cyan"                 },
	{ 0xe000, 0xff00, 0xff00, "lightcyan"            },
	{ 0x5f00, 0x9e00, 0xa000, "cadetblue"            },
	{ 0x6600, 0xcd00, 0xaa00, "mediumaquamarine"     },
	{ 0x7f00, 0xff00, 0xd400, "aquamarine"           },
	{ 0x0000, 0x6400, 0x0000, "darkgreen"            },
	{ 0x5500, 0x6b00, 0x2f00, "darkolivegreen"       },
	{ 0x8f00, 0xbc00, 0x8f00, "darkseagreen"         },
	{ 0x2e00, 0x8b00, 0x5700, "seagreen"             },
	{ 0x3c00, 0xb300, 0x7100, "mediumseagreen"       },
	{ 0x2000, 0xb200, 0xaa00, "lightseagreen"        },
	{ 0x9800, 0xfb00, 0x9800, "palegreen"            },
	{ 0x0000, 0xff00, 0x7f00, "springgreen"          },
	{ 0x7c00, 0xfc00, 0x0000, "lawngreen"            },
	{ 0x0000, 0xff00, 0x0000, "green"                },
	{ 0x7f00, 0xff00, 0x0000, "chartreuse"           },
	{ 0x0000, 0xfa00, 0x9a00, "mediumspringgreen"    },
	{ 0xad00, 0xff00, 0x2f00, "greenyellow"          },
	{ 0x3200, 0xcd00, 0x3200, "limegreen"            },
	{ 0x9a00, 0xcd00, 0x3200, "yellowgreen"          },
	{ 0x2200, 0x8b00, 0x2200, "forestgreen"          },
	{ 0x6b00, 0x8e00, 0x2300, "olivedrab"            },
	{ 0xbd00, 0xb700, 0x6b00, "darkkhaki"            },
	{ 0xf000, 0xe600, 0x8c00, "khaki"                },
	{ 0xee00, 0xe800, 0xaa00, "palegoldenrod"        },
	{ 0xfa00, 0xfa00, 0xd200, "lightgoldenrodyellow" },
	{ 0xff00, 0xff00, 0xe000, "lightyellow"          },
	{ 0xff00, 0xff00, 0x0000, "yellow"               },
	{ 0xff00, 0xd700, 0x0000, "gold"                 },
	{ 0xee00, 0xdd00, 0x8200, "lightgoldenrod"       },
	{ 0xda00, 0xa500, 0x2000, "goldenrod"            },
	{ 0xb800, 0x8600, 0x0b00, "darkgoldenrod"        },
	{ 0xbc00, 0x8f00, 0x8f00, "rosybrown"            },
	{ 0xcd00, 0x5c00, 0x5c00, "indianred"            },
	{ 0x8b00, 0x4500, 0x1300, "saddlebrown"          },
	{ 0xa000, 0x5200, 0x2d00, "sienna"               },
	{ 0xcd00, 0x8500, 0x3f00, "peru"                 },
	{ 0xde00, 0xb800, 0x8700, "burlywood"            },
	{ 0xf500, 0xf500, 0xdc00, "beige"                },
	{ 0xf500, 0xde00, 0xb300, "wheat"                },
	{ 0xf400, 0xa400, 0x6000, "sandybrown"           },
	{ 0xd200, 0xb400, 0x8c00, "tan"                  },
	{ 0xd200, 0x6900, 0x1e00, "chocolate"            },
	{ 0xb200, 0x2200, 0x2200, "firebrick"            },
	{ 0xa500, 0x2a00, 0x2a00, "brown"                },
	{ 0xe900, 0x9600, 0x7a00, "darksalmon"           },
	{ 0xfa00, 0x8000, 0x7200, "salmon"               },
	{ 0xff00, 0xa000, 0x7a00, "lightsalmon"          },
	{ 0xff00, 0xa500, 0x0000, "orange"               },
	{ 0xff00, 0x8c00, 0x0000, "darkorange"           },
	{ 0xff00, 0x7f00, 0x5000, "coral"                },
	{ 0xf000, 0x8000, 0x8000, "lightcoral"           },
	{ 0xff00, 0x6300, 0x4700, "tomato"               },
	{ 0xff00, 0x4500, 0x0000, "orangered"            },
	{ 0xff00, 0x0000, 0x0000, "red"                  },
	{ 0xff00, 0x6900, 0xb400, "hotpink"              },
	{ 0xff00, 0x1400, 0x9300, "deeppink"             },
	{ 0xff00, 0xc000, 0xcb00, "pink"                 },
	{ 0xff00, 0xb600, 0xc100, "lightpink"            },
	{ 0xdb00, 0x7000, 0x9300, "palevioletred"        },
	{ 0xb000, 0x3000, 0x6000, "maroon"               },
	{ 0xc700, 0x1500, 0x8500, "mediumvioletred"      },
	{ 0xd000, 0x2000, 0x9000, "violetred"            },
	{ 0xff00, 0x0000, 0xff00, "magenta"              },
	{ 0xee00, 0x8200, 0xee00, "violet"               },
	{ 0xdd00, 0xa000, 0xdd00, "plum"                 },
	{ 0xda00, 0x7000, 0xd600, "orchid"               },
	{ 0xba00, 0x5500, 0xd300, "mediumorchid"         },
	{ 0x9900, 0x3200, 0xcc00, "darkorchid"           },
	{ 0x9400, 0x0000, 0xd300, "darkviolet"           },
	{ 0x8a00, 0x2b00, 0xe200, "blueviolet"           },
	{ 0xa000, 0x2000, 0xf000, "purple"               },
	{ 0x9300, 0x7000, 0xdb00, "mediumpurple"         },
	{ 0xd800, 0xbf00, 0xd800, "thistle"              },
	{ 0xff00, 0xfa00, 0xfa00, "snow1"                },
	{ 0xee00, 0xe900, 0xe900, "snow2"                },
	{ 0xcd00, 0xc900, 0xc900, "snow3"                },
	{ 0x8b00, 0x8900, 0x8900, "snow4"                },
	{ 0xff00, 0xf500, 0xee00, "seashell1"            },
	{ 0xee00, 0xe500, 0xde00, "seashell2"            },
	{ 0xcd00, 0xc500, 0xbf00, "seashell3"            },
	{ 0x8b00, 0x8600, 0x8200, "seashell4"            },
	{ 0xff00, 0xef00, 0xdb00, "antiquewhite1"        },
	{ 0xee00, 0xdf00, 0xcc00, "antiquewhite2"        },
	{ 0xcd00, 0xc000, 0xb000, "antiquewhite3"        },
	{ 0x8b00, 0x8300, 0x7800, "antiquewhite4"        },
	{ 0xff00, 0xe400, 0xc400, "bisque1"              },
	{ 0xee00, 0xd500, 0xb700, "bisque2"              },
	{ 0xcd00, 0xb700, 0x9e00, "bisque3"              },
	{ 0x8b00, 0x7d00, 0x6b00, "bisque4"              },
	{ 0xff00, 0xda00, 0xb900, "peachpuff1"           },
	{ 0xee00, 0xcb00, 0xad00, "peachpuff2"           },
	{ 0xcd00, 0xaf00, 0x9500, "peachpuff3"           },
	{ 0x8b00, 0x7700, 0x6500, "peachpuff4"           },
	{ 0xff00, 0xde00, 0xad00, "navajowhite1"         },
	{ 0xee00, 0xcf00, 0xa100, "navajowhite2"         },
	{ 0xcd00, 0xb300, 0x8b00, "navajowhite3"         },
	{ 0x8b00, 0x7900, 0x5e00, "navajowhite4"         },
	{ 0xff00, 0xfa00, 0xcd00, "lemonchiffon1"        },
	{ 0xee00, 0xe900, 0xbf00, "lemonchiffon2"        },
	{ 0xcd00, 0xc900, 0xa500, "lemonchiffon3"        },
	{ 0x8b00, 0x8900, 0x7000, "lemonchiffon4"        },
	{ 0xff00, 0xf800, 0xdc00, "cornsilk1"            },
	{ 0xee00, 0xe800, 0xcd00, "cornsilk2"            },
	{ 0xcd00, 0xc800, 0xb100, "cornsilk3"            },
	{ 0x8b00, 0x8800, 0x7800, "cornsilk4"            },
	{ 0xff00, 0xff00, 0xf000, "ivory1"               },
	{ 0xee00, 0xee00, 0xe000, "ivory2"               },
	{ 0xcd00, 0xcd00, 0xc100, "ivory3"               },
	{ 0x8b00, 0x8b00, 0x8300, "ivory4"               },
	{ 0xf000, 0xff00, 0xf000, "honeydew1"            },
	{ 0xe000, 0xee00, 0xe000, "honeydew2"            },
	{ 0xc100, 0xcd00, 0xc100, "honeydew3"            },
	{ 0x8300, 0x8b00, 0x8300, "honeydew4"            },
	{ 0xff00, 0xf000, 0xf500, "lavenderblush1"       },
	{ 0xee00, 0xe000, 0xe500, "lavenderblush2"       },
	{ 0xcd00, 0xc100, 0xc500, "lavenderblush3"       },
	{ 0x8b00, 0x8300, 0x8600, "lavenderblush4"       },
	{ 0xff00, 0xe400, 0xe100, "mistyrose1"           },
	{ 0xee00, 0xd500, 0xd200, "mistyrose2"           },
	{ 0xcd00, 0xb700, 0xb500, "mistyrose3"           },
	{ 0x8b00, 0x7d00, 0x7b00, "mistyrose4"           },
	{ 0xf000, 0xff00, 0xff00, "azure1"               },
	{ 0xe000, 0xee00, 0xee00, "azure2"               },
	{ 0xc100, 0xcd00, 0xcd00, "azure3"               },
	{ 0x8300, 0x8b00, 0x8b00, "azure4"               },
	{ 0x8300, 0x6f00, 0xff00, "slateblue1"           },
	{ 0x7a00, 0x6700, 0xee00, "slateblue2"           },
	{ 0x6900, 0x5900, 0xcd00, "slateblue3"           },
	{ 0x4700, 0x3c00, 0x8b00, "slateblue4"           },
	{ 0x4800, 0x7600, 0xff00, "royalblue1"           },
	{ 0x4300, 0x6e00, 0xee00, "royalblue2"           },
	{ 0x3a00, 0x5f00, 0xcd00, "royalblue3"           },
	{ 0x2700, 0x4000, 0x8b00, "royalblue4"           },
	{ 0x0000, 0x0000, 0xff00, "blue1"                },
	{ 0x0000, 0x0000, 0xee00, "blue2"                },
	{ 0x0000, 0x0000, 0xcd00, "blue3"                },
	{ 0x0000, 0x0000, 0x8b00, "blue4"                },
	{ 0x1e00, 0x9000, 0xff00, "dodgerblue1"          },
	{ 0x1c00, 0x8600, 0xee00, "dodgerblue2"          },
	{ 0x1800, 0x7400, 0xcd00, "dodgerblue3"          },
	{ 0x1000, 0x4e00, 0x8b00, "dodgerblue4"          },
	{ 0x6300, 0xb800, 0xff00, "steelblue1"           },
	{ 0x5c00, 0xac00, 0xee00, "steelblue2"           },
	{ 0x4f00, 0x9400, 0xcd00, "steelblue3"           },
	{ 0x3600, 0x6400, 0x8b00, "steelblue4"           },
	{ 0x0000, 0xbf00, 0xff00, "deepskyblue1"         },
	{ 0x0000, 0xb200, 0xee00, "deepskyblue2"         },
	{ 0x0000, 0x9a00, 0xcd00, "deepskyblue3"         },
	{ 0x0000, 0x6800, 0x8b00, "deepskyblue4"         },
	{ 0x8700, 0xce00, 0xff00, "skyblue1"             },
	{ 0x7e00, 0xc000, 0xee00, "skyblue2"             },
	{ 0x6c00, 0xa600, 0xcd00, "skyblue3"             },
	{ 0x4a00, 0x7000, 0x8b00, "skyblue4"             },
	{ 0xb000, 0xe200, 0xff00, "lightskyblue1"        },
	{ 0xa400, 0xd300, 0xee00, "lightskyblue2"        },
	{ 0x8d00, 0xb600, 0xcd00, "lightskyblue3"        },
	{ 0x6000, 0x7b00, 0x8b00, "lightskyblue4"        },
	{ 0xc600, 0xe200, 0xff00, "slategray1"           },
	{ 0xb900, 0xd300, 0xee00, "slategray2"           },
	{ 0x9f00, 0xb600, 0xcd00, "slategray3"           },
	{ 0x6c00, 0x7b00, 0x8b00, "slategray4"           },
	{ 0xca00, 0xe100, 0xff00, "lightsteelblue1"      },
	{ 0xbc00, 0xd200, 0xee00, "lightsteelblue2"      },
	{ 0xa200, 0xb500, 0xcd00, "lightsteelblue3"      },
	{ 0x6e00, 0x7b00, 0x8b00, "lightsteelblue4"      },
	{ 0xbf00, 0xef00, 0xff00, "lightblue1"           },
	{ 0xb200, 0xdf00, 0xee00, "lightblue2"           },
	{ 0x9a00, 0xc000, 0xcd00, "lightblue3"           },
	{ 0x6800, 0x8300, 0x8b00, "lightblue4"           },
	{ 0xe000, 0xff00, 0xff00, "lightcyan1"           },
	{ 0xd100, 0xee00, 0xee00, "lightcyan2"           },
	{ 0xb400, 0xcd00, 0xcd00, "lightcyan3"           },
	{ 0x7a00, 0x8b00, 0x8b00, "lightcyan4"           },
	{ 0xbb00, 0xff00, 0xff00, "paleturquoise1"       },
	{ 0xae00, 0xee00, 0xee00, "paleturquoise2"       },
	{ 0x9600, 0xcd00, 0xcd00, "paleturquoise3"       },
	{ 0x6600, 0x8b00, 0x8b00, "paleturquoise4"       },
	{ 0x9800, 0xf500, 0xff00, "cadetblue1"           },
	{ 0x8e00, 0xe500, 0xee00, "cadetblue2"           },
	{ 0x7a00, 0xc500, 0xcd00, "cadetblue3"           },
	{ 0x5300, 0x8600, 0x8b00, "cadetblue4"           },
	{ 0x0000, 0xf500, 0xff00, "turquoise1"           },
	{ 0x0000, 0xe500, 0xee00, "turquoise2"           },
	{ 0x0000, 0xc500, 0xcd00, "turquoise3"           },
	{ 0x0000, 0x8600, 0x8b00, "turquoise4"           },
	{ 0x0000, 0xff00, 0xff00, "cyan1"                },
	{ 0x0000, 0xee00, 0xee00, "cyan2"                },
	{ 0x0000, 0xcd00, 0xcd00, "cyan3"                },
	{ 0x0000, 0x8b00, 0x8b00, "cyan4"                },
	{ 0x9700, 0xff00, 0xff00, "darkslategray1"       },
	{ 0x8d00, 0xee00, 0xee00, "darkslategray2"       },
	{ 0x7900, 0xcd00, 0xcd00, "darkslategray3"       },
	{ 0x5200, 0x8b00, 0x8b00, "darkslategray4"       },
	{ 0x7f00, 0xff00, 0xd400, "aquamarine1"          },
	{ 0x7600, 0xee00, 0xc600, "aquamarine2"          },
	{ 0x6600, 0xcd00, 0xaa00, "aquamarine3"          },
	{ 0x4500, 0x8b00, 0x7400, "aquamarine4"          },
	{ 0xc100, 0xff00, 0xc100, "darkseagreen1"        },
	{ 0xb400, 0xee00, 0xb400, "darkseagreen2"        },
	{ 0x9b00, 0xcd00, 0x9b00, "darkseagreen3"        },
	{ 0x6900, 0x8b00, 0x6900, "darkseagreen4"        },
	{ 0x5400, 0xff00, 0x9f00, "seagreen1"            },
	{ 0x4e00, 0xee00, 0x9400, "seagreen2"            },
	{ 0x4300, 0xcd00, 0x8000, "seagreen3"            },
	{ 0x2e00, 0x8b00, 0x5700, "seagreen4"            },
	{ 0x9a00, 0xff00, 0x9a00, "palegreen1"           },
	{ 0x9000, 0xee00, 0x9000, "palegreen2"           },
	{ 0x7c00, 0xcd00, 0x7c00, "palegreen3"           },
	{ 0x5400, 0x8b00, 0x5400, "palegreen4"           },
	{ 0x0000, 0xff00, 0x7f00, "springgreen1"         },
	{ 0x0000, 0xee00, 0x7600, "springgreen2"         },
	{ 0x0000, 0xcd00, 0x6600, "springgreen3"         },
	{ 0x0000, 0x8b00, 0x4500, "springgreen4"         },
	{ 0x0000, 0xff00, 0x0000, "green1"               },
	{ 0x0000, 0xee00, 0x0000, "green2"               },
	{ 0x0000, 0xcd00, 0x0000, "green3"               },
	{ 0x0000, 0x8b00, 0x0000, "green4"               },
	{ 0x7f00, 0xff00, 0x0000, "chartreuse1"          },
	{ 0x7600, 0xee00, 0x0000, "chartreuse2"          },
	{ 0x6600, 0xcd00, 0x0000, "chartreuse3"          },
	{ 0x4500, 0x8b00, 0x0000, "chartreuse4"          },
	{ 0xc000, 0xff00, 0x3e00, "olivedrab1"           },
	{ 0xb300, 0xee00, 0x3a00, "olivedrab2"           },
	{ 0x9a00, 0xcd00, 0x3200, "olivedrab3"           },
	{ 0x6900, 0x8b00, 0x2200, "olivedrab4"           },
	{ 0xca00, 0xff00, 0x7000, "darkolivegreen1"      },
	{ 0xbc00, 0xee00, 0x6800, "darkolivegreen2"      },
	{ 0xa200, 0xcd00, 0x5a00, "darkolivegreen3"      },
	{ 0x6e00, 0x8b00, 0x3d00, "darkolivegreen4"      },
	{ 0xff00, 0xf600, 0x8f00, "khaki1"               },
	{ 0xee00, 0xe600, 0x8500, "khaki2"               },
	{ 0xcd00, 0xc600, 0x7300, "khaki3"               },
	{ 0x8b00, 0x8600, 0x4e00, "khaki4"               },
	{ 0xff00, 0xec00, 0x8b00, "lightgoldenrod1"      },
	{ 0xee00, 0xdc00, 0x8200, "lightgoldenrod2"      },
	{ 0xcd00, 0xbe00, 0x7000, "lightgoldenrod3"      },
	{ 0x8b00, 0x8100, 0x4c00, "lightgoldenrod4"      },
	{ 0xff00, 0xff00, 0xe000, "lightyellow1"         },
	{ 0xee00, 0xee00, 0xd100, "lightyellow2"         },
	{ 0xcd00, 0xcd00, 0xb400, "lightyellow3"         },
	{ 0x8b00, 0x8b00, 0x7a00, "lightyellow4"         },
	{ 0xff00, 0xff00, 0x0000, "yellow1"              },
	{ 0xee00, 0xee00, 0x0000, "yellow2"              },
	{ 0xcd00, 0xcd00, 0x0000, "yellow3"              },
	{ 0x8b00, 0x8b00, 0x0000, "yellow4"              },
	{ 0xff00, 0xd700, 0x0000, "gold1"                },
	{ 0xee00, 0xc900, 0x0000, "gold2"                },
	{ 0xcd00, 0xad00, 0x0000, "gold3"                },
	{ 0x8b00, 0x7500, 0x0000, "gold4"                },
	{ 0xff00, 0xc100, 0x2500, "goldenrod1"           },
	{ 0xee00, 0xb400, 0x2200, "goldenrod2"           },
	{ 0xcd00, 0x9b00, 0x1d00, "goldenrod3"           },
	{ 0x8b00, 0x6900, 0x1400, "goldenrod4"           },
	{ 0xff00, 0xb900, 0x0f00, "darkgoldenrod1"       },
	{ 0xee00, 0xad00, 0x0e00, "darkgoldenrod2"       },
	{ 0xcd00, 0x9500, 0x0c00, "darkgoldenrod3"       },
	{ 0x8b00, 0x6500, 0x0800, "darkgoldenrod4"       },
	{ 0xff00, 0xc100, 0xc100, "rosybrown1"           },
	{ 0xee00, 0xb400, 0xb400, "rosybrown2"           },
	{ 0xcd00, 0x9b00, 0x9b00, "rosybrown3"           },
	{ 0x8b00, 0x6900, 0x6900, "rosybrown4"           },
	{ 0xff00, 0x6a00, 0x6a00, "indianred1"           },
	{ 0xee00, 0x6300, 0x6300, "indianred2"           },
	{ 0xcd00, 0x5500, 0x5500, "indianred3"           },
	{ 0x8b00, 0x3a00, 0x3a00, "indianred4"           },
	{ 0xff00, 0x8200, 0x4700, "sienna1"              },
	{ 0xee00, 0x7900, 0x4200, "sienna2"              },
	{ 0xcd00, 0x6800, 0x3900, "sienna3"              },
	{ 0x8b00, 0x4700, 0x2600, "sienna4"              },
	{ 0xff00, 0xd300, 0x9b00, "burlywood1"           },
	{ 0xee00, 0xc500, 0x9100, "burlywood2"           },
	{ 0xcd00, 0xaa00, 0x7d00, "burlywood3"           },
	{ 0x8b00, 0x7300, 0x5500, "burlywood4"           },
	{ 0xff00, 0xe700, 0xba00, "wheat1"               },
	{ 0xee00, 0xd800, 0xae00, "wheat2"               },
	{ 0xcd00, 0xba00, 0x9600, "wheat3"               },
	{ 0x8b00, 0x7e00, 0x6600, "wheat4"               },
	{ 0xff00, 0xa500, 0x4f00, "tan1"                 },
	{ 0xee00, 0x9a00, 0x4900, "tan2"                 },
	{ 0xcd00, 0x8500, 0x3f00, "tan3"                 },
	{ 0x8b00, 0x5a00, 0x2b00, "tan4"                 },
	{ 0xff00, 0x7f00, 0x2400, "chocolate1"           },
	{ 0xee00, 0x7600, 0x2100, "chocolate2"           },
	{ 0xcd00, 0x6600, 0x1d00, "chocolate3"           },
	{ 0x8b00, 0x4500, 0x1300, "chocolate4"           },
	{ 0xff00, 0x3000, 0x3000, "firebrick1"           },
	{ 0xee00, 0x2c00, 0x2c00, "firebrick2"           },
	{ 0xcd00, 0x2600, 0x2600, "firebrick3"           },
	{ 0x8b00, 0x1a00, 0x1a00, "firebrick4"           },
	{ 0xff00, 0x4000, 0x4000, "brown1"               },
	{ 0xee00, 0x3b00, 0x3b00, "brown2"               },
	{ 0xcd00, 0x3300, 0x3300, "brown3"               },
	{ 0x8b00, 0x2300, 0x2300, "brown4"               },
	{ 0xff00, 0x8c00, 0x6900, "salmon1"              },
	{ 0xee00, 0x8200, 0x6200, "salmon2"              },
	{ 0xcd00, 0x7000, 0x5400, "salmon3"              },
	{ 0x8b00, 0x4c00, 0x3900, "salmon4"              },
	{ 0xff00, 0xa000, 0x7a00, "lightsalmon1"         },
	{ 0xee00, 0x9500, 0x7200, "lightsalmon2"         },
	{ 0xcd00, 0x8100, 0x6200, "lightsalmon3"         },
	{ 0x8b00, 0x5700, 0x4200, "lightsalmon4"         },
	{ 0xff00, 0xa500, 0x0000, "orange1"              },
	{ 0xee00, 0x9a00, 0x0000, "orange2"              },
	{ 0xcd00, 0x8500, 0x0000, "orange3"              },
	{ 0x8b00, 0x5a00, 0x0000, "orange4"              },
	{ 0xff00, 0x7f00, 0x0000, "darkorange1"          },
	{ 0xee00, 0x7600, 0x0000, "darkorange2"          },
	{ 0xcd00, 0x6600, 0x0000, "darkorange3"          },
	{ 0x8b00, 0x4500, 0x0000, "darkorange4"          },
	{ 0xff00, 0x7200, 0x5600, "coral1"               },
	{ 0xee00, 0x6a00, 0x5000, "coral2"               },
	{ 0xcd00, 0x5b00, 0x4500, "coral3"               },
	{ 0x8b00, 0x3e00, 0x2f00, "coral4"               },
	{ 0xff00, 0x6300, 0x4700, "tomato1"              },
	{ 0xee00, 0x5c00, 0x4200, "tomato2"              },
	{ 0xcd00, 0x4f00, 0x3900, "tomato3"              },
	{ 0x8b00, 0x3600, 0x2600, "tomato4"              },
	{ 0xff00, 0x4500, 0x0000, "orangered1"           },
	{ 0xee00, 0x4000, 0x0000, "orangered2"           },
	{ 0xcd00, 0x3700, 0x0000, "orangered3"           },
	{ 0x8b00, 0x2500, 0x0000, "orangered4"           },
	{ 0xff00, 0x0000, 0x0000, "red1"                 },
	{ 0xee00, 0x0000, 0x0000, "red2"                 },
	{ 0xcd00, 0x0000, 0x0000, "red3"                 },
	{ 0x8b00, 0x0000, 0x0000, "red4"                 },
	{ 0xff00, 0x1400, 0x9300, "deeppink1"            },
	{ 0xee00, 0x1200, 0x8900, "deeppink2"            },
	{ 0xcd00, 0x1000, 0x7600, "deeppink3"            },
	{ 0x8b00, 0x0a00, 0x5000, "deeppink4"            },
	{ 0xff00, 0x6e00, 0xb400, "hotpink1"             },
	{ 0xee00, 0x6a00, 0xa700, "hotpink2"             },
	{ 0xcd00, 0x6000, 0x9000, "hotpink3"             },
	{ 0x8b00, 0x3a00, 0x6200, "hotpink4"             },
	{ 0xff00, 0xb500, 0xc500, "pink1"                },
	{ 0xee00, 0xa900, 0xb800, "pink2"                },
	{ 0xcd00, 0x9100, 0x9e00, "pink3"                },
	{ 0x8b00, 0x6300, 0x6c00, "pink4"                },
	{ 0xff00, 0xae00, 0xb900, "lightpink1"           },
	{ 0xee00, 0xa200, 0xad00, "lightpink2"           },
	{ 0xcd00, 0x8c00, 0x9500, "lightpink3"           },
	{ 0x8b00, 0x5f00, 0x6500, "lightpink4"           },
	{ 0xff00, 0x8200, 0xab00, "palevioletred1"       },
	{ 0xee00, 0x7900, 0x9f00, "palevioletred2"       },
	{ 0xcd00, 0x6800, 0x8900, "palevioletred3"       },
	{ 0x8b00, 0x4700, 0x5d00, "palevioletred4"       },
	{ 0xff00, 0x3400, 0xb300, "maroon1"              },
	{ 0xee00, 0x3000, 0xa700, "maroon2"              },
	{ 0xcd00, 0x2900, 0x9000, "maroon3"              },
	{ 0x8b00, 0x1c00, 0x6200, "maroon4"              },
	{ 0xff00, 0x3e00, 0x9600, "violetred1"           },
	{ 0xee00, 0x3a00, 0x8c00, "violetred2"           },
	{ 0xcd00, 0x3200, 0x7800, "violetred3"           },
	{ 0x8b00, 0x2200, 0x5200, "violetred4"           },
	{ 0xff00, 0x0000, 0xff00, "magenta1"             },
	{ 0xee00, 0x0000, 0xee00, "magenta2"             },
	{ 0xcd00, 0x0000, 0xcd00, "magenta3"             },
	{ 0x8b00, 0x0000, 0x8b00, "magenta4"             },
	{ 0xff00, 0x8300, 0xfa00, "orchid1"              },
	{ 0xee00, 0x7a00, 0xe900, "orchid2"              },
	{ 0xcd00, 0x6900, 0xc900, "orchid3"              },
	{ 0x8b00, 0x4700, 0x8900, "orchid4"              },
	{ 0xff00, 0xbb00, 0xff00, "plum1"                },
	{ 0xee00, 0xae00, 0xee00, "plum2"                },
	{ 0xcd00, 0x9600, 0xcd00, "plum3"                },
	{ 0x8b00, 0x6600, 0x8b00, "plum4"                },
	{ 0xe000, 0x6600, 0xff00, "mediumorchid1"        },
	{ 0xd100, 0x5f00, 0xee00, "mediumorchid2"        },
	{ 0xb400, 0x5200, 0xcd00, "mediumorchid3"        },
	{ 0x7a00, 0x3700, 0x8b00, "mediumorchid4"        },
	{ 0xbf00, 0x3e00, 0xff00, "darkorchid1"          },
	{ 0xb200, 0x3a00, 0xee00, "darkorchid2"          },
	{ 0x9a00, 0x3200, 0xcd00, "darkorchid3"          },
	{ 0x6800, 0x2200, 0x8b00, "darkorchid4"          },
	{ 0x9b00, 0x3000, 0xff00, "purple1"              },
	{ 0x9100, 0x2c00, 0xee00, "purple2"              },
	{ 0x7d00, 0x2600, 0xcd00, "purple3"              },
	{ 0x5500, 0x1a00, 0x8b00, "purple4"              },
	{ 0xab00, 0x8200, 0xff00, "mediumpurple1"        },
	{ 0x9f00, 0x7900, 0xee00, "mediumpurple2"        },
	{ 0x8900, 0x6800, 0xcd00, "mediumpurple3"        },
	{ 0x5d00, 0x4700, 0x8b00, "mediumpurple4"        },
	{ 0xff00, 0xe100, 0xff00, "thistle1"             },
	{ 0xee00, 0xd200, 0xee00, "thistle2"             },
	{ 0xcd00, 0xb500, 0xcd00, "thistle3"             },
	{ 0x8b00, 0x7b00, 0x8b00, "thistle4"             },
	{ 0x0000, 0x0000, 0x0000, "grey0"                },
	{ 0x0300, 0x0300, 0x0300, "grey1"                },
	{ 0x0500, 0x0500, 0x0500, "grey2"                },
	{ 0x0800, 0x0800, 0x0800, "grey3"                },
	{ 0x0a00, 0x0a00, 0x0a00, "grey4"                },
	{ 0x0d00, 0x0d00, 0x0d00, "grey5"                },
	{ 0x0f00, 0x0f00, 0x0f00, "grey6"                },
	{ 0x1200, 0x1200, 0x1200, "grey7"                },
	{ 0x1400, 0x1400, 0x1400, "grey8"                },
	{ 0x1700, 0x1700, 0x1700, "grey9"                },
	{ 0x1a00, 0x1a00, 0x1a00, "grey10"               },
	{ 0x1c00, 0x1c00, 0x1c00, "grey11"               },
	{ 0x1f00, 0x1f00, 0x1f00, "grey12"               },
	{ 0x2100, 0x2100, 0x2100, "grey13"               },
	{ 0x2400, 0x2400, 0x2400, "grey14"               },
	{ 0x2600, 0x2600, 0x2600, "grey15"               },
	{ 0x2900, 0x2900, 0x2900, "grey16"               },
	{ 0x2b00, 0x2b00, 0x2b00, "grey17"               },
	{ 0x2e00, 0x2e00, 0x2e00, "grey18"               },
	{ 0x3000, 0x3000, 0x3000, "grey19"               },
	{ 0x3300, 0x3300, 0x3300, "grey20"               },
	{ 0x3600, 0x3600, 0x3600, "grey21"               },
	{ 0x3800, 0x3800, 0x3800, "grey22"               },
	{ 0x3b00, 0x3b00, 0x3b00, "grey23"               },
	{ 0x3d00, 0x3d00, 0x3d00, "grey24"               },
	{ 0x4000, 0x4000, 0x4000, "grey25"               },
	{ 0x4200, 0x4200, 0x4200, "grey26"               },
	{ 0x4500, 0x4500, 0x4500, "grey27"               },
	{ 0x4700, 0x4700, 0x4700, "grey28"               },
	{ 0x4a00, 0x4a00, 0x4a00, "grey29"               },
	{ 0x4d00, 0x4d00, 0x4d00, "grey30"               },
	{ 0x4f00, 0x4f00, 0x4f00, "grey31"               },
	{ 0x5200, 0x5200, 0x5200, "grey32"               },
	{ 0x5400, 0x5400, 0x5400, "grey33"               },
	{ 0x5700, 0x5700, 0x5700, "grey34"               },
	{ 0x5900, 0x5900, 0x5900, "grey35"               },
	{ 0x5c00, 0x5c00, 0x5c00, "grey36"               },
	{ 0x5e00, 0x5e00, 0x5e00, "grey37"               },
	{ 0x6100, 0x6100, 0x6100, "grey38"               },
	{ 0x6300, 0x6300, 0x6300, "grey39"               },
	{ 0x6600, 0x6600, 0x6600, "grey40"               },
	{ 0x6900, 0x6900, 0x6900, "grey41"               },
	{ 0x6b00, 0x6b00, 0x6b00, "grey42"               },
	{ 0x6e00, 0x6e00, 0x6e00, "grey43"               },
	{ 0x7000, 0x7000, 0x7000, "grey44"               },
	{ 0x7300, 0x7300, 0x7300, "grey45"               },
	{ 0x7500, 0x7500, 0x7500, "grey46"               },
	{ 0x7800, 0x7800, 0x7800, "grey47"               },
	{ 0x7a00, 0x7a00, 0x7a00, "grey48"               },
	{ 0x7d00, 0x7d00, 0x7d00, "grey49"               },
	{ 0x7f00, 0x7f00, 0x7f00, "grey50"               },
	{ 0x8200, 0x8200, 0x8200, "grey51"               },
	{ 0x8500, 0x8500, 0x8500, "grey52"               },
	{ 0x8700, 0x8700, 0x8700, "grey53"               },
	{ 0x8a00, 0x8a00, 0x8a00, "grey54"               },
	{ 0x8c00, 0x8c00, 0x8c00, "grey55"               },
	{ 0x8f00, 0x8f00, 0x8f00, "grey56"               },
	{ 0x9100, 0x9100, 0x9100, "grey57"               },
	{ 0x9400, 0x9400, 0x9400, "grey58"               },
	{ 0x9600, 0x9600, 0x9600, "grey59"               },
	{ 0x9900, 0x9900, 0x9900, "grey60"               },
	{ 0x9c00, 0x9c00, 0x9c00, "grey61"               },
	{ 0x9e00, 0x9e00, 0x9e00, "grey62"               },
	{ 0xa100, 0xa100, 0xa100, "grey63"               },
	{ 0xa300, 0xa300, 0xa300, "grey64"               },
	{ 0xa600, 0xa600, 0xa600, "grey65"               },
	{ 0xa800, 0xa800, 0xa800, "grey66"               },
	{ 0xab00, 0xab00, 0xab00, "grey67"               },
	{ 0xad00, 0xad00, 0xad00, "grey68"               },
	{ 0xb000, 0xb000, 0xb000, "grey69"               },
	{ 0xb300, 0xb300, 0xb300, "grey70"               },
	{ 0xb500, 0xb500, 0xb500, "grey71"               },
	{ 0xb800, 0xb800, 0xb800, "grey72"               },
	{ 0xba00, 0xba00, 0xba00, "grey73"               },
	{ 0xbd00, 0xbd00, 0xbd00, "grey74"               },
	{ 0xbf00, 0xbf00, 0xbf00, "grey75"               },
	{ 0xc200, 0xc200, 0xc200, "grey76"               },
	{ 0xc400, 0xc400, 0xc400, "grey77"               },
	{ 0xc700, 0xc700, 0xc700, "grey78"               },
	{ 0xc900, 0xc900, 0xc900, "grey79"               },
	{ 0xcc00, 0xcc00, 0xcc00, "grey80"               },
	{ 0xcf00, 0xcf00, 0xcf00, "grey81"               },
	{ 0xd100, 0xd100, 0xd100, "grey82"               },
	{ 0xd400, 0xd400, 0xd400, "grey83"               },
	{ 0xd600, 0xd600, 0xd600, "grey84"               },
	{ 0xd900, 0xd900, 0xd900, "grey85"               },
	{ 0xdb00, 0xdb00, 0xdb00, "grey86"               },
	{ 0xde00, 0xde00, 0xde00, "grey87"               },
	{ 0xe000, 0xe000, 0xe000, "grey88"               },
	{ 0xe300, 0xe300, 0xe300, "grey89"               },
	{ 0xe500, 0xe500, 0xe500, "grey90"               },
	{ 0xe800, 0xe800, 0xe800, "grey91"               },
	{ 0xeb00, 0xeb00, 0xeb00, "grey92"               },
	{ 0xed00, 0xed00, 0xed00, "grey93"               },
	{ 0xf000, 0xf000, 0xf000, "grey94"               },
	{ 0xf200, 0xf200, 0xf200, "grey95"               },
	{ 0xf500, 0xf500, 0xf500, "grey96"               },
	{ 0xf700, 0xf700, 0xf700, "grey97"               },
	{ 0xfa00, 0xfa00, 0xfa00, "grey98"               },
	{ 0xfc00, 0xfc00, 0xfc00, "grey99"               },
	{ 0xff00, 0xff00, 0xff00, "grey100"              },
	{ 0xa900, 0xa900, 0xa900, "darkgray"             },
	{ 0x0000, 0x0000, 0x8b00, "darkblue"             },
	{ 0x0000, 0x8b00, 0x8b00, "darkcyan"             },
	{ 0x8b00, 0x0000, 0x8b00, "darkmagenta"          },
	{ 0x8b00, 0x0000, 0x0000, "darkred"              },
	{ 0x9000, 0xee00, 0x9000, "lightgreen"           },
	{ 0x0000, 0x0000, 0x0000, NULL                   }
};

static const uint8_t ansiToVGA[] = {
	0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15
};

static bool hasColorMap;
static bool colorMapSaved;

#if defined (__linux__)
static unsigned short or[256], og[256], ob[256], ot[256];
static struct fb_cmap ocmap = { 0, 256, or, og, ob, ot };
static unsigned short nr[256], ng[256], nb[256];
static struct fb_cmap ncmap = { 0, 256, nr, ng, nb, NULL };
#elif defined (__FreeBSD__)
static u_char or[256], og[256], ob[256], ot[256];
static video_color_palette_t ocmap = { 0, 256, or, og, ob, ot };
static u_char nr[256], ng[256], nb[256];
static video_color_palette_t ncmap = { 0, 256, nr, ng, nb, NULL };
#elif defined (__NetBSD__) || defined (__OpenBSD__)
static u_char or[256], og[256], ob[256];
static struct wsdisplay_cmap ocmap = { 0, 256, or, og, ob };
static u_char nr[256], ng[256], nb[256];
static struct wsdisplay_cmap ncmap = { 0, 256, nr, ng, nb };
#else
        #error not implement
#endif

static u_int bitsPerPixel;
static uint32_t trueColor32[256];
static uint8_t  trueColor24[3][256];
static uint16_t trueColor16[256];
static int rLength, gLength, bLength;
static int rOffset, gOffset, bOffset;
static int rIndex, gIndex, bIndex;

static void setRGBIndex(void);
static void setColor(int i, uint16_t r, uint16_t g, uint16_t b);
#if defined (__NetBSD__)
static void setOldColorMap(void);
#endif

#if defined (__linux__)
void palette_initialize(const struct fb_var_screeninfo *fb_var_screeninfo,
			const struct fb_fix_screeninfo *fb_fix_screeninfo)
{
	int i;

	assert(fb_var_screeninfo != NULL);
	assert(fb_fix_screeninfo != NULL);

	bitsPerPixel = fb_var_screeninfo->bits_per_pixel;
	if ((fb_fix_screeninfo->type == FB_TYPE_PACKED_PIXELS &&
	     fb_fix_screeninfo->visual == FB_VISUAL_PSEUDOCOLOR &&
	     bitsPerPixel == 8) ||
	    (fb_fix_screeninfo->type == FB_TYPE_VGA_PLANES &&
	     fb_fix_screeninfo->visual == FB_VISUAL_PSEUDOCOLOR &&
	     bitsPerPixel == 4)) {
		ocmap.len = 1 << bitsPerPixel;
		ncmap.len = 1 << bitsPerPixel;
		hasColorMap = true;
	}
	if (hasColorMap) {
		framebuffer_getColorMap(&ocmap);
		colorMapSaved = true;
	}
	rLength = fb_var_screeninfo->red.length;
	gLength = fb_var_screeninfo->green.length;
	bLength = fb_var_screeninfo->blue.length;
	rOffset = fb_var_screeninfo->red.offset;
	gOffset = fb_var_screeninfo->green.offset;
	bOffset = fb_var_screeninfo->blue.offset;
	setRGBIndex();
	for (i = 0; i < 256; i++)
		setColor(i, r256[i], g256[i], b256[i]);
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
}
#elif defined (__FreeBSD__)
void palette_initialize(const video_info_t *video_info,
			const video_adapter_info_t *video_adapter_info)
{
	int i;

	assert(video_info != NULL);
	assert(video_adapter_info != NULL);

	bitsPerPixel = video_info->vi_depth;
	if ((video_adapter_info->va_flags & V_ADP_PALETTE) &&
	    ((video_info->vi_mem_model == V_INFO_MM_PACKED &&
	      bitsPerPixel == 8) ||
	     (video_info->vi_mem_model == V_INFO_MM_PLANAR &&
	      bitsPerPixel == 4))) {
		ocmap.count = 1 << bitsPerPixel;
		ncmap.count = 1 << bitsPerPixel;
		hasColorMap = true;
	}
	if (hasColorMap) {
		framebuffer_getColorMap(&ocmap);
		colorMapSaved = true;
	}
	rLength = video_info->vi_pixel_fsizes[0];
	gLength = video_info->vi_pixel_fsizes[1];
	bLength = video_info->vi_pixel_fsizes[2];
	rOffset = video_info->vi_pixel_fields[0];
	gOffset = video_info->vi_pixel_fields[1];
	bOffset = video_info->vi_pixel_fields[2];
	setRGBIndex();
	for (i = 0; i < 256; i++)
		setColor(i, r256[i], g256[i], b256[i]);
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void palette_initialize(const struct wsdisplay_fbinfo *wsdisplay_fbinfo,
			const int wsdisplay_type)
{
	int i;

	assert(wsdisplay_fbinfo != NULL);

	bitsPerPixel = wsdisplay_fbinfo->depth;
	if ((wsdisplay_fbinfo->cmsize > 0 && bitsPerPixel == 8)) {
		ocmap.count = wsdisplay_fbinfo->cmsize;
		ncmap.count = wsdisplay_fbinfo->cmsize;
		hasColorMap = true;
	}
	if (hasColorMap) {
#if defined (__NetBSD__)
		if (wsdisplay_type == WSDISPLAY_TYPE_VESA)
			setOldColorMap();
		else
			framebuffer_getColorMap(&ocmap);
#elif defined (__OpenBSD__)
		framebuffer_getColorMap(&ocmap);
#endif
		colorMapSaved = true;
	}
	switch (bitsPerPixel) {
	case 15:
		/* RGB 0:5:5:5 */
		rLength =  5; gLength =  5; bLength =  5;
		rOffset = 10; gOffset =  5; bOffset =  0;
		break;
	case 16:
		/* RGB 5:6:5 */
		rLength =  5; gLength =  6; bLength =  5;
		rOffset = 11; gOffset =  5; bOffset =  0;
#if 0
		/* ARGB 1:5:5:5 */
		rLength =  5; gLength =  5; bLength =  5;
		rOffset = 10; gOffset =  5; bOffset =  0;
#endif
		break;
	case 24:
		/* RGB 8:8:8 */
		rLength =  8; gLength =  8; bLength =  8;
		rOffset = 16; gOffset =  8; bOffset =  0;
		break;
	case 32:
		/* ARGB 8:8:8:8 */
		rLength =  8; gLength =  8; bLength =  8;
		rOffset = 16; gOffset =  8; bOffset =  0;
#if 0
		/* RGBA 8:8:8:8 */
		rLength =  8; gLength =  8; bLength =  8;
		rOffset = 24; gOffset = 16; bOffset =  8;
#endif
		break;
	default:
		break;
	}
	setRGBIndex();
	for (i = 0; i < 256; i++)
		setColor(i, r256[i], g256[i], b256[i]);
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
}
#else
        #error not implement
#endif

void palette_configure(TCaps *caps)
{
	int i;
	const char *config;
	char name[10];

	assert(caps != NULL);

	for (i = 0; i < 16; i++) {
		snprintf(name, sizeof(name), "color.%d", i);
		name[sizeof(name) - 1] = '\0';
		config = caps_findFirst(caps, name);
		if (config != NULL)
			if (!palette_update(i, config))
				warnx("Invalid color (%d): %s", i, config);
	}
}

static void setRGBIndex(void)
{
	if (rOffset > gOffset && rOffset > bOffset && gOffset > bOffset)
		rIndex = 0, gIndex = 1, bIndex = 2;
	else if (rOffset > gOffset && rOffset > bOffset && bOffset > gOffset)
		rIndex = 0, bIndex = 1, gIndex = 2;
	else if (gOffset > rOffset && gOffset > bOffset && rOffset > bOffset)
		gIndex = 0, rIndex = 1, bIndex = 2;
	else if (gOffset > rOffset && gOffset > bOffset && bOffset > rOffset)
		gIndex = 0, bIndex = 1, rIndex = 2;
	else if (bOffset > rOffset && bOffset > gOffset && rOffset > gOffset)
		bIndex = 0, rIndex = 1, gIndex = 2;
	else if (bOffset > rOffset && bOffset > gOffset && gOffset > rOffset)
		bIndex = 0, gIndex = 1, rIndex = 2;
	else
		rIndex = 0, gIndex = 1, bIndex = 2;
}

static void setColor(int i, uint16_t r, uint16_t g, uint16_t b)
{
	switch (bitsPerPixel) {
	case 4:
		/* FALLTHROUGH */
	case 8:
#if defined (__linux__)
		nr[i] = r;
		ng[i] = g;
		nb[i] = b;
#elif defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)
		nr[i] = (u_char)(r >> 8);
		ng[i] = (u_char)(g >> 8);
		nb[i] = (u_char)(b >> 8);
#else
        #error not implement
#endif
		break;
	case 15:
		/* FALLTHROUGH */
	case 16:
		trueColor16[i] =
			((r >> (16 - rLength)) << rOffset) |
			((g >> (16 - gLength)) << gOffset) |
			((b >> (16 - bLength)) << bOffset);
		break;
	case 24:
		if (rIndex == 0 && gIndex == 1 && bIndex == 2) {
			trueColor24[0][i] = (uint8_t)(r >> 8);
			trueColor24[1][i] = (uint8_t)(g >> 8);
			trueColor24[2][i] = (uint8_t)(b >> 8);
		} else if (rIndex == 0 && bIndex == 1 && gIndex == 2) {
			trueColor24[0][i] = (uint8_t)(r >> 8);
			trueColor24[1][i] = (uint8_t)(b >> 8);
			trueColor24[2][i] = (uint8_t)(g >> 8);
		} else if (gIndex == 0 && rIndex == 1 && bIndex == 2) {
			trueColor24[0][i] = (uint8_t)(g >> 8);
			trueColor24[1][i] = (uint8_t)(r >> 8);
			trueColor24[2][i] = (uint8_t)(b >> 8);
		} else if (gIndex == 0 && bIndex == 1 && rIndex == 2) {
			trueColor24[0][i] = (uint8_t)(g >> 8);
			trueColor24[1][i] = (uint8_t)(b >> 8);
			trueColor24[2][i] = (uint8_t)(r >> 8);
		} else if (bIndex == 0 && rIndex == 1 && gIndex == 2) {
			trueColor24[0][i] = (uint8_t)(b >> 8);
			trueColor24[1][i] = (uint8_t)(r >> 8);
			trueColor24[2][i] = (uint8_t)(g >> 8);
		} else if (bIndex == 0 && gIndex == 1 && rIndex == 2) {
			trueColor24[0][i] = (uint8_t)(b >> 8);
			trueColor24[1][i] = (uint8_t)(g >> 8);
			trueColor24[2][i] = (uint8_t)(r >> 8);
		}
		break;
	case 32:
		trueColor32[i] =
			(((uint32_t)(r) >> (16 - rLength)) << rOffset) |
			(((uint32_t)(g) >> (16 - gLength)) << gOffset) |
			(((uint32_t)(b) >> (16 - bLength)) << bOffset);
		break;
	default:
		break;
	}
}

bool palette_hasColorMap(void)
{
	return hasColorMap;
}

void palette_reset(void)
{
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
}

void palette_restore(void)
{
	if (colorMapSaved)
		framebuffer_setColorMap(&ocmap);
}

int palette_getRLength(void)
{
	return rLength;
}

int palette_getGLength(void)
{
	return gLength;
}

int palette_getBLength(void)
{
	return bLength;
}

int palette_getROffset(void)
{
	return rOffset;
}

int palette_getGOffset(void)
{
	return gOffset;
}

int palette_getBOffset(void)
{
	return bOffset;
}

int palette_getRIndex(void)
{
	return rIndex;
}

int palette_getGIndex(void)
{
	return gIndex;
}

int palette_getBIndex(void)
{
	return bIndex;
}

inline uint8_t palette_ansiToVGA(const int ansiColor)
{
	assert(ansiColor >= 0 && ansiColor <= 255);

	if (ansiColor >= 0 && ansiColor <= 15)
		return ansiToVGA[ansiColor];
	else
		return (uint8_t)ansiColor;
}

inline uint16_t palette_getTrueColor15(const uint8_t color)
{
	return trueColor16[color];
}

inline uint16_t palette_getTrueColor16(const uint8_t color)
{
	return trueColor16[color];
}

inline void palette_getTrueColor24(const uint8_t color,
				   uint8_t *c0, uint8_t *c1, uint8_t *c2)
{
	*c0 = trueColor24[0][color];
	*c1 = trueColor24[1][color];
	*c2 = trueColor24[2][color];
}

inline uint32_t palette_getTrueColor32(const uint8_t color)
{
	return trueColor32[color];
}

void palette_reverse(bool status)
{
	static bool reverse;
	int i;

	if (reverse == status)
		return;
	switch (bitsPerPixel) {
	case 4:
		/* FALLTHROUGH */
	case 8:
		for (i = 0; i < 256; i++) {
			nr[i] = ~nr[i];
			ng[i] = ~ng[i];
			nb[i] = ~nb[i];
		}
		break;
	case 15:
		/* FALLTHROUGH */
	case 16:
		for (i = 0; i < 256; i++)
			trueColor16[i] = ~trueColor16[i];
		break;
	case 24:
		for (i = 0; i < 256; i++) {
			trueColor24[rIndex][i] = ~trueColor24[rIndex][i];
			trueColor24[gIndex][i] = ~trueColor24[gIndex][i];
			trueColor24[bIndex][i] = ~trueColor24[bIndex][i];
		}
		break;
	case 32:
		for (i = 0; i < 256; i++)
			trueColor32[i] = ~trueColor32[i];
		break;
	default:
		break;
	}
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
	reverse = status;
}

bool palette_update(const int ansiColor, const char *value)
{
	uint16_t r, g, b;
	char *p, *v, *s;
	uint8_t i;
	int j;

	if (ansiColor < 0 || ansiColor > 255)
		return false;   /* illegal argument */
	if (value == NULL || strlen(value) == 0)
		return false;   /* illegal argument */
	i = palette_ansiToVGA(ansiColor);
	v = s = strdup(value);
	if (v == NULL)
		err(1, "strdup()");
	/* to lowercase */
	for (p = s; *p != '\0'; p++)
		*p = (char)tolower((int)*p);
	/* "gray" to "grey" */
	if ((p = strstr(s, "gray")) != NULL)
		p[2] = 'e';
	r = g = b = 0;
	if (strlen(s) >= 1 && strncmp(s, "#", 1) == 0) {
		s++;
		if (strlen(s) == 3) {
			/* #fff */
			r = hex2int(s[0]) << 12;
			g = hex2int(s[1]) << 12;
			b = hex2int(s[2]) << 12;
		} else if (strlen(s) == 6) {
			/* #ffffff */
			r = hex2int(s[0]) << 12 |
			    hex2int(s[1]) << 8;
			g = hex2int(s[2]) << 12 |
			    hex2int(s[3]) << 8;
			b = hex2int(s[4]) << 12 |
			    hex2int(s[5]) << 8;
		} else if (strlen(s) == 12) {
			/* #ffffffffffff */
			r = hex2int(s[0]) << 12 |
			    hex2int(s[1]) << 8 |
			    hex2int(s[2]) << 4 |
			    hex2int(s[3]);
			g = hex2int(s[4]) << 12 |
			    hex2int(s[5]) << 8 |
			    hex2int(s[6]) << 4 |
			    hex2int(s[7]);
			b = hex2int(s[8]) << 12 |
			    hex2int(s[9]) << 8 |
			    hex2int(s[10]) << 4 |
			    hex2int(s[11]);
		} else {
			free(v);
			return false; /* bad format */
		}
	} else if (strlen(s) >= 4 && strncmp(s, "rgb:", 4) == 0) {
		s += 4;
		if (strlen(s) >= 5) {
			int slash;
			char *pr, *pg, *pb;
			/* rgb:r/g/b */
			slash = 0;
			for (p = s; *p != '\0'; p++)
				if (*p == '/')
					slash++;
			if (slash != 2) {
				free(v);
				return false; /* bad format */
			}
			pr = pg = pb = NULL;
			for (p = strtok(s, "/"); p != NULL;
			     p = strtok(NULL, "/")) {
				if (strlen(p) < 1 || strlen(p) > 4) {
					free(v);
					return false; /* bad format */
				}
				if (pr == NULL) pr = p;
				else if (pg == NULL) pg = p;
				else if (pb == NULL) pb = p;
			}
			for (j = 0; j < 4; j++) {
				if (strlen(pr) >= j + 1)
					r = hex2int(pr[j]) | (r << 4);
				else
					r = (r << 4);
			}
			for (j = 0; j < 4; j++) {
				if (strlen(pg) >= j + 1)
					g = hex2int(pg[j]) | (g << 4);
				else
					g = (g << 4);
			}
			for (j = 0; j < 4; j++) {
				if (strlen(pb) >= j + 1)
					b = hex2int(pb[j]) | (b << 4);
				else
					b = (b << 4);
			}
		} else {
			free(v);
			return false; /* bad format */
		}
	} else {
		/* TODO: parse rgb.txt */
		bool found;
		found = false;
		for (j = 0; namedColor[j].name != NULL; j++) {
			if (strcmp(s, namedColor[j].name) == 0) {
				r = namedColor[j].r;
				g = namedColor[j].g;
				b = namedColor[j].b;
				found = true;
				break;
			}
		}
		if (!found) {
			free(v);
			return false; /* not found */
		}
	}
	setColor(i, r, g, b);
	if (hasColorMap)
		framebuffer_setColorMap(&ncmap);
	free(v);
	return true;
}

#if defined (__NetBSD__)
/*
 * vesafb(4) ioctl WSDISPLAYIO_GETCMAP returns wrong values.
 * http://www.netbsd.org/cgi-bin/query-pr-single.pl?number=35030
 */

static uint32_t cmap0to15[16] = {
	0x000000, 0x7f0000, 0x007f00, 0x7f7f00,
	0x00007f, 0x7f007f, 0x007f7f, 0xc7c7c7,
	0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00,
	0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
};

static uint32_t cmap240to255[16] = {
	0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00,
	0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
	0x000000, 0x7f0000, 0x007f00, 0x7f7f00,
	0x00007f, 0x7f007f, 0x007f7f, 0xc7c7c7
};

static void setOldColorMap(void)
{
	int i;

	memset(or, 0xff, sizeof(or));
	memset(og, 0xff, sizeof(og));
	memset(ob, 0xff, sizeof(ob));
	for (i = 0; i < 16; i++) {
		or[i] = (u_char)((cmap0to15[i] & 0xff0000) >> 16);
		og[i] = (u_char)((cmap0to15[i] & 0x00ff00) >> 8);
		ob[i] = (u_char)((cmap0to15[i] & 0x0000ff));
	}
	for (i = 0; i < 16; i++) {
		or[240 + i] = (u_char)((cmap240to255[i] & 0xff0000) >> 16);
		og[240 + i] = (u_char)((cmap240to255[i] & 0x00ff00) >> 8);
		ob[240 + i] = (u_char)((cmap240to255[i] & 0x0000ff));
	}
}
#endif

