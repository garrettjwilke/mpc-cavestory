## mpc cavestory

cave story (doukutsu-rs) for the akai mpc sample.

### requirements

* rust
* zig


### build

get deps and submodules:

```
git submodule update --init
```

put your cave story `data/` folder inside `gamedata/data/` and run:

```
make
```

### install

`make` writes `build/autorun/` and `build/mpc-cavestory.zip`.

copy `build/autorun/` onto the SD card, or add the zip with [mpc-boot-thingy](https://github.com/home-less/mpc-boot-thingy):

```
../mpc-boot-thingy/setup.sh --SD "/path/to/card" --add build/mpc-cavestory.zip
```

`make deploy` does that over SSH.

### controls

they are super jank....press the top buttons to bring up menus to change your binds...it's an mpc, so controls will just be odd no matter what

