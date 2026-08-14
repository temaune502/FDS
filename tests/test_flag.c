#define FDS_IMPLEMENTATION
#define FLAG_PARSER_IMPLEMENTATION
#include "flag_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FlagSet *fs = flagset_new();

    char *input_path = NULL;
    char *output_path = NULL;
    bool verbose = false;

    flagset_string(fs, &input_path, "i", "", "input file path");
    flagset_required(fs);       // -i обов'язковий

    flagset_string(fs, &output_path, "o", "", "output file path");
    flagset_required(fs);       // -o обов'язковий

    flagset_bool(fs, &verbose, "verbose", false, "print file content to console");
    flagset_bool(fs, &verbose, "v", false, "print file content (short)");

    // Парсинг (автоматично перевірить обов'язкові)
    flagset_parse(fs, argc, argv);

    // Відкриваємо вхідний файл
    FILE *fin = fopen(input_path, "r");
    if (!fin) {
        perror("fopen input");
        flagset_free(fs);   // тут input_path ще живий, можна викликати
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (fsize < 0) {
        fprintf(stderr, "Error: cannot get file size\n");
        fclose(fin);
        flagset_free(fs);
        return 1;
    }

    char *buffer = (char*)malloc(fsize + 1);
    if (!buffer) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(fin);
        flagset_free(fs);
        return 1;
    }
    size_t nread = fread(buffer, 1, fsize, fin);
    if (nread != (size_t)fsize && ferror(fin)) {
        fprintf(stderr, "Error: read failed\n");
        free(buffer);
        fclose(fin);
        flagset_free(fs);
        return 1;
    }
    buffer[nread] = '\0';
    fclose(fin);

    FILE *fout = fopen(output_path, "w");
    if (!fout) {
        perror("fopen output");
        free(buffer);
        flagset_free(fs);
        return 1;
    }
    fwrite(buffer, 1, nread, fout);
    fclose(fout);

    if (verbose) {
        printf("--- File content ---\n%s\n--------------------\n", buffer);
    }

    // Використовуємо input_path / output_path тут,
    // поки вони ще не звільнені
    printf("Successfully copied %zu bytes from %s to %s\n",
           nread, input_path, output_path);
    
    output_path = NULL;
    input_path = NULL;
    

    free(buffer);
    buffer = NULL;
    // Тільки тепер звільняємо fs, коли input_path/output_path більше не потрібні
    flagset_free(fs);

    return 0;
}