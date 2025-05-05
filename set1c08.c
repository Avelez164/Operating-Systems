#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 16
#define MAX_LINES 200
#define MAX_BYTES 1024

// Converts hex to raw bytes
int hex_to_bytes(const char *hex, unsigned char *bytes)
{
    int len = strlen(hex) / 2;
    for (int i = 0; i < len; i++)
    {
        sscanf(hex + 2 * i, "%2hhx", &bytes[i]);
    }
    return len;
}

// Counts repeated 16-byte blocks in a byte array
int count_ecb_repetitions(unsigned char *data, int len)
{
    int blocks = len / BLOCK_SIZE;
    int count = 0;

    for (int i = 0; i < blocks; i++)
    {
        for (int j = i + 1; j < blocks; j++)
        {
            if (memcmp(data + i * BLOCK_SIZE, data + j * BLOCK_SIZE, BLOCK_SIZE) == 0)
            {
                count++;
            }
        }
    }
    return count;
}

int main()
{
    FILE *f = fopen("8.txt", "r");
    if (!f)
    {
        perror("fopen");
        return 1;
    }

    char line[2048];
    int line_num = 0;
    int max_repetitions = -1;
    int ecb_line = -1;

    while (fgets(line, sizeof(line), f))
    {
        unsigned char bytes[MAX_BYTES];
        int byte_len = hex_to_bytes(line, bytes);
        int reps = count_ecb_repetitions(bytes, byte_len);

        if (reps > max_repetitions)
        {
            max_repetitions = reps;
            ecb_line = line_num;
        }

        line_num++;
    }

    fclose(f);

    printf("ECB-encrypted ciphertext is at line %d with %d repeated blocks.\n", ecb_line, max_repetitions);
    return 0;
}
