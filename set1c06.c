#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// === Base64 decoding ===
int b64_value(char c)
{
    if ('A' <= c && c <= 'Z')
        return c - 'A';
    if ('a' <= c && c <= 'z')
        return c - 'a' + 26;
    if ('0' <= c && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

unsigned char *base64_decode(const char *input, size_t *out_len)
{
    size_t len = strlen(input);
    unsigned char *output = malloc(len * 3 / 4);
    int val = 0, valb = -8;
    size_t out_i = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (isspace(input[i]))
            continue;
        int d = b64_value(input[i]);
        if (d == -1)
            break;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0)
        {
            output[out_i++] = (val >> valb) & 0xFF;
            valb -= 8;
        }
    }
    *out_len = out_i;
    return output;
}

// === Hamming distance ===
int hamming_distance(const unsigned char *a, const unsigned char *b, size_t len)
{
    int dist = 0;
    for (size_t i = 0; i < len; i++)
    {
        unsigned char xor = a[i] ^ b[i];
        while (xor)
        {
            dist += xor & 1;
            xor >>= 1;
        }
    }
    return dist;
}

// === Score for English-likeness ===
float score_english(const unsigned char *text, size_t len)
{
    float score = 0.0;
    for (size_t i = 0; i < len; i++)
    {
        char c = text[i];
        if (isalpha(c))
            score += 1.0;
        else if (c == ' ')
            score += 2.0;
        else if (c == 'e' || c == 'E')
            score += 2.5;
        else if (!isprint(c))
            score -= 5.0;
    }
    return score;
}

// === Single-byte XOR brute force ===
unsigned char find_single_xor_key(const unsigned char *block, size_t len)
{
    float best_score = -INFINITY;
    unsigned char best_key = 0;
    unsigned char *decrypted = malloc(len + 1);

    for (int k = 0; k <= 255; k++)
    {
        for (size_t i = 0; i < len; i++)
        {
            decrypted[i] = block[i] ^ k;
        }
        decrypted[len] = '\0';
        float s = score_english(decrypted, len);
        if (s > best_score)
        {
            best_score = s;
            best_key = k;
        }
    }

    free(decrypted);
    return best_key;
}

// === Repeating-key XOR ===
void repeating_key_xor(const unsigned char *input, size_t len, const unsigned char *key, size_t keylen, unsigned char *output)
{
    for (size_t i = 0; i < len; i++)
    {
        output[i] = input[i] ^ key[i % keylen];
    }
}

// === Main ===
int main()
{
    FILE *f = fopen("6.txt", "r");
    if (!f)
    {
        perror("fopen");
        return 1;
    }

    char *base64_data = malloc(8192);
    base64_data[0] = '\0';

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        strcat(base64_data, line);
    }
    fclose(f);

    size_t data_len;
    unsigned char *cipher = base64_decode(base64_data, &data_len);
    free(base64_data);

    // Validate hamming distance
    if (hamming_distance((unsigned char *)"this is a test", (unsigned char *)"wokka wokka!!!", 14) != 37)
    {
        fprintf(stderr, "Hamming test failed.\n");
        return 1;
    }

    // Guess keysize
    int best_keysize = 0;
    float best_distance = INFINITY;

    for (int keysize = 2; keysize <= 40; keysize++)
    {
        float total_dist = 0.0;
        int blocks = 4;
        //
        int pairs = 0;
        for (int i = 0; i < blocks - 1; i++)
        {
            for (int j = i + 1; j < blocks; j++)
            {
                const unsigned char *block1 = cipher + i * keysize;
                const unsigned char *block2 = cipher + j * keysize;
                if ((i + 1) * keysize > data_len || (j + 1) * keysize > data_len)
                    continue;
                total_dist += hamming_distance(block1, block2, keysize);
                pairs++;
            }
        }
        if (pairs > 0)
        {
            float normalized = total_dist / (pairs * keysize);
            if (normalized < best_distance)
            {
                best_distance = normalized;
                best_keysize = keysize;
            }
        }
        //

        float normalized = total_dist / ((blocks - 1) * keysize);
        if (normalized < best_distance)
        {
            best_distance = normalized;
            best_keysize = keysize;
        }
    }

    printf("Best guessed keysize: %d\n", best_keysize);

    // Recover key
    unsigned char key[64];
    for (int i = 0; i < best_keysize; i++)
    {
        unsigned char block[4096];
        size_t block_len = 0;

        for (size_t j = i; j < data_len; j += best_keysize)
        {
            block[block_len++] = cipher[j];
        }

        key[i] = find_single_xor_key(block, block_len);
    }

    printf("Recovered key: ");
    for (int i = 0; i < best_keysize; i++)
    {
        printf("%c", key[i]);
    }
    printf("\n");

    // Decrypt
    unsigned char *plaintext = malloc(data_len + 1);
    repeating_key_xor(cipher, data_len, key, best_keysize, plaintext);
    plaintext[data_len] = '\0';

    printf("---- Decrypted Output ----\n");
    printf("%s\n", plaintext);

    free(cipher);
    free(plaintext);
    return 0;
}
