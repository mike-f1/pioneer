#ifndef FRAMEID_H_INCLUDED
#define FRAMEID_H_INCLUDED

typedef int FrameId;

constexpr FrameId noFrameId = -1;

constexpr FrameId rootFrameId = 0;

inline bool IsIdValid(FrameId fId) { return fId >= 0; };

#endif // FRAMEID_H_INCLUDED
