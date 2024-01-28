# Documentation on the Linux Kernel Programming Project


## Rotating file system
Goal of exercise:
- [ ] Automatically delete files whenever free space becomes critical
- [ ] Evict based on *least-recently used*
- [ ] Should be used if
    - [ ] Less the $x$% of blocks remain free
    - [ ] If a directory is full, delete least-recently used files. If it contains only directories, return failure.
- [ ] Mechanism must be trigged automatically
- [ ] Only delete files not (currently) in use

## Modular eviction policies
- [ ] Implement different eviction policy
    - [ ] Delete largest files first
- [ ] Policies can be added or changed while the module is loaded
- [ ] Provide API for eviction policies
- [ ] Enable policies to be located in a different module and registered

## Manual rotation
- [ ] Add a way for users to trigger eviction
- [ ] Choose communication API provided by kernel