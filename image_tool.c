#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

/*
 * c-image-tool
 *
 * A command-line BMP image processing toolkit.
 *
 * Supported formats:
 * - Uncompressed 24-bit BMP
 * - Uncompressed 32-bit BMP
 *
 * Operations:
 * - grayscale
 * - invert
 * - reflect
 * - blur
 */

#define BMP_HEADER_SIZE 54

typedef struct
{
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
} Pixel;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint16_t bits_per_pixel;
    size_t row_stride;
    size_t pixel_count;
    Pixel *pixels;
} Image;


/* Read a 16-bit little-endian integer. */
static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t) data[0]
        | ((uint16_t) data[1] << 8);
}


/* Read a 32-bit little-endian integer. */
static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t) data[0]
        | ((uint32_t) data[1] << 8)
        | ((uint32_t) data[2] << 16)
        | ((uint32_t) data[3] << 24);
}


/* Write a 16-bit little-endian integer. */
static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
}


/* Write a 32-bit little-endian integer. */
static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}


/*
 * Free the dynamically allocated pixel array
 * and reset the Image structure.
 */
static void free_image(Image *image)
{
    if (image == NULL)
    {
        return;
    }

    free(image->pixels);

    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
    image->bits_per_pixel = 0;
    image->row_stride = 0;
    image->pixel_count = 0;
}


/*
 * Load an uncompressed 24-bit or 32-bit BMP.
 *
 * The BMP header is read separately from the pixel data.
 * Pixels are stored in memory from top to bottom,
 * regardless of their original BMP orientation.
 */
static int load_bmp(const char *filename, Image *image)
{
    FILE *file = NULL;
    uint8_t header[BMP_HEADER_SIZE];

    uint32_t pixel_offset;
    uint32_t dib_header_size;
    uint32_t compression;

    int32_t signed_width;
    int32_t signed_height;

    uint64_t row_bits;
    uint64_t stride64;
    uint64_t pixel_count64;

    size_t row;
    size_t column;

    int result = 0;

    if (filename == NULL || image == NULL)
    {
        fprintf(stderr, "Error: Invalid input.\n");
        return 0;
    }

    memset(image, 0, sizeof(*image));

    file = fopen(filename, "rb");

    if (file == NULL)
    {
        perror("Error opening input file");
        return 0;
    }

    if (fread(header, 1, BMP_HEADER_SIZE, file)
        != BMP_HEADER_SIZE)
    {
        fprintf(stderr, "Error: Could not read BMP header.\n");
        goto cleanup;
    }

    /* Check the BMP signature: 'B' followed by 'M'. */
    if (header[0] != 'B' || header[1] != 'M')
    {
        fprintf(stderr, "Error: Input is not a BMP file.\n");
        goto cleanup;
    }

    pixel_offset = read_u32(&header[10]);
    dib_header_size = read_u32(&header[14]);

    signed_width = (int32_t) read_u32(&header[18]);
    signed_height = (int32_t) read_u32(&header[22]);

    image->bits_per_pixel = read_u16(&header[28]);
    compression = read_u32(&header[30]);

    /*
     * This implementation expects the standard
     * 40-byte BITMAPINFOHEADER or a larger DIB header.
     */
    if (dib_header_size < 40)
    {
        fprintf(stderr, "Error: Unsupported BMP header.\n");
        goto cleanup;
    }

    if (signed_width <= 0 || signed_height == 0
        || signed_height == INT32_MIN)
    {
        fprintf(stderr, "Error: Invalid image dimensions.\n");
        goto cleanup;
    }

    if (image->bits_per_pixel != 24
        && image->bits_per_pixel != 32)
    {
        fprintf(stderr,
                "Error: Only 24-bit and 32-bit BMP files are supported.\n");
        goto cleanup;
    }

    /*
     * Compression value 0 means BI_RGB.
     * Compressed BMP formats are not supported.
     */
    if (compression != 0)
    {
        fprintf(stderr,
                "Error: Compressed BMP files are not supported.\n");
        goto cleanup;
    }

    image->width = (uint32_t) signed_width;

    /*
     * A negative BMP height indicates a top-down image.
     * A positive height indicates a bottom-up image.
     */
    image->height = (uint32_t)
        (signed_height < 0 ? -signed_height : signed_height);

    /*
     * BMP rows are padded to a multiple of four bytes.
     *
     * Calculate the row stride using 64-bit arithmetic
     * to reduce the risk of integer overflow.
     */
    row_bits = (uint64_t) image->width
        * image->bits_per_pixel;

    stride64 = ((row_bits + 31) / 32) * 4;

    pixel_count64 = (uint64_t) image->width
        * image->height;

    if (stride64 > SIZE_MAX
        || pixel_count64 > SIZE_MAX / sizeof(Pixel))
    {
        fprintf(stderr, "Error: Image is too large.\n");
        goto cleanup;
    }

    if ((uint64_t) pixel_offset
        < (uint64_t) 14 + dib_header_size)
    {
        fprintf(stderr, "Error: Invalid pixel-data offset.\n");
        goto cleanup;
    }

    image->row_stride = (size_t) stride64;
    image->pixel_count = (size_t) pixel_count64;

    /*
     * Allocate memory dynamically for every pixel.
     */
    image->pixels = calloc(
        image->pixel_count,
        sizeof(Pixel)
    );

    if (image->pixels == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        goto cleanup;
    }

    /*
     * Move to the beginning of the pixel data.
     */
    if (fseek(file, (long) pixel_offset, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: Could not locate pixel data.\n");
        goto cleanup;
    }

    {
        uint8_t *row_buffer = malloc(image->row_stride);

        if (row_buffer == NULL)
        {
            fprintf(stderr, "Error: Row allocation failed.\n");
            goto cleanup;
        }

        for (row = 0; row < image->height; row++)
        {
            size_t destination_row;

            if (fread(row_buffer, 1, image->row_stride, file)
                != image->row_stride)
            {
                fprintf(stderr,
                        "Error: Unexpected end of pixel data.\n");

                free(row_buffer);
                goto cleanup;
            }

            /*
             * Convert BMP row order into top-to-bottom
             * memory order.
             */
            if (signed_height > 0)
            {
                destination_row =
                    image->height - 1 - row;
            }
            else
            {
                destination_row = row;
            }

            for (column = 0;
                 column < image->width;
                 column++)
            {
                size_t source_index =
                    column * (image->bits_per_pixel / 8);

                size_t destination_index =
                    destination_row * image->width + column;

                Pixel *pixel =
                    &image->pixels[destination_index];

                pixel->blue = row_buffer[source_index];
                pixel->green = row_buffer[source_index + 1];
                pixel->red = row_buffer[source_index + 2];

                /*
                 * 24-bit BMP has no alpha channel.
                 * 32-bit BI_RGB alpha is not guaranteed
                 * to contain meaningful transparency.
                 */
                pixel->alpha =
                    image->bits_per_pixel == 32
                    ? row_buffer[source_index + 3]
                    : 255;
            }
        }

        free(row_buffer);
    }

    result = 1;

cleanup:

    if (file != NULL)
    {
        fclose(file);
    }

    if (!result)
    {
        free_image(image);
    }

    return result;
}


/*
 * Write the processed pixels to a new BMP file.
 */
static int save_bmp(const char *filename, const Image *image)
{
    FILE *file = NULL;

    uint8_t header[BMP_HEADER_SIZE] = {0};

    uint64_t row_bits;
    uint64_t stride64;
    uint64_t image_size64;
    uint64_t file_size64;

    size_t row;
    size_t column;

    int result = 0;

    if (filename == NULL || image == NULL
        || image->pixels == NULL)
    {
        fprintf(stderr, "Error: Invalid output image.\n");
        return 0;
    }

    row_bits = (uint64_t) image->width
        * image->bits_per_pixel;

    stride64 = ((row_bits + 31) / 32) * 4;

    image_size64 = stride64 * image->height;
    file_size64 = BMP_HEADER_SIZE + image_size64;

    /*
     * BMP file size and image size fields are 32-bit.
     */
    if (stride64 > SIZE_MAX
        || image_size64 > UINT32_MAX
        || file_size64 > UINT32_MAX
        || image->pixel_count > SIZE_MAX / sizeof(Pixel))
    {
        fprintf(stderr, "Error: Output image is too large.\n");
        return 0;
    }

    /*
     * Construct the BMP file header.
     */
    header[0] = 'B';
    header[1] = 'M';

    write_u32(&header[2], (uint32_t) file_size64);
    write_u32(&header[10], BMP_HEADER_SIZE);

    write_u32(&header[14], 40);
    write_u32(&header[18], image->width);
    write_u32(&header[22], image->height);

    write_u16(&header[26], 1);
    write_u16(&header[28], image->bits_per_pixel);

    write_u32(&header[30], 0);
    write_u32(&header[34], (uint32_t) image_size64);

    file = fopen(filename, "wb");

    if (file == NULL)
    {
        perror("Error creating output file");
        return 0;
    }

    if (fwrite(header, 1, BMP_HEADER_SIZE, file)
        != BMP_HEADER_SIZE)
    {
        fprintf(stderr, "Error writing BMP header.\n");
        goto cleanup;
    }

    {
        uint8_t *row_buffer = calloc(
            (size_t) stride64,
            sizeof(uint8_t)
        );

        if (row_buffer == NULL)
        {
            fprintf(stderr, "Error allocating output row.\n");
            goto cleanup;
        }

        /*
         * Write BMP rows from bottom to top.
         */
        for (row = 0; row < image->height; row++)
        {
            size_t source_row = image->height - 1 - row;

            memset(row_buffer, 0, (size_t) stride64);

            for (column = 0;
                 column < image->width;
                 column++)
            {
                size_t source_index =
                    source_row * image->width + column;

                size_t destination_index =
                    column * (image->bits_per_pixel / 8);

                const Pixel *pixel =
                    &image->pixels[source_index];

                row_buffer[destination_index] = pixel->blue;
                row_buffer[destination_index + 1] = pixel->green;
                row_buffer[destination_index + 2] = pixel->red;

                if (image->bits_per_pixel == 32)
                {
                    row_buffer[destination_index + 3] =
                        pixel->alpha;
                }
            }

            if (fwrite(row_buffer, 1, (size_t) stride64, file)
                != (size_t) stride64)
            {
                fprintf(stderr, "Error writing pixel data.\n");
                free(row_buffer);
                goto cleanup;
            }
        }

        free(row_buffer);
    }

    result = 1;

cleanup:

    if (fclose(file) != 0)
    {
        fprintf(stderr, "Error closing output file.\n");
        result = 0;
    }

    return result;
}


/*
 * Convert each pixel into grayscale.
 *
 * A weighted formula approximates human brightness
 * perception more accurately than a simple average.
 */
static void grayscale(Image *image)
{
    size_t i;

    for (i = 0; i < image->pixel_count; i++)
    {
        Pixel *pixel = &image->pixels[i];

        unsigned int gray =
            (299u * pixel->red
            + 587u * pixel->green
            + 114u * pixel->blue
            + 500u) / 1000u;

        pixel->red = (uint8_t) gray;
        pixel->green = (uint8_t) gray;
        pixel->blue = (uint8_t) gray;
    }
}


/*
 * Invert every color channel.
 *
 * 0 becomes 255.
 * 255 becomes 0.
 */
static void invert(Image *image)
{
    size_t i;

    for (i = 0; i < image->pixel_count; i++)
    {
        Pixel *pixel = &image->pixels[i];

        pixel->red = 255 - pixel->red;
        pixel->green = 255 - pixel->green;
        pixel->blue = 255 - pixel->blue;
    }
}


/*
 * Reflect the image horizontally.
 *
 * Swap the left and right pixels in each row.
 */
static void reflect(Image *image)
{
    size_t row;
    size_t column;

    for (row = 0; row < image->height; row++)
    {
        for (column = 0;
             column < image->width / 2;
             column++)
        {
            size_t left_index =
                row * image->width + column;

            size_t right_index =
                row * image->width
                + (image->width - 1 - column);

            Pixel temporary = image->pixels[left_index];

            image->pixels[left_index] =
                image->pixels[right_index];

            image->pixels[right_index] = temporary;
        }
    }
}


/*
 * Apply a simple 3-by-3 box blur.
 *
 * Each output pixel receives the average color
 * of its neighboring pixels.
 *
 * A separate array preserves the original pixels
 * while the blurred image is being calculated.
 */
static int blur(Image *image)
{
    Pixel *original;

    size_t row;
    size_t column;

    original = malloc(
        image->pixel_count * sizeof(Pixel)
    );

    if (original == NULL)
    {
        fprintf(stderr, "Error allocating blur buffer.\n");
        return 0;
    }

    memcpy(
        original,
        image->pixels,
        image->pixel_count * sizeof(Pixel)
    );

    for (row = 0; row < image->height; row++)
    {
        for (column = 0;
             column < image->width;
             column++)
        {
            unsigned int red_sum = 0;
            unsigned int green_sum = 0;
            unsigned int blue_sum = 0;

            unsigned int count = 0;

            int dy;
            int dx;

            for (dy = -1; dy <= 1; dy++)
            {
                for (dx = -1; dx <= 1; dx++)
                {
                    int neighbor_row = (int) row + dy;
                    int neighbor_column = (int) column + dx;

                    size_t neighbor_index;
                    const Pixel *neighbor;

                    if (neighbor_row < 0
                        || neighbor_column < 0
                        || (uint64_t) neighbor_row >= image->height
                        || (uint64_t) neighbor_column >= image->width)
                    {
                        continue;
                    }

                    neighbor_index =
                        (size_t) neighbor_row * image->width
                        + (size_t) neighbor_column;

                    neighbor = &original[neighbor_index];

                    red_sum += neighbor->red;
                    green_sum += neighbor->green;
                    blue_sum += neighbor->blue;

                    count++;
                }
            }

            {
                size_t index =
                    row * image->width + column;

                image->pixels[index].red =
                    (uint8_t) (red_sum / count);

                image->pixels[index].green =
                    (uint8_t) (green_sum / count);

                image->pixels[index].blue =
                    (uint8_t) (blue_sum / count);
            }
        }
    }

    free(original);

    return 1;
}


/*
 * Build the output filename.
 *
 * Example:
 * photo.bmp + grayscale -> photo-grayscale.bmp
 */
static char *make_output_filename(
    const char *input,
    const char *operation
)
{
    const char *extension = strrchr(input, '.');

    size_t base_length = strlen(input);
    size_t operation_length = strlen(operation);

    size_t total_length;
    char *output;

    if (extension != NULL
        && strcmp(extension, ".bmp") == 0)
    {
        base_length = (size_t) (extension - input);
    }

    if (base_length > SIZE_MAX - operation_length - 6)
    {
        fprintf(stderr, "Error: Output filename is too long.\n");
        return NULL;
    }

    total_length = base_length + operation_length + 6;

    output = malloc(total_length);

    if (output == NULL)
    {
        fprintf(stderr, "Error allocating output filename.\n");
        return NULL;
    }

    snprintf(
        output,
        total_length,
        "%.*s-%s.bmp",
        (int) base_length,
        input,
        operation
    );

    return output;
}


/* Print usage instructions. */
static void print_usage(const char *program)
{
    printf("\n");
    printf("C Image Tool\n");
    printf("============\n\n");

    printf("Usage:\n");
    printf("  %s <image.bmp> <operation>\n\n", program);

    printf("Operations:\n");
    printf("  grayscale   Convert the image to grayscale\n");
    printf("  invert      Invert the image colors\n");
    printf("  reflect     Flip the image horizontally\n");
    printf("  blur        Apply a 3x3 box blur\n\n");

    printf("Example:\n");
    printf("  %s photo.bmp grayscale\n\n", program);
}


/* Program entry point. */
int main(int argc, char *argv[])
{
    Image image = {0};

    char *output_filename = NULL;

    const char *input_filename;
    const char *operation;

    int success = 0;

    if (argc != 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    input_filename = argv[1];
    operation = argv[2];

    if (strcmp(operation, "grayscale") != 0
        && strcmp(operation, "invert") != 0
        && strcmp(operation, "reflect") != 0
        && strcmp(operation, "blur") != 0)
    {
        fprintf(stderr, "Error: Unknown operation '%s'.\n",
                operation);

        print_usage(argv[0]);
        return 1;
    }

    printf("Loading image: %s\n", input_filename);

    if (!load_bmp(input_filename, &image))
    {
        return 1;
    }

    printf("Image dimensions: %u x %u\n",
           image.width, image.height);

    printf("Bits per pixel: %u\n",
           image.bits_per_pixel);

    if (strcmp(operation, "grayscale") == 0)
    {
        grayscale(&image);
    }
    else if (strcmp(operation, "invert") == 0)
    {
        invert(&image);
    }
    else if (strcmp(operation, "reflect") == 0)
    {
        reflect(&image);
    }
    else if (strcmp(operation, "blur") == 0)
    {
        if (!blur(&image))
        {
            free_image(&image);
            return 1;
        }
    }

    output_filename =
        make_output_filename(input_filename, operation);

    if (output_filename == NULL)
    {
        free_image(&image);
        return 1;
    }

    printf("Applying operation: %s\n", operation);

    if (!save_bmp(output_filename, &image))
    {
        fprintf(stderr, "Failed to save processed image.\n");
        goto cleanup;
    }

    printf("\nImage processed successfully!\n");
    printf("Output: %s\n", output_filename);

    success = 1;

cleanup:

    free(output_filename);
    free_image(&image);

    return success ? 0 : 1;
}
