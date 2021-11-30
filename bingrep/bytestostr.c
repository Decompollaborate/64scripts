#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t DigitFromChar(char ch) {
    switch (ch) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return ch - '0';

        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return ch - 'A' + 0xA;

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            return ch - 'a' + 0xA;

        default:
            return -1;
    }
}

/**
 * Converts the hex digits in a string into a preallocated byte array.
 *
 * Returns number of bytes written.
 */
int BytesFromString(uint8_t* byteArray, const char* string) {

    if (string != NULL) {
        int len = 0;
        int parity = 0;
        int inIndex;
        int outIndex = 0;

        for (inIndex = 0; string[inIndex] != '\0'; inIndex++) {
            len++;
            if (isxdigit(string[inIndex])) {
                parity++;
                parity &= 1;
            } else {
                fprintf(stderr, "Found nonhexadecimal character '%c', skipping.\n", string[inIndex]);
            }
        }
        if (parity & 1) {
            printf("Input \"%s\" has an odd number of nybbles, padding with a leading zero.", string);
        }
        for (inIndex = 0; inIndex < len; inIndex++) {
            if (isxdigit(string[inIndex])) {
                uint8_t digit = DigitFromChar(string[inIndex]);
                if ((outIndex + parity) & 1) {
                    byteArray[(outIndex + parity) / 2] += digit;
                } else {
                    byteArray[(outIndex + parity) / 2] = digit << 4;
                }
                outIndex++;
            }
        }
        byteArray[(outIndex + parity) / 2] = '\0';
        return (outIndex + parity) / 2;
    }

    return -1;
}

int main(int argc, char** argv) {
    char* bytes;
    int length;
    char* outString;
    char* ptr;

    if (argc < 2) {
        printf("Usage: %s BYTES\n", argv[0]);
        return 1;
    }

    bytes = argv[1];
    length = strlen(bytes);
    ptr = outString = malloc(length + 1);

    BytesFromString(outString, bytes);

    while (*ptr != '\0') {
        printf("%c", *ptr);
        ptr++;
    }
    puts("");

    return 0;
}
