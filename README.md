# Oboe-vita

PS Vita port for [google/oboe](https://github.com/google/oboe)

## Note

Please note that this is just a shabby (with tons of dirty hack) and poorly-tested port. 
It is motivated by my port of the Android game Kera Blaster and is the only use case so 
far. So use it with your own judgment.

## Installation

See `./package`

## Build

`cmake . && make`

If you want logging to be enabled, add compiler option `-DOBOE_LOGGING=1`. And you should
implement your `__android_log_print`.

## License
[LICENSE](LICENSE)

