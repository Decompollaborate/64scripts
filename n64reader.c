#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <getopt.h>
#include <iconv.h>

#define N64_HEADER_SIZE 0x40

typedef struct {
    char ch;
    char* description;
} CharDescription;

CharDescription mediaCharDescription[] = {
    { 'N', "cartridge" },
    { 'D', "64DD disk" },
    { 'C', "cartridge part of expandable game OR GameCube" },
    { 'E', "64DD expansion for cart" },
    { 'Z', "Aleck64 cartridge" },
    { 0 },
};

CharDescription countryCharDescription[] = {
    { '7', "Beta" },
    { 'A', "Asian (NTSC)" },
    { 'B', "Brazilian" },
    { 'C', "Chinese" },
    { 'D', "German" },
    { 'E', "North America" },
    { 'F', "French" },
    { 'G', "Gateway 64 (NTSC)" },
    { 'H', "Dutch" },
    { 'I', "Italian" },
    { 'J', "Japanese" },
    { 'K', "Korean" },
    { 'L', "Gateway 64 (PAL)" },
    { 'N', "Canadian" },
    { 'P', "European (basic spec.)" },
    { 'S', "Spanish" },
    { 'U', "Australian" },
    { 'W', "Scandinavian" },
    { 'X', "European" },
    { 'Y', "European" },
    { 0 },
};

/* Example header: (OoT debug) */
// .byte  0x80, 0x37, 0x12, 0x40   /* PI BSD Domain 1 register */
// .word  0x0000000F               /* Clockrate setting */
// .word  0x80000400               /* Entrypoint function (`entrypoint`) */
// .word  0x0000144C               /* Revision */
// .word  0x917D18F6               /* Checksum 1 */
// .word  0x69BC5453               /* Checksum 2 */
// .word  0x00000000               /* Unknown */
// .word  0x00000000               /* Unknown */
// .ascii "THE LEGEND OF ZELDA "   /* Internal ROM name */
// .word  0x00000000               /* Unknown */
// .word  0x0000004E               /* Cartridge */
// .ascii "ZL"                     /* Cartridge ID */
// .ascii "P"                      /* Region */
// .byte  0x0F                     /* Version */

typedef struct {
    /* 0x00 */ uint8_t PIBSDDomain1Register[4];
    /* 0x04 */ uint32_t clockRate;
    /* 0x08 */ uint32_t entrypoint;
    /* 0x0C */ uint32_t revision; /* Bottom byte is libultra version */
    /* 0x10 */ uint32_t checksum1;
    /* 0x14 */ uint32_t checksum2;
    /* 0x18 */ char unk_18[8];
    /* 0x20 */ char imageName[20]; /* Internal ROM name */
    /* 0x34 */ char unk_34[4];
    /* 0x38 */ uint32_t mediaFormat;
    /* 0x3C */ char cartridgeId[2];
    /* 0x3E */ char countryCode;
    /* 0x3F */ char version;
} N64Header;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define REEND32(w) ((w & 0xFF) << 0x18 | (w & 0xFF00) << 0x8 | (w & 0xFF0000) >> 0x8 | (w >> 0x18))
#else
#define REEND32(w) (w)
#endif

void SwapBytes16(uint16_t* data, int length) {
    int i;
    for (i = 0; i < length / 2; i++) {
        data[i] = ((data[i] & 0xFF) << 0x8) | (data[i] >> 0x8);
    }
}

void SwapBytes32(uint32_t* data, int length) {
    int i;
    for (i = 0; i < length / 4; i++) {
        data[i] = ((data[i] & 0xFF) << 0x18) | ((data[i] & 0xFF00) << 0x8) | ((data[i] & 0xFF0000) >> 0x8) |
                  (data[i] >> 0x18);
    }
}

void ReEndHeader(N64Header* header) {
    header->clockRate = REEND32(header->clockRate);
    header->entrypoint = REEND32(header->entrypoint);
    header->revision = REEND32(header->revision);
    header->checksum1 = REEND32(header->checksum1);
    header->checksum2 = REEND32(header->checksum2);
    header->mediaFormat = REEND32(header->mediaFormat);
}

char* FindDescriptionFromChar(char ch, CharDescription* charDescription) {
    while (charDescription->ch != '\0') {
        if (ch == charDescription->ch) {
            return charDescription->description;
        }
        charDescription++;
    }
    return NULL;
}

struct option longOptions[] = {
    { "csv", no_argument, NULL, 'c' },
    { "little-endian", no_argument, NULL, 'n' },
    { "print-endian", no_argument, NULL, 'p' },
    { "separator", required_argument, NULL, 's' },
    { "utf-8", no_argument, NULL, 'u' },
    { "byteswapped", no_argument, NULL, 'v' },
    { "big-endian", no_argument, NULL, 'z' },
    { "help", no_argument, NULL, 'h' },
    { 0 },
};

typedef enum {
    GOOD_ENDIAN,
    BAD_ENDIAN,
    UGLY_ENDIAN,
    UNKNOWN_ENDIAN,
} Endianness;

char* endiannessStrings[] = { "Big", "Little", "Middle", "Unknown" };

int main(int argc, char** argv) {
    int opt;
    bool csv = false;
    bool utf8 = false;
    bool endianSpecified = false;
    bool printEndian = false;
    FILE* romFile;
    N64Header header;
    size_t romSize;
    Endianness endianness;
    char separator = ',';

    if (argc < 2) {
        fprintf(stderr, "%s -cu[n|v|z] -s SEP ROMFILE\n", argv[0]);
        fprintf(stderr, "No ROM file provided. Exiting.\n");
        return 1;
    }

    while (true) {
        int optionIndex = 0;
        if ((opt = getopt_long(argc, argv, "s:cnpuvzh", longOptions, &optionIndex)) == EOF) {
            break;
        }

        switch (opt) {
            case 's':
                separator = *optarg;
                break;

            case 'c':
                csv = true;
                break;

            case 'n':
                endianSpecified = true;
                endianness = BAD_ENDIAN;
                break;

            case 'p':
                printEndian = true;
                break;

            case 'u':
                utf8 = true;
                break;

            case 'v':
                endianSpecified = true;
                endianness = UGLY_ENDIAN;
                break;

            case 'z':
                endianSpecified = true;
                endianness = GOOD_ENDIAN;
                break;

            case 'h':
                fprintf(stderr, "%s -cu[n|v|z] -s SEP ROMFILE\n", argv[0]);
                puts("Reads an N64 ROM header and prints the information it contains.");
                puts("Options:\n"
                     "  -s, --separator CHAR   Change the separator character used in CSV mode (default: ',')\n"
                     "\n"
                     "  -c, --csv              Output in csv format.\n"
                     "  -n, --little-endian    Read input as little-endian.\n"
                     "  -p, --print-endian     Print endianness.\n"
                     "  -u, --utf-8            Convert image name to UTF-8.\n"
                     "  -v, --byteswapped      Read input as byteswapped.\n"
                     "  -z, --big-endian       Read input as big-endian.\n"
                     "  -h, --help             Display this message and exit.\n");
                return 1;

            default:
                fprintf(stderr, "Getopt returned character code: 0x%X", opt);
        }
    }

    romFile = fopen(argv[optind], "rb");
    fread(&header, N64_HEADER_SIZE, 1, romFile);
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);

    /* Guess endianness from first byte */
    if (!endianSpecified) {
        switch (header.PIBSDDomain1Register[0]) {
            case 0x80:
                endianness = GOOD_ENDIAN;
                break;

            case 0x40:
                endianness = BAD_ENDIAN;
                break;

            case 0x37:
                endianness = UGLY_ENDIAN;
                break;

            default:
                endianness = UNKNOWN_ENDIAN;
                fprintf(stderr, "warning: unable to determine endianness from first byte of header: it is not one of "
                                "0x80, 0x37, 0x40.\n  Recommend investigating the raw bytes with a hexdump.");
                break;
        }
    }

    switch (endianness) {
        case GOOD_ENDIAN:
        case UNKNOWN_ENDIAN:
            break;
        case BAD_ENDIAN:
            SwapBytes32((uint32_t*)&header, sizeof(header));
            break;
        case UGLY_ENDIAN:
            SwapBytes16((uint16_t*)&header, sizeof(header));
            break;
    }

    ReEndHeader(&header);
    {
        char imageNameCopy[21] = { 0 };
        char imageNameUTF8[100] = { 0 };
        char* imageName = imageNameCopy;

        /* Copy the name to make sure it ends in '\0' */
        memcpy(imageNameCopy, header.imageName, sizeof(header.imageName));

        if (utf8) {
            iconv_t conv = iconv_open("UTF-8//TRANSLIT", "SHIFT-JIS");
            size_t inBytes = sizeof(imageNameCopy);
            size_t outBytes = sizeof(imageNameUTF8);
            char* inPtr = imageNameCopy;
            char* outPtr = imageNameUTF8;

            if (conv == (iconv_t)-1) {
                fprintf(stderr, "Conversion invalid\n");
                return 1;
            }

            if (iconv(conv, &inPtr, &inBytes, &outPtr, &outBytes) == (size_t)-1) {
                fprintf(stderr, "Conversion failed.\n");
                return 1;
            }

            imageName = imageNameUTF8;

            iconv_close(conv);
        }

        if (!csv) {
            printf("File: %s\n", argv[optind]);
            printf("ROM size: 0x%zX bytes (%zd MB)\n", romSize, romSize >> 20);
            if (printEndian) {
                printf("Endianness: %s\n", endiannessStrings[endianness]);
            }
            putchar('\n');

            printf("entrypoint:       %08X\n", header.entrypoint);
            printf("Libultra version: %c\n", header.revision & 0xFF);
            printf("CRC:              %08X %08X\n", header.checksum1, header.checksum2);
            printf("Image name:       \"%s\"\n", imageName);
            printf("Media format:     %c: %s\n", header.mediaFormat,
                   FindDescriptionFromChar(header.mediaFormat, mediaCharDescription));
            printf("Cartridge Id:     %c%c\n", header.cartridgeId[0], header.cartridgeId[1]);
            printf("Country code:     %c: %s\n", header.countryCode,
                   FindDescriptionFromChar(header.countryCode, countryCharDescription));
            printf("Version mask:     0x%X\n", header.version);
        } else {

            printf("%s", argv[optind]);
            putchar(separator);
            printf("0x%zX", romSize);
            putchar(separator);
            if (printEndian) {
                printf("%s", endiannessStrings[endianness]);
            }
            putchar(separator);

            printf("%08X", header.entrypoint);
            putchar(separator);
            printf("%c", header.revision & 0xFF);
            putchar(separator);
            printf("%08X %08X", header.checksum1, header.checksum2);
            putchar(separator);
            printf("\"%s\"", imageName);
            putchar(separator);
            printf("%c", header.mediaFormat);
            putchar(separator);
            printf("%c%c", header.cartridgeId[0], header.cartridgeId[1]);
            putchar(separator);
            printf("%c", header.countryCode);
            putchar(separator);
            printf("0x%X\n", header.version);
        }
    }

    fclose(romFile);
    return 0;
}
