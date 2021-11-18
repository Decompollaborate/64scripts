#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Size of bad character table, needs a value for every character */
#define ASIZE 0x100

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

// uint8_t DigitFromChar(char ch) {
//     if ('0' <= ch <= '9') {
//         return ch - '0';
//     }
//     if ('A' <= ch <= 'F') {
//         return ch - 'A' + 0xA;
//     }
//     if ('a' <= ch <= 'f') {
//         return ch - 'a' + 0xA;
//     }
//     return -1;
// }

/** 
 * Converts the hex digits in a string into a preallocated byte array.
 * 
 * Returns number of bytes written.
 */
int BytesFromString( uint8_t* byteArray, const char* string) {
    
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
                if (outIndex + parity & 1) {
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

#define SETAF_RED "\x1b[31m"
#define SGR0 "\x1b[0m"
#define MIN(a, b) (((a) < (b) ) ? (a) : (b))
#define MAX(a, b) (((a) > (b) ) ? (a) : (b))

#define LOOK_FORWARD 2
#define LOOK_BACKWARD 2

/* Output function */
void OUTPUT(int j, int m, uint8_t* y, int n) {
    int k;

    /* Offset */
    printf("[%06X]:  ", j);
    for (k = MAX(0, j - LOOK_BACKWARD); k < j; k++) {
        printf("%02X ", y[k]);
    }

    /* Matched string */
    printf(SETAF_RED);
    for (k = j; k < j + m; k++) {
        printf("%02X ", y[k]);
    }
    printf(SGR0);

    for (k = j + m; k < MIN(j + m + LOOK_FORWARD, n); k++) {
        printf("%02X ", y[k]);
    }
    putchar('\n');
}

/**
 * See https://www-igm.univ-mlv.fr/~lecroq/string/node19.html
 * 
 * x is search string
 * m is length of x
 * qsBc the "bad character table" to generate
 */
void preQsBc(uint8_t* x, int m, int qsBc[]) {
    int i;

    for (i = 0; i < ASIZE; ++i) {
        qsBc[i] = m + 1;
    }
    for (i = 0; i < m; ++i) {
        qsBc[x[i]] = m - i;
    }
}

/**
 * QuickSearch implementation
 * 
 * x is search string
 * m is length of x
 * y is buffer to search
 * n is length of y
 */
void QS(uint8_t* x, int m, uint8_t* y, int n) {
    int j; 
    int qsBc[ASIZE];

    /* Preprocessing */
    preQsBc(x, m, qsBc);

    /* Searching */
    j = 0;
    while (j <= n - m) {
        // printf("%d: %d\n", j, memcmp(x, y + j, m));
        if (memcmp(x, y + j, m) == 0) {
            OUTPUT(j, m, y, n);
        }
        j += qsBc[y[j + m]]; /* shift */
    }
}

void BruteForceSearch(uint8_t* x, int m, uint8_t* y, int n) {
    int j = 0;
    while (j <= n - m) {
        if (memcmp(x, y + j, m) == 0) {
            OUTPUT(j, m, y, n);
        }
        j++;
    }
}

int main(int argc, char** argv) {
    FILE* inputFile;
    uint8_t* fileBuffer;
    size_t fileLength;
    uint8_t* search;
    int searchLength;

    if (argc < 3) {
        printf("Usage: %s PATTERN FILE", argv[0]);
    }

    searchLength = strlen(argv[1] + 1 );
    search = malloc(searchLength);
    // memcpy(search, argv[1], searchLength);
    searchLength = BytesFromString(search, argv[1]);

    if ((inputFile = fopen(argv[2], "rb")) == NULL) {
        fprintf(stderr,"Failed to open file %s", argv[2]);
    }

    {
        int i;
        printf("%s\n", argv[1]);
        for (i = 0; i < searchLength; i++) {
            printf("%02X ", search[i]);
        }
        putchar('\n');
    }

    fseek(inputFile, 0, SEEK_END);
    fileLength = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);
    fileBuffer = malloc(fileLength);
    fread(fileBuffer, fileLength, 1, inputFile);

    QS(search, searchLength, fileBuffer, fileLength);
    puts("=======");
    BruteForceSearch(search, searchLength, fileBuffer, fileLength);



    fclose(inputFile);
    free(search);
    free(fileBuffer);
    return 0;
}
