# Arena
ARENA implementation for POSIX compliant systems (windows todo) \
Similar to the Arena API Ryan Fleury discussed in [his blog](https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator) (check it out btw, it's awesome) \
Differences:
- ...Zero() functions were adapted to ...ZeroInit(), for clarity
- added PopArray and PopStruct for convenience
- added basic non-thread safe scratch arena
- added some sort of page size allocation thingy

Usage notes:
- likely very SCUFFED in a million different and creative ways, use at your own risk and mental sanity ig?
- uses the same conventions as the malloc API, aka the functions return a NULL pointer when they fail
- NO THREAD-SAFETY is implemented, neither in the main arena API nor in the scratch arena API
- I added some ugly docs so enjoy IDE suggestions and stuff :)

# Str
oh you thought the Arena code was ugly? \
I welcome you to the str.h, even uglier, incomplete, and unsafe! \\

Imma implement those helper functions at some point, but I wouldn't wait for them

# License
I couldn't care less, do whatever you want with it