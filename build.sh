#!/bin/bash

# Clean old binaries
rm -f bin/interrupts_EP
rm -f bin/interrupts_RR
rm -f bin/interrupts_EP_RR

# Compile all three schedulers
clang++ -std=c++17 interrupts_EP_101268686_101311227.cpp -o bin/interrupts_EP
clang++ -std=c++17 interrupts_RR_101268686_101311227.cpp -o bin/interrupts_RR
clang++ -std=c++17 interrupts_EP_RR_101268686_101311227.cpp -o bin/interrupts_EP_RR

echo "Build complete!"
ls -l bin

