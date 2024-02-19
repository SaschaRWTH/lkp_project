# Documentation on the Linux Kernel Programming Project


## Rotating file system
- [x] Automatically delete files whenever free space becomes critical
- [x] Evict based on *least-recently used*
- Should be used if
    - [x] Less the $x$% of blocks remain free
    - [x] If a directory is full, delete least-recently used files. If it contains only directories, return failure.
- [x] Mechanism must be trigged automatically
- [ ] Only delete files not (currently) in use

## Modular eviction policies
- [x] Implement different eviction policies
    - [x] Delete least-recently used file
    - [x] Delete largest file
- [x] Policies can be added or changed while the module is loaded
- [x] Provide API for eviction policies
- [x] Enable policies to be located in a different module and registered

## Manual rotation
- [x] Add a way for users to trigger eviction
- [x] Choose communication API provided by kernel

## Next steps
- [ ] Change parent policy to ignore dirty nodes