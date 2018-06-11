#include "udMath.h"

struct udGeoZone
{
  udDouble2 latLongBoundMin;
  udDouble2 latLongBoundMax;
  double meridian;
  double flattening;
  double semiMajorAxis;
  double semiMinorAxis;
  double thirdFlattening;
  double eccentricity;
  double secEccentricitySq;
  double scaleFactor;
  double meridonialParams[5];
  double phiParams[4];
  int32_t falseNorthing;
  int32_t falseEasting;
  int32_t zone;
  char hemisphere;
  char padding[3];
};

// Find an appropriate SRID code for a given lat/long (for example as a default value)
udResult udGeoZone_FindSRID(int32_t *pSRIDCode, const udDouble2 &latLong);

// Set the zone structure parameters from a given srid code
udResult udGeoZone_SetFromSRID(udGeoZone *pZone, int32_t sridCode);

// Convert a point from lat/long to the cartesian coordinate system defined by the zone
udDouble3 udGeoZone_ToCartesian(const udGeoZone &zone, const udDouble3 &latLong);

// Convert a point from the cartesian coordinate system defined by the zone to lat/long
udDouble3 udGeoZone_ToLatLong(const udGeoZone &zone, const udDouble3 &position);
