#ifndef UTIL_H
#define UTIL_H

location_t location_of (tree t);
static inline bool loc_is_unknown(location_t loc) {
  location_t unk = UNKNOWN_LOCATION;
  return !memcmp(&loc, &unk, sizeof(location_t));
}
char const * loc_as_string (location_t loc);

#endif
