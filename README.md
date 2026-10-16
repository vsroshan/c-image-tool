C Image Tool

A command-line image processing toolkit written in C that manipulates BMP images at the pixel level.

This project explores fundamental systems programming concepts, including pointers, structures, dynamic memory allocation, file I/O, and direct manipulation of image data.

Features

* Grayscale: Convert a color image to grayscale.
* Invert: Invert the red, green, and blue color channels.
* Reflect: Flip an image horizontally.
* Blur: Apply a simple 3×3 box blur.

The application reads an image, processes its pixels in memory, and saves the result as a new BMP file.

Supported Formats

This version supports:

* Uncompressed 24-bit BMP images.
* Uncompressed 32-bit BMP images using the BI_RGB format.

JPEG, PNG, compressed BMP files, and other image formats are not supported.

Requirements

* macOS, Linux, or another compatible Unix-like environment.
* Clang or another compatible C compiler.
* GNU Make or a compatible make utility.
* Git for cloning the repository.

No external image-processing libraries are required.

Installation

1. Clone the repository
git clone https://github.com/vsroshan/c-image-tool.git
cd c-image-tool

2. Compile the program

make

This compiles the C source code and creates the image-tool executable.

3. Check the available commands

./image-tool

The program displays its usage instructions when no arguments are provided.

Usage

The general command format is:

./image-tool <image.bmp> <operation>

Convert to grayscale

./image-tool photo.bmp grayscale

Output:

photo-grayscale.bmp

Invert colors

./image-tool photo.bmp invert

Output:

photo-invert.bmp

Reflect horizontally

./image-tool photo.bmp reflect

Output:

photo-reflect.bmp

Apply blur

./image-tool photo.bmp blur

Output:

photo-blur.bmp

The original image is preserved. Each operation produces a separate output file.

How It Works

The image-processing pipeline follows these steps:

Input BMP Image
       |
       v
Read File Header
       |
       v
Validate Image Format
       |
       v
Allocate Memory for Pixels
       |
       v
Read Pixel Data
       |
       v
Apply Image Transformation
       |
       v
Write New BMP Header
       |
       v
Write Processed Pixel Data
       |
       v
Output BMP Image

1. Read the BMP header

The program reads the BMP header to determine the image dimensions, pixel format, compression method, and location of the pixel data.

2. Allocate memory

A dynamically allocated array stores the image’s pixels.

Each pixel is represented by a C structure containing blue, green, red, and alpha fields.

3. Manipulate pixels

The program traverses the pixel array and modifies the color channels according to the selected operation.

4. Write the output

The program constructs a BMP header, writes the transformed pixel data, and saves the result as a new file.

Concepts Demonstrated

Pointers

Pointers provide access to image structures and dynamically allocated pixel arrays.

Structures

The Pixel and Image structures organize related image data.

Dynamic Memory Allocation

malloc() and calloc() allocate memory for pixel data and temporary buffers. free() releases allocated memory when it is no longer needed.

File I/O

fopen(), fread(), fwrite(), fseek(), and fclose() handle binary file operations.

Arrays and Memory Access

The program uses array indexing and pointer access to read and modify individual pixels.

Integer Representation

The BMP header contains little-endian integer fields. The program explicitly reads and writes these fields rather than relying on the computer’s native structure layout.

Image Representation

The project demonstrates how an image can be represented as a two-dimensional grid of pixels stored in a one-dimensional array.

Memory Management

The application checks memory allocation results and releases its dynamically allocated buffers.

Compiler Warnings

The Makefile enables several compiler warnings to help identify potential mistakes during development.

Design Decisions

* No external image libraries: BMP parsing and pixel manipulation are implemented directly in C.
* Separate image operations: Each transformation is implemented in its own function.
* Preserve the original: The program writes a new file instead of overwriting the input.
* Handle BMP row padding: BMP rows are aligned to four-byte boundaries.
* Normalize image orientation: Pixel data is stored in top-to-bottom order in memory.
* Validate supported formats: The program rejects unsupported bit depths and compressed BMP files.
* Release allocated memory: Dynamically allocated buffers are freed when they are no longer needed.

Limitations

* Only uncompressed 24-bit and 32-bit BMP images are supported.
* The implementation expects a standard Windows BMP information header of at least 40 bytes.
* The blur operation uses a basic 3×3 box filter.
* The program is designed as a learning project, not a hardened image-processing library for untrusted files.
* Additional validation and testing would be needed before using it with arbitrary or potentially malicious image files.

Future Improvements

Potential extensions include:

* Add brightness and contrast controls.
* Implement edge detection.
* Add image rotation.
* Add support for additional BMP variants.
* Introduce unit tests for pixel transformations.
* Add benchmarks for large images.
* Improve malformed-file validation.
* Explore memory sanitizers and debugging tools.

Learning Objectives

The goal of this project is to move beyond learning individual C language features and apply them to a practical systems programming task.

By building this tool, I explored how binary file formats represent images, how pixel data is stored in memory, and how pointers and dynamic memory allocation enable direct manipulation of image data.

License

This project is available under the MIT License. Add a LICENSE file containing the standard MIT License text before publishing it under that license.
