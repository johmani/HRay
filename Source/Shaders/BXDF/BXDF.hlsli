#ifndef BXDF_H
#define BXDF_H

#if defined(DISNEY_BRDF)
#   include "Disney.hlsli"
#else
#   include "Lambert.hlsli"
#endif

#endif // BXDF_H