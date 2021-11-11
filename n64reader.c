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
        data[i] = ((data[i] & 0xFF) << 0x18) | ((data[i] & 0xFF00) << 0x8) | ((data[i] & 0xFF0000) >> 0x8) | (data[i] >> 0x18);
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
    { "byteswapped", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { 0 },
};

typedef enum {
    GOOD_ENDIAN,
    BAD_ENDIAN,
    UGLY_ENDIAN,
} Endianness;

int main(int argc, char** argv) {
    int opt;
    bool csv = false;
    FILE* romFile;
    N64Header header;
    size_t romSize;
    Endianness endianness;

    if (argc < 2) {
        fprintf(stderr, "Please provide an N64 ROM file.\n");
        return 1;
    }

    while (true) {
        int optionIndex = 0;
        if ((opt = getopt_long(argc, argv, "cnvh", longOptions, &optionIndex)) == EOF) {
            break;
        }

        switch (opt) {
            case 'c':
                csv = true;
                break;
            case 'n':
                endianness = BAD_ENDIAN;
                break;
            case 'v':
                endianness = UGLY_ENDIAN;
                break;
            case 'h':
                puts("Reads an N64 ROM header and prints the information it contains.");
                puts("Options:\n"
                       "  -c, --csv              Output in csv format.\n"
                       "  -n, --little-endian    Read input as little-endian.\n"
                       "  -v, --byteswapped      Read input as byteswapped.\n");
                return 1;
        }
    }

    romFile = fopen(argv[optind], "rb");
    fread(&header, N64_HEADER_SIZE, 1, romFile);
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);


    switch (endianness) {
        case GOOD_ENDIAN:
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

        /* Copy the name to make sure it ends in '\0' */
        memcpy(imageNameCopy, header.imageName, sizeof(header.imageName));
        /* Convert to UTF-8 for printing */
        {
            iconv_t conv = iconv_open("UTF-8//TRANSLIT", "SHIFT-JIS");
            size_t inBytes = sizeof(imageNameCopy);
            size_t outBytes = sizeof(imageNameUTF8);
            char* inPtr = imageNameCopy;
            char* outPtr = imageNameUTF8;

            memset(imageNameUTF8, '\0', sizeof(imageNameUTF8));
            if (conv == (iconv_t)-1) {
                fprintf(stderr, "Conversion invalid\n");
                return 1;
            }

            if(iconv(conv, &inPtr, &inBytes, &outPtr, &outBytes) == (size_t)-1) {
                fprintf(stderr, "Conversion failed.\n");
                return 1;
            }

            printf("Original name: %s\n", imageNameCopy);
            printf("Converted name: %s\n", imageNameUTF8);

            iconv_close(conv);
        }


        if (!csv) {
            printf("File: %s\n", argv[optind]);
            printf("ROM size: 0x%zX bytes (%zd MB)\n\n", romSize, romSize >> 20);
            
            printf("entrypoint:       %08X\n", header.entrypoint);
            printf("Libultra version: %c\n", header.revision & 0xFF);
            printf("CRC:              %08X %08X\n", header.checksum1, header.checksum2);
            printf("Image name:       \"%s\"\n", imageNameCopy);
            printf("Media format:     %c: %s\n", header.mediaFormat,
                   FindDescriptionFromChar(header.mediaFormat, mediaCharDescription));
            printf("Cartridge Id:     %c%c\n", header.cartridgeId[0], header.cartridgeId[1]);
            printf("Country code:     %c: %s\n", header.countryCode,
                   FindDescriptionFromChar(header.countryCode, countryCharDescription));
            printf("Version mask:     0x%X\n", header.version);
        } else {
            
            printf("%s,", argv[optind]);
            printf("0x%zX,", romSize);
            
            printf("%08X,", header.entrypoint);
            printf("%c,", header.revision & 0xFF);
            printf("%08X %08X,", header.checksum1, header.checksum2);
            printf("\"%s\",", imageNameCopy);
            printf("%c,", header.mediaFormat);
            printf("%c%c,", header.cartridgeId[0], header.cartridgeId[1]);
            printf("%c,", header.countryCode);
            printf("0x%X\n", header.version);

        }
    }

    fclose(romFile);
    return 0;
}
