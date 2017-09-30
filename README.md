# seedsm

[![CircleCI](https://circleci.com/gh/maueki/seedsm.svg?style=shield)](https://circleci.com/gh/maueki/seedsm)

## About

Seedsm(Simple and Easy Event Driven State Machine) is a C++ header only state machine library based on [libev](http://software.schmorp.de/pkg/libev.html).

## Requirements

* [libev](http://software.schmorp.de/pkg/libev.html)
* C++11 or higher

## Usage

TODO

see `example`

## Test

### Requirements

* [googletest](https://github.com/google/googletest)

### Run

```bash
$ cd test
$ mkdir build && cd build
$ cmake ..
$ make
$ ./unit_test
```

## License

MIT

## TODO

- [x] parallel state
- [x] high priority event
- [ ] history
