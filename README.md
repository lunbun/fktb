# fktb
Chess engine made from scratch

Play me on Lichess! https://lichess.org/@/frogskittens_thebot

## Building

A prebuilt Windows binary is available in the
[Releases](https://github.com/lunbun/fktb/releases/tag/v0.0.76).

GCC or Clang is recommended. I have not tested MSVC; proceed at your own risk.

No external dependencies are required.

Clone the repository:
```bash
git clone https://github.com/lunbun/fktb.git
```

Build with CMake:
```bash
cd fktb
mkdir build
cd build
cmake ..
make
```

## Usage

fktb is a
[Universal Chess Interface (UCI)](https://en.wikipedia.org/wiki/Universal_Chess_Interface)
chess engine.

You will need a UCI-compatible GUI, like [Nibbler](https://github.com/rooklift/nibbler).

If you know what you are doing, you can also interact with it from the
command-line using
[standard UCI commands](https://official-stockfish.github.io/docs/stockfish-wiki/UCI-&-Commands.html).

## NNUE

fktb has an experimental
[NNUE (Efficiently Updatable Neural Network) branch](https://github.com/lunbun/fktb/tree/nnue).
The neural network was trained using Pytorch with data from Lichess.
Training code is in [this repository](https://github.com/lunbun/fktb_nnue).
